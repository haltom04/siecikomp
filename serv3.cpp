#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <cstdlib>
#include <ctime>
#include <set>

enum class State { LOBBY, WAITING, IN_ROUND };

struct Client {
    int fd;             
    char buffer[2048];
    int buffer_len; 
    char nick[64];
    bool registered;
    State state;     
    char lastWord[128];
    int score;
};

int countLobbyPlayers(const std::vector<Client> &clients) {
    int count = 0;
    for (auto &c : clients) {
        if (c.registered && c.state == State::LOBBY) count++;
    }
    return count;
}

std::vector<char*> dictionaryDynamic;

void loadDictionary(const std::string &filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Nie można otworzyć słownika: " << filename << "\n";
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        char* w = new char[line.size() + 1];
        std::strcpy(w, line.c_str());
        dictionaryDynamic.push_back(w);
    }
    dictionaryDynamic.push_back(nullptr);
    file.close();
}

bool isWordInDictionary(const char* word) {
    for (size_t i = 0; dictionaryDynamic[i] != nullptr; i++) {
        if (strcmp(dictionaryDynamic[i], word) == 0) return true;
    }
    return false;
}

void freeDictionary() {
    for (size_t i = 0; i < dictionaryDynamic.size(); i++) {
        delete[] dictionaryDynamic[i];
    }
}



std::string generateLetters(int n) {
    std::string result;
    const char* letterPool =
        "aaaaaaa" // a
        "b"       // b
        "cc"      // c
        "d"       // d
        "eeeeee"  // e
        "f"       // f
        "g"       // g
        "h"       // h
        "i"       // i
        "j"       // j
        "k"       // k
        "lll"     // l
        "m"       // m
        "nnn"     // n
        "oooo"    // o
        "p"       // p
        "r"       // r
        "sss"     // s
        "t"       // t
        "u"       // u
        "w"       // w
        "y"       // y
        "zzz";    // z

    int poolSize = strlen(letterPool);
    for (int i = 0; i < n; i++) {
        char r = letterPool[rand() % poolSize];
        result.push_back(r);
    }
    return result;
}

bool canBuildFromLetters(const char* word, const std::string &letters) {
    std::multiset<char> letterSet;
    for (char c : letters) letterSet.insert(c);

    for (size_t i = 0; word[i]; i++) {
        auto it = letterSet.find(word[i]);
        if (it == letterSet.end()) return false;
        letterSet.erase(it);
    }
    return true;
}


void endRound(std::vector<Client> &clients, std::set<std::string> &usedWords, std::string &current_letters) {
    printf("Runda zakończona!\n");

    std::string summary = "PODSUMOWANIE RUNDY:\n";
    summary += "Użyte słowa: ";
    for (const auto &w : usedWords) summary += w + " ";
    summary += "\nWyniki:\n";

    for (auto &c : clients) {
        if (c.registered && c.state == State::IN_ROUND) {
            c.state = State::LOBBY;
            summary += std::string(c.nick) + ": " + std::to_string(c.score) + "\n";
        }
    }

    for (auto &c : clients) {
        if (c.registered) write(c.fd, summary.c_str(), summary.size());
    }

    usedWords.clear();
    current_letters = "";
}


int main(){
    srand(time(nullptr));
    loadDictionary("out.txt");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Nie mogę stworzyć socketu\n");
        return 1;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);    
    addr.sin_addr.s_addr = INADDR_ANY; 

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("BIND ERROR");
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("Serwer nasłuchuje..\n");

    std::vector<pollfd> fds;
    fds.push_back({server_fd, POLLIN, 0}); 
    std::vector<Client> clients; 

    std::set<std::string> usedWords;
    std::string current_letters;
    time_t roundStartTime = 0;
    const int ROUND_DURATION = 30;
    bool roundActive = false;

    while (true) {
        poll(fds.data(), fds.size(), 1000); 

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd > 0) {
                printf("Nowy klient!\n");
                fds.push_back({client_fd, POLLIN, 0});
                
                Client c;
                c.fd = client_fd;
                c.buffer_len = 0;
                c.registered = false;
                c.state = State::LOBBY; 
                c.score = 0;
                clients.push_back(c);
            }
        }

        if (roundActive) {
            time_t now = time(nullptr);
            if (difftime(now, roundStartTime) >= ROUND_DURATION) {
                endRound(clients, usedWords, current_letters);
                roundActive = false;
            }
        }

        for (size_t i = 1; i < fds.size(); i++){
            if (fds[i].revents & POLLIN){ 
                Client &client = clients[i-1];
                ssize_t bytes = read(client.fd, client.buffer + client.buffer_len, sizeof(client.buffer) - client.buffer_len - 1);
                if (bytes <= 0) { 
                    printf("Klient rozłączony\n");
                    close(client.fd);
                    fds.erase(fds.begin() + i);
                    clients.erase(clients.begin() + i-1);
                    i--;
                    continue;
                }
                client.buffer_len += bytes;
                client.buffer[client.buffer_len] = '\0';

                char* nl;
                while ((nl = strchr(client.buffer, '\n')) != NULL) {
                    *nl = '\0'; 
                    printf("Odebrano: %s\n", client.buffer);

                    if (!client.registered) {
                        if (strncmp(client.buffer, "register ", 9) == 0) {
                            const char* nick = client.buffer + 9;
                            bool exists = false;
                            for (auto &c : clients) {
                                if (c.registered && strcmp(c.nick, nick) == 0) exists = true;
                            }
                            if (exists) {
                                const char* msg = "Nick zajęty\n";
                                write(client.fd, msg, strlen(msg));
                            } else {
                                strncpy(client.nick, nick, sizeof(client.nick)-1);
                                client.registered = true;
                                const char* msg = "Nick zarejestrowany\n";
                                write(client.fd, msg, strlen(msg));
                            }
                        } else {
                            const char* msg = "Musisz się zarejestrować najpierw\n";
                            write(client.fd, msg, strlen(msg));
                        }

                        int consumed = (nl - client.buffer) + 1;
                        memmove(client.buffer, client.buffer + consumed, client.buffer_len - consumed);
                        client.buffer_len -= consumed;
                        client.buffer[client.buffer_len] = '\0';
                        continue;
                    }

                    if (strcmp(client.buffer, "ranking") == 0) {
                        char line[256];
                        for (auto &c : clients) {
                            if (c.registered) {
                                snprintf(line, sizeof(line), "%s: %d\n", c.nick, c.score);
                                write(client.fd, line, strlen(line));
                            }
                        }
                    }
               
                    else if (strncmp(client.buffer, "start", 5) == 0) {
                        if (roundActive) {
                            const char* msg = "Runda już trwa\n";
                            write(client.fd, msg, strlen(msg));
                            continue;
                        }
                        if (countLobbyPlayers(clients) >= 2) {
                            for (auto &c : clients) {
                                if (c.state == State::LOBBY) c.score = 0;
                            }

                            printf("Runda startuje!\n");
                            current_letters = generateLetters(15); 
                            printf("Wylosowane litery: %s\n", current_letters.c_str());

                            for (auto &c : clients) {
                                if (c.state == State::LOBBY) {
                                    c.state = State::IN_ROUND;
                                    const char* msg1 = "ROUND_START\n";
                                    write(c.fd, msg1, strlen(msg1));

                                    std::string msg2 = "LETTERS ";
                                    msg2 += current_letters;
                                    msg2 += "\n";
                                    write(c.fd, msg2.c_str(), msg2.size());
                                }
                            }

                            roundStartTime = time(nullptr);
                            roundActive = true;
                        } else {
                            const char* msg = "Za mało graczy w lobby\n";
                            write(client.fd, msg, strlen(msg));
                        }
                    }
 
                    else if (client.state == State::IN_ROUND) {
                        const char* word = client.buffer;

                        if (usedWords.count(word)) {
                            const char* msg = "WORD_USED\n";
                            write(client.fd, msg, strlen(msg));
                        }
                        else if (!canBuildFromLetters(word, current_letters)) {
                            const char* msg = "WORD_BAD\n";
                            write(client.fd, msg, strlen(msg));
                        }
                        else if (!isWordInDictionary(word)) {
                            const char* msg = "WORD_BAD\n";
                            write(client.fd, msg, strlen(msg));
                        }
                        else {
                            usedWords.insert(word);
                            int points = strlen(word);
                            client.score += points;

                            std::string info = "WORD_ACCEPTED ";
                            info += client.nick;
                            info += " ";
                            info += word;
                            info += " +";
                            info += std::to_string(points);
                            info += "\n";

                            for (auto &c : clients) {
                                if (c.registered && c.state == State::IN_ROUND) {
                                    write(c.fd, info.c_str(), info.size());
                                }
                            }

                            std::string ranking = "RANKING:\n";
                            for (auto &c : clients) {
                                if (c.registered && c.state == State::IN_ROUND) {
                                    ranking += std::string(c.nick) + ": " + std::to_string(c.score) + "\n";
                                }
                            }

                            for (auto &c : clients) {
                                if (c.registered && c.state == State::IN_ROUND) {
                                    write(c.fd, ranking.c_str(), ranking.size());
                                }
                            }

                            printf("%s zdobywa %d punktów za \"%s\" (łącznie %d)\n",
                                client.nick, points, word, client.score);
                        }
                    }

                    int consumed = (nl - client.buffer) + 1;
                    memmove(client.buffer, client.buffer + consumed, client.buffer_len - consumed);
                    client.buffer_len -= consumed;
                    client.buffer[client.buffer_len] = '\0';
                }
            }
        }
    }

    freeDictionary();
    return 0;
}
