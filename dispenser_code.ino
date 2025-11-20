/*
 * DISPENSER DE ENVELOPES - VERSÃO FINAL CORRIGIDA
 * Controle completo com dois motores e três sensores
 * Compatível com interface Python
 * 
 * LÓGICA DOS SENSORES:
 * - SENSOR_ESTOQUE: HIGH = VAZIO, LOW = COM ENVELOPES (invertido)
 * - SENSOR_SAIDA: HIGH = LIVRE, LOW = ENVELOPE NA SAÍDA (invertido)
 * - SENSOR_POSICAO: HIGH = envelope na posição
 * 
 * COMANDOS SERIAIS:
 * D - Dispensar envelope
 * S - Status do sistema
 * R - Reset/Reiniciar sistema
 */

// ===== DEFINIÇÃO DOS PINOS =====
const int MOTOR1_A = 3;      // PWM para velocidade motor 1
const int MOTOR1_B = 4;      // Direção motor 1
const int MOTOR2_A = 5;      // PWM para velocidade motor 2  
const int MOTOR2_B = 6;      // Direção motor 2
const int SENSOR_ESTOQUE = 7;    // HIGH = VAZIO, LOW = COM ENVELOPES (INVERTIDO)
const int SENSOR_SAIDA = 8;      // HIGH = LIVRE, LOW = ENVELOPE NA SAÍDA (INVERTIDO - PULLUP)
const int SENSOR_POSICAO = 9;    // Endstop óptico - HIGH = envelope na posição

// ===== CONFIGURAÇÕES DE TEMPO (ms) =====
const unsigned long TEMPO_AVANCO = 2000;      // Tempo que ambos motores avançam juntos
const unsigned long TEMPO_EJECAO_M2 = 5000;   // Tempo máximo para ejetar envelope
const unsigned long TIMEOUT_CICLO = 15000;    // Timeout total do ciclo

// ===== VELOCIDADES DOS MOTORES (0-255 PWM) =====
const int VELOCIDADE_NORMAL = 200;
const int VELOCIDADE_LENTA = 150;

// ===== ESTADOS DO SISTEMA =====
enum Estado {
  REPOUSO,
  VERIFICANDO_CONDICOES,
  LEVANDO_ENVELOPE_POSICAO,
  AVANCANDO_ENVELOPE,
  EJETANDO_ENVELOPE,
  AGUARDANDO_RETIRADA,
  ERRO_ESTOQUE_VAZIO,
  ERRO_SAIDA_OCUPADA,
  ERRO_TIMEOUT
};

// ===== VARIÁVEIS GLOBAIS =====
Estado estadoAtual = REPOUSO;
unsigned long tempoInicioEstado = 0;
unsigned long tempoInicioCiclo = 0;
String comandoSerial = "";

// ===== CONFIGURAÇÃO INICIAL =====
void setup() {
  // Inicializa comunicação serial
  Serial.begin(9600);
  while (!Serial) {
    ; // Espera a porta serial conectar
  }
  
  Serial.println("=== DISPENSER DE ENVELOPES - SISTEMA PRINCIPAL ===");
  Serial.println("Sistema inicializando...");
  
  // Configura pinos dos motores
  pinMode(MOTOR1_A, OUTPUT);
  pinMode(MOTOR1_B, OUTPUT);
  pinMode(MOTOR2_A, OUTPUT);
  pinMode(MOTOR2_B, OUTPUT);
  
  // Configura pinos dos sensores
  pinMode(SENSOR_ESTOQUE, INPUT_PULLUP);  // Usa pullup interno
  pinMode(SENSOR_SAIDA, INPUT_PULLUP);    // Usa pullup interno (invertido)
  pinMode(SENSOR_POSICAO, INPUT);
  
  // Para todos os motores inicialmente
  pararMotores();
  
  // Verifica estado inicial dos sensores
  verificarEstadoInicial();
  
  Serial.println("=== SISTEMA PRONTO PARA OPERAÇÃO ===");
  Serial.println("Comandos: D-Dispensar, S-Status, R-Reset");
  enviarStatusCompleto();
}

// ===== LOOP PRINCIPAL =====
void loop() {
  // Processa comandos seriais
  processarComandosSeriais();
  
  // Leitura dos sensores (ambos invertidos por PULLUP)
  bool estoqueVazio = digitalRead(SENSOR_ESTOQUE);       // HIGH = VAZIO (corrigido)
  bool envelopeNaSaida = !digitalRead(SENSOR_SAIDA);     // LOW = ENVELOPE NA SAÍDA (invertido)
  bool envelopeNaPosicao = digitalRead(SENSOR_POSICAO);  // Normal
  
  // Máquina de estados principal
  switch(estadoAtual) {
    
    case REPOUSO:
      // Sistema pronto - aguarda comando
      // Nenhuma ação necessária no loop
      break;
      
    case VERIFICANDO_CONDICOES:
      Serial.println("VERIFICANDO: Condições para operação...");
      
      if (estoqueVazio) {
        Serial.println("ERRO: Estoque vazio! Recarregue envelopes.");
        mudarEstado(ERRO_ESTOQUE_VAZIO);
      } else if (envelopeNaSaida) {
        Serial.println("ERRO: Saída ocupada! Retire o envelope antes.");
        mudarEstado(ERRO_SAIDA_OCUPADA);
      } else {
        Serial.println("SUCESSO: Condições OK, iniciando ciclo...");
        mudarEstado(LEVANDO_ENVELOPE_POSICAO);
        
        // Ambos motores giram para TRÁS para levar envelope até sensor de posição
        girarMotor1ParaTras();
        girarMotor2ParaTras();
        Serial.println("MOTORES: Ambos girando para TRÁS");
      }
      break;
      
    case LEVANDO_ENVELOPE_POSICAO:
      // Verifica se envelope chegou na posição do sensor
      if (envelopeNaPosicao) {
        Serial.println("SUCESSO: Envelope chegou na posição do sensor");
        mudarEstado(AVANCANDO_ENVELOPE);
        
        // Para ambos motores
        pararMotores();
        delay(200); // Pequena pausa para estabilização
        
        // Ambos motores giram para FRENTE na mesma direção
        girarMotor1ParaFrente();
        girarMotor2ParaFrente();
        Serial.println("MOTORES: Ambos girando para FRENTE por 2 segundos");
      } else if (millis() - tempoInicioCiclo > TIMEOUT_CICLO) {
        Serial.println("ERRO: Timeout ao levar envelope para posição");
        mudarEstado(ERRO_TIMEOUT);
        pararMotores();
      }
      break;
      
    case AVANCANDO_ENVELOPE:
      // Avança envelope por tempo pré-definido
      if (millis() - tempoInicioEstado >= TEMPO_AVANCO) {
        Serial.println("SUCESSO: Envelope avançado, iniciando ejeção...");
        mudarEstado(EJETANDO_ENVELOPE);
        
        // Motor 1 para, apenas motor 2 continua girando
        pararMotor1();
        girarMotor2ParaFrente();
        Serial.println("MOTORES: Apenas Motor 2 girando para ejetar");
      }
      break;
      
    case EJETANDO_ENVELOPE:
      // Ejetar envelope até ser detectado na saída
      if (envelopeNaSaida) {
        Serial.println("SUCESSO: Envelope ejetado e detectado na saída!");
        pararMotores();
        mudarEstado(AGUARDANDO_RETIRADA);
        enviarStatusCompleto();
      } else if (millis() - tempoInicioEstado >= TEMPO_EJECAO_M2) {
        Serial.println("AVISO: Tempo de ejeção excedido - verifique envelope");
        pararMotores();
        mudarEstado(AGUARDANDO_RETIRADA);
        enviarStatusCompleto();
      }
      break;
      
    case AGUARDANDO_RETIRADA:
      // Aguarda usuário retirar o envelope da saída
      if (!envelopeNaSaida) {
        Serial.println("SUCESSO: Envelope retirado pelo usuário - Sistema pronto");
        mudarEstado(REPOUSO);
        enviarStatusCompleto();
      }
      break;
      
    case ERRO_ESTOQUE_VAZIO:
      // Aguarda recarga do estoque
      if (!estoqueVazio) {
        Serial.println("SUCESSO: Estoque recarregado - Sistema normalizado");
        mudarEstado(REPOUSO);
        enviarStatusCompleto();
      }
      break;
      
    case ERRO_SAIDA_OCUPADA:
      // Aguarda retirada do envelope da saída
      if (!envelopeNaSaida) {
        Serial.println("SUCESSO: Saída liberada - Sistema normalizado");
        mudarEstado(REPOUSO);
        enviarStatusCompleto();
      }
      break;
      
    case ERRO_TIMEOUT:
      // Requer reset manual (comando R) - já implementado
      break;
  }
  
  delay(100); // Pequeno delay para estabilidade
}

// ===== FUNÇÕES DE COMUNICAÇÃO SERIAL =====

/**
 * Processa comandos recebidos pela porta serial
 */
void processarComandosSeriais() {
  while (Serial.available() > 0) {
    char caractere = Serial.read();
    
    if (caractere == '\n' || caractere == '\r') {
      if (comandoSerial.length() > 0) {
        comandoSerial.toUpperCase();
        Serial.print("COMANDO RECEBIDO: ");
        Serial.println(comandoSerial);
        
        if (comandoSerial == "D") {
          // Comando para dispensar envelope
          if (estadoAtual == REPOUSO) {
            Serial.println("INICIANDO: Ciclo de dispensação solicitado");
            mudarEstado(VERIFICANDO_CONDICOES);
            tempoInicioCiclo = millis();
          } else {
            Serial.print("AVISO: Sistema ocupado - Estado atual: ");
            serialPrintEstado(estadoAtual);
          }
        } else if (comandoSerial == "S") {
          // Comando para solicitar status
          enviarStatusCompleto();
        } else if (comandoSerial == "R") {
          // Comando para resetar sistema - FUNCIONA EM QUALQUER ESTADO
          resetSistema();
        }
        
        comandoSerial = "";
      }
    } else if (isAlpha(caractere)) {
      comandoSerial += caractere;
    }
  }
}

/**
 * Envia status completo do sistema para a serial
 */
void enviarStatusCompleto() {
  // Lê estado atual dos sensores (ambos invertidos)
  bool estoqueVazio = digitalRead(SENSOR_ESTOQUE);   // HIGH = VAZIO
  bool envelopeNaSaida = !digitalRead(SENSOR_SAIDA); // LOW = ENVELOPE NA SAÍDA (invertido)
  bool envelopeNaPosicao = digitalRead(SENSOR_POSICAO);
  
  Serial.println("=== STATUS DO SISTEMA ===");
  Serial.print("Estado: ");
  serialPrintEstado(estadoAtual);
  Serial.print("Estoque: ");
  Serial.println(estoqueVazio ? "VAZIO" : "COM ENVELOPES");
  Serial.print("Saída: ");
  Serial.println(envelopeNaSaida ? "OCUPADA" : "LIVRE");
  Serial.print("Posição: ");
  Serial.println(envelopeNaPosicao ? "ENVELOPE PRESENTE" : "LIVRE");
  Serial.print("Tempo no estado: ");
  Serial.print((millis() - tempoInicioEstado) / 1000);
  Serial.println(" segundos");
  Serial.println("========================");
}

/**
 * Imprime o estado atual do sistema em formato legível
 */
void serialPrintEstado(Estado estado) {
  switch(estado) {
    case REPOUSO: 
      Serial.println("REPOUSO - Pronto para uso"); 
      break;
    case VERIFICANDO_CONDICOES: 
      Serial.println("VERIFICANDO CONDIÇÕES"); 
      break;
    case LEVANDO_ENVELOPE_POSICAO: 
      Serial.println("LEVANDO ENVELOPE À POSIÇÃO"); 
      break;
    case AVANCANDO_ENVELOPE: 
      Serial.println("AVANÇANDO ENVELOPE"); 
      break;
    case EJETANDO_ENVELOPE: 
      Serial.println("EJETANDO ENVELOPE"); 
      break;
    case AGUARDANDO_RETIRADA: 
      Serial.println("AGUARDANDO RETIRADA"); 
      break;
    case ERRO_ESTOQUE_VAZIO: 
      Serial.println("ERRO - ESTOQUE VAZIO"); 
      break;
    case ERRO_SAIDA_OCUPADA: 
      Serial.println("ERRO - SAÍDA OCUPADA"); 
      break;
    case ERRO_TIMEOUT: 
      Serial.println("ERRO - TIMEOUT"); 
      break;
  }
}

/**
 * Reinicia o sistema para o estado inicial
 * FUNCIONA EM QUALQUER ESTADO - CORREÇÃO CRÍTICA
 */
void resetSistema() {
  Serial.println("REINICIANDO: Sistema sendo resetado...");
  pararMotores();
  estadoAtual = REPOUSO;
  tempoInicioEstado = millis();
  Serial.println("SUCESSO: Sistema resetado e pronto para uso");
  enviarStatusCompleto();
}

/**
 * Muda o estado do sistema e registra o tempo da mudança
 */
void mudarEstado(Estado novoEstado) {
  estadoAtual = novoEstado;
  tempoInicioEstado = millis();
  Serial.print("MUDANÇA DE ESTADO: ");
  serialPrintEstado(novoEstado);
}

// ===== FUNÇÕES DE CONTROLE DOS MOTORES =====

void pararMotores() {
  digitalWrite(MOTOR1_A, LOW);
  digitalWrite(MOTOR1_B, LOW);
  digitalWrite(MOTOR2_A, LOW);
  digitalWrite(MOTOR2_B, LOW);
}

void pararMotor1() {
  digitalWrite(MOTOR1_A, LOW);
  digitalWrite(MOTOR1_B, LOW);
}

void pararMotor2() {
  digitalWrite(MOTOR2_A, LOW);
  digitalWrite(MOTOR2_B, LOW);
}

void girarMotor1ParaTras() {
  analogWrite(MOTOR1_A, VELOCIDADE_NORMAL);
  digitalWrite(MOTOR1_B, LOW);
}

void girarMotor1ParaFrente() {
  digitalWrite(MOTOR1_A, LOW);
  analogWrite(MOTOR1_B, VELOCIDADE_NORMAL);
}

void girarMotor2ParaTras() {
  analogWrite(MOTOR2_A, VELOCIDADE_NORMAL);
  digitalWrite(MOTOR2_B, LOW);
}

void girarMotor2ParaFrente() {
  digitalWrite(MOTOR2_A, LOW);
  analogWrite(MOTOR2_B, VELOCIDADE_NORMAL);
}

// ===== FUNÇÕES AUXILIARES =====

/**
 * Verifica e exibe o estado inicial de todos os sensores
 */
void verificarEstadoInicial() {
  Serial.println("--- VERIFICAÇÃO INICIAL DOS SENSORES ---");
  
  Serial.print("Sensor Estoque (Pino 7): ");
  Serial.println(digitalRead(SENSOR_ESTOQUE) ? "VAZIO (HIGH)" : "COM ENVELOPES (LOW)");
  
  Serial.print("Sensor Saída (Pino 8): ");
  Serial.println(digitalRead(SENSOR_SAIDA) ? "LIVRE (HIGH)" : "OCUPADO (LOW)");
  
  Serial.print("Sensor Posição (Pino 9): ");
  Serial.println(digitalRead(SENSOR_POSICAO) ? "OCUPADO (HIGH)" : "LIVRE (LOW)");
  
  Serial.println("--- VERIFICAÇÃO CONCLUÍDA ---");
}