import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import queue

class DispenserInterface:
    def __init__(self, root):
        self.root = root
        self.root.title("Controlador de Dispenser de Envelopes")
        self.root.geometry("600x500")
        
        # Variáveis de controle
        self.serial_connection = None
        self.is_connected = False
        self.data_queue = queue.Queue()
        
        # Configurar interface
        self.setup_interface()
        
        # Iniciar thread para leitura serial
        self.serial_thread = None
        self.running = True
        
        # Tentar conectar automaticamente na COM5
        self.auto_connect()
        
        # Iniciar processamento de dados
        self.process_serial_data()

    def setup_interface(self):
        # Frame de conexão
        connection_frame = ttk.LabelFrame(self.root, text="Conexão Serial", padding="10")
        connection_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(connection_frame, text="Porta COM:").grid(row=0, column=0, padx=5)
        self.com_port = tk.StringVar(value="COM5")
        ttk.Entry(connection_frame, textvariable=self.com_port, width=10).grid(row=0, column=1, padx=5)
        
        self.connect_btn = ttk.Button(connection_frame, text="Conectar", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=2, padx=5)
        
        ttk.Button(connection_frame, text="Listar Portas", command=self.list_ports).grid(row=0, column=3, padx=5)
        
        self.status_label = ttk.Label(connection_frame, text="Desconectado", foreground="red")
        self.status_label.grid(row=0, column=4, padx=10)
        
        # Frame de controle
        control_frame = ttk.LabelFrame(self.root, text="Controles", padding="10")
        control_frame.pack(fill="x", padx=10, pady=5)
        
        self.dispense_btn = ttk.Button(control_frame, text="Dispensar Envelope", 
                                      command=self.dispense_envelope, state="disabled")
        self.dispense_btn.pack(side="left", padx=5)
        
        self.status_btn = ttk.Button(control_frame, text="Ver Status", 
                                    command=self.request_status, state="disabled")
        self.status_btn.pack(side="left", padx=5)
        
        self.reset_btn = ttk.Button(control_frame, text="Reset Sistema", 
                                   command=self.reset_system, state="disabled")
        self.reset_btn.pack(side="left", padx=5)
        
        # Frame de status do sistema
        status_frame = ttk.LabelFrame(self.root, text="Status do Sistema", padding="10")
        status_frame.pack(fill="x", padx=10, pady=5)
        
        # Grid para informações de status
        ttk.Label(status_frame, text="Estado:").grid(row=0, column=0, sticky="w")
        self.state_label = ttk.Label(status_frame, text="Desconectado", foreground="red")
        self.state_label.grid(row=0, column=1, sticky="w", padx=5)
        
        ttk.Label(status_frame, text="Estoque:").grid(row=1, column=0, sticky="w")
        self.stock_label = ttk.Label(status_frame, text="---")
        self.stock_label.grid(row=1, column=1, sticky="w", padx=5)
        
        ttk.Label(status_frame, text="Saída:").grid(row=2, column=0, sticky="w")
        self.output_label = ttk.Label(status_frame, text="---")
        self.output_label.grid(row=2, column=1, sticky="w", padx=5)
        
        ttk.Label(status_frame, text="Posição:").grid(row=3, column=0, sticky="w")
        self.position_label = ttk.Label(status_frame, text="---")
        self.position_label.grid(row=3, column=1, sticky="w", padx=5)
        
        # Frame do log
        log_frame = ttk.LabelFrame(self.root, text="Log do Sistema", padding="10")
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, height=15, width=70)
        self.log_text.pack(fill="both", expand=True)
        self.log_text.config(state="disabled")

    def auto_connect(self):
        """Tenta conectar automaticamente na COM5"""
        self.log("Tentando conexão automática com COM5...")
        self.connect_serial()

    def list_ports(self):
        """Lista todas as portas seriais disponíveis"""
        ports = serial.tools.list_ports.comports()
        port_list = [f"{port.device} - {port.description}" for port in ports]
        if port_list:
            message = "Portas disponíveis:\n" + "\n".join(port_list)
        else:
            message = "Nenhuma porta serial encontrada!"
        messagebox.showinfo("Portas Seriais", message)

    def toggle_connection(self):
        """Conecta ou desconecta da porta serial"""
        if not self.is_connected:
            self.connect_serial()
        else:
            self.disconnect_serial()

    def connect_serial(self):
        """Estabelece conexão serial"""
        try:
            port = self.com_port.get()
            self.serial_connection = serial.Serial(
                port=port,
                baudrate=9600,
                timeout=1,
                write_timeout=1
            )
            
            # Aguardar Arduino inicializar
            time.sleep(2)
            
            # Limpar buffer serial
            self.serial_connection.reset_input_buffer()
            
            self.is_connected = True
            self.status_label.config(text="Conectado", foreground="green")
            self.state_label.config(text="Conectando...", foreground="blue")
            self.connect_btn.config(text="Desconectar")
            
            # Ativar botões de controle
            self.dispense_btn.config(state="normal")
            self.status_btn.config(state="normal")
            self.reset_btn.config(state="normal")
            
            self.log(f"Conectado com sucesso à {port}")
            
            # Iniciar thread de leitura serial
            self.serial_thread = threading.Thread(target=self.read_serial_data, daemon=True)
            self.serial_thread.start()
            
            # Solicitar status inicial
            self.request_status()
            
        except Exception as e:
            self.log(f"ERRO na conexão: {str(e)}")
            messagebox.showerror("Erro de Conexão", f"Não foi possível conectar em {self.com_port.get()}\n\nErro: {str(e)}")

    def disconnect_serial(self):
        """Fecha a conexão serial"""
        self.is_connected = False
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
        
        self.status_label.config(text="Desconectado", foreground="red")
        self.state_label.config(text="Desconectado", foreground="red")
        self.connect_btn.config(text="Conectar")
        
        # Desativar botões de controle
        self.dispense_btn.config(state="disabled")
        self.status_btn.config(state="disabled")
        self.reset_btn.config(state="disabled")
        
        # Resetar labels de status
        self.stock_label.config(text="---")
        self.output_label.config(text="---")
        self.position_label.config(text="---")
        
        self.log("Conexão serial fechada")

    def read_serial_data(self):
        """Thread para leitura contínua dos dados seriais"""
        while self.is_connected and self.running:
            try:
                if self.serial_connection and self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.data_queue.put(line)
                time.sleep(0.1)
            except Exception as e:
                if self.is_connected:  # Só loga erro se ainda deveria estar conectado
                    self.data_queue.put(f"ERRO_LEITURA: {str(e)}")
                break

    def process_serial_data(self):
        """Processa dados da fila e atualiza a interface"""
        try:
            while not self.data_queue.empty():
                data = self.data_queue.get_nowait()
                self.update_interface(data)
        except queue.Empty:
            pass
        
        # Agendar próxima verificação
        self.root.after(100, self.process_serial_data)

    def update_interface(self, data):
        """Atualiza a interface com dados recebidos"""
        self.log(data)
        
        data_upper = data.upper()
        
        # Atualiza status baseado nas mensagens recebidas
        if "ESTOQUE: VAZIO" in data_upper:
            self.stock_label.config(text="VAZIO", foreground="red")
        elif "ESTOQUE: COM ENVELOPES" in data_upper:
            self.stock_label.config(text="COM ENVELOPES", foreground="green")
            
        if "SAÍDA: OCUPADA" in data_upper:
            self.output_label.config(text="OCUPADA", foreground="orange")
        elif "SAÍDA: LIVRE" in data_upper:
            self.output_label.config(text="LIVRE", foreground="green")
            
        if "POSIÇÃO: ENVELOPE PRESENTE" in data_upper:
            self.position_label.config(text="PRESENTE", foreground="blue")
        elif "POSIÇÃO: LIVRE" in data_upper:
            self.position_label.config(text="LIVRE", foreground="green")
            
        # CRÍTICO: Atualiza estado do sistema e controle do botão Dispensar
        if "REPOUSO - PRONTO PARA USO" in data_upper:
            self.state_label.config(text="PRONTO", foreground="green")
            self.dispense_btn.config(state="normal")  # HABILITA o botão
            self.log("Sistema pronto para dispensar")
            
        elif "AGUARDANDO RETIRADA" in data_upper:
            self.state_label.config(text="AGUARDANDO RETIRADA", foreground="orange")
            self.dispense_btn.config(state="disabled")  # DESABILITA o botão
            
        elif "ERRO - ESTOQUE VAZIO" in data_upper:
            self.state_label.config(text="ESTOQUE VAZIO", foreground="red")
            self.dispense_btn.config(state="disabled")
            
        elif "ERRO - SAÍDA OCUPADA" in data_upper:
            self.state_label.config(text="SAÍDA OCUPADA", foreground="red")
            self.dispense_btn.config(state="disabled")
            
        elif "EJETANDO ENVELOPE" in data_upper or "AVANÇANDO ENVELOPE" in data_upper or "LEVANDO ENVELOPE" in data_upper:
            self.state_label.config(text="EM OPERAÇÃO", foreground="blue")
            self.dispense_btn.config(state="disabled")
            
        elif "VERIFICANDO CONDIÇÕES" in data_upper:
            self.state_label.config(text="VERIFICANDO", foreground="blue")
            self.dispense_btn.config(state="disabled")

        # Detecta quando o envelope foi retirado e sistema voltou ao normal
        if "ENVELOPE RETIRADO" in data_upper and "SISTEMA PRONTO" in data_upper:
            self.state_label.config(text="PRONTO", foreground="green")
            self.dispense_btn.config(state="normal")
            self.log("Envelope retirado - Sistema pronto para próximo ciclo")

        # Detecta reset do sistema
        if "SISTEMA RESETADO" in data_upper and "PRONTO PARA USO" in data_upper:
            self.state_label.config(text="PRONTO", foreground="green")
            self.dispense_btn.config(state="normal")
            self.log("Sistema resetado com sucesso")

    def send_command(self, command):
        """Envia comando para o Arduino"""
        if self.is_connected and self.serial_connection:
            try:
                self.serial_connection.write(f"{command}\n".encode())
                self.log(f"Comando enviado: {command}")
            except Exception as e:
                self.log(f"ERRO ao enviar comando: {str(e)}")
                messagebox.showerror("Erro", f"Falha ao enviar comando: {str(e)}")

    def dispense_envelope(self):
        """Envia comando para dispensar envelope"""
        self.send_command("D")
        self.dispense_btn.config(state="disabled")  # Desativa até completar o ciclo

    def request_status(self):
        """Solicita status do sistema"""
        self.send_command("S")

    def reset_system(self):
        """Reseta o sistema"""
        self.send_command("R")
        # Não desabilita o botão aqui - o Arduino vai enviar o status atualizado

    def log(self, message):
        """Adiciona mensagem ao log"""
        self.log_text.config(state="normal")
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{timestamp}] {message}\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")

    def on_closing(self):
        """Executado ao fechar a janela"""
        self.running = False
        self.disconnect_serial()
        self.root.destroy()

# Instruções de uso
INSTRUCTIONS = """
INSTRUÇÕES DE INSTALAÇÃO E USO:

1. INSTALAÇÃO DO PYTHON:
   - Baixe Python em: python.org
   - Durante a instalação, MARQUE a opção "Add Python to PATH"

2. INSTALAÇÃO DA BIBLIOTECA PYSERIAL:
   - Abra o Prompt de Comando (cmd)
   - Digite: pip install pyserial

3. EXECUÇÃO:
   - Salve este arquivo como "dispenser_interface.py"
   - Clique duplo para executar
   - Ou execute via cmd: python dispenser_interface.py

4. USO:
   - A interface tentará conectar automaticamente na COM5
   - Use "Listar Portas" se não conectar
   - Clique em "Dispensar Envelope" para operar o dispenser
   - O botão será automaticamente habilitado/desabilitado conforme o estado
"""

def show_instructions():
    """Mostra instruções de uso"""
    messagebox.showinfo("Instruções de Instalação", INSTRUCTIONS)

if __name__ == "__main__":
    root = tk.Tk()
    app = DispenserInterface(root)
    
    # Menu com instruções
    menu_bar = tk.Menu(root)
    help_menu = tk.Menu(menu_bar, tearoff=0)
    help_menu.add_command(label="Instruções de Instalação", command=show_instructions)
    menu_bar.add_cascade(label="Ajuda", menu=help_menu)
    root.config(menu=menu_bar)
    
    # Configurar fechamento seguro
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    
    root.mainloop()