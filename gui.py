import socket
import threading
import tkinter as tk
from tkinter import scrolledtext, messagebox

class ClientGUI:
    def __init__(self, master):
        self.master = master
        self.master.title("Gra Słowna - Klient")

        tk.Label(master, text="IP serwera:").grid(row=0, column=0)
        tk.Label(master, text="Port:").grid(row=0, column=2)

        self.ip_entry = tk.Entry(master)
        self.ip_entry.grid(row=0, column=1)
        self.port_entry = tk.Entry(master)
        self.port_entry.grid(row=0, column=3)

        self.connect_button = tk.Button(master, text="Połącz", command=self.connect)
        self.connect_button.grid(row=0, column=4)

        tk.Label(master, text="Nick:").grid(row=1, column=0)
        self.nick_entry = tk.Entry(master)
        self.nick_entry.grid(row=1, column=1)

        self.register_button = tk.Button(master, text="Zarejestruj", command=self.register)
        self.register_button.grid(row=1, column=2)

        tk.Label(master, text="Litery:").grid(row=2, column=0)
        self.letters_var = tk.StringVar()
        self.letters_label = tk.Label(master, textvariable=self.letters_var, font=("Helvetica", 16))
        self.letters_label.grid(row=2, column=1, columnspan=4, sticky="w")

        self.start_button = tk.Button(master, text="START", command=self.start_round)
        self.start_button.grid(row=3, column=0)

        tk.Label(master, text="Twoje słowo:").grid(row=4, column=0)
        self.word_entry = tk.Entry(master)
        self.word_entry.grid(row=4, column=1)
        self.send_word_button = tk.Button(master, text="Wyślij", command=self.send_word)
        self.send_word_button.grid(row=4, column=2)

        tk.Label(master, text="Komunikaty / Ranking:").grid(row=5, column=0, columnspan=5)
        self.messages = scrolledtext.ScrolledText(master, width=50, height=20, state='disabled')
        self.messages.grid(row=6, column=0, columnspan=5)

        self.sock = None
        self.running = False

    def connect(self):
        ip = self.ip_entry.get()
        port = int(self.port_entry.get())
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((ip, port))
            self.running = True
            threading.Thread(target=self.receive_messages, daemon=True).start()
            self.add_message("Połączono z serwerem")
        except Exception as e:
            messagebox.showerror("Błąd", f"Nie można połączyć: {e}")

    def register(self):
        nick = self.nick_entry.get()
        if self.sock and nick:
            self.sock.sendall(f"register {nick}\n".encode())

    def start_round(self):
        if self.sock:
            self.sock.sendall(b"start\n")

    def send_word(self):
        word = self.word_entry.get()
        if self.sock and word:
            self.sock.sendall((word + "\n").encode())
            self.word_entry.delete(0, tk.END)

    def receive_messages(self):
        while self.running:
            try:
                data = self.sock.recv(4096)
                if not data:
                    self.add_message("Rozłączono z serwerem")
                    self.running = False
                    break

                messages = data.decode().split('\n')
                for msg in messages:
                    if msg.startswith("LETTERS "):
                        self.letters_var.set(msg[8:])
                    else:
                        self.add_message(msg)
            except Exception as e:
                self.add_message(f"Błąd: {e}")
                self.running = False
                break

    def add_message(self, msg):
        self.messages.config(state='normal')
        self.messages.insert(tk.END, msg + "\n")
        self.messages.see(tk.END)
        self.messages.config(state='disabled')

if __name__ == "__main__":
    root = tk.Tk()
    gui = ClientGUI(root)
    root.mainloop()
