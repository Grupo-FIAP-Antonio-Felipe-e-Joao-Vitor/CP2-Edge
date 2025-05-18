// Comentários gerais sobre o código:
// Este programa implementa um sistema de monitoramento ambiental que lê dados de
// luminosidade, temperatura e umidade. Ele exibe essas informações em um display LCD I2C,
// permite a configuração de unidades de temperatura e fuso horário através do monitor serial,
// e registra dados anormais na memória EEPROM.
// Também possui um sistema de alerta visual (LEDs) e sonoro (buzzer).

// --- BIBLIOTECAS ---
#include <RTClib.h>         // Biblioteca para o módulo Real Time Clock (RTC)
#include <LiquidCrystal_I2C.h> // Biblioteca para controlar o display LCD com interface I2C
#include <Wire.h>           // Biblioteca para comunicação I2C (necessária para LCD I2C e RTC)
#include "DHT.h"            // Biblioteca para o sensor de temperatura e umidade 
#include <EEPROM.h>         // Biblioteca para interagir com a memória EEPROM interna do microcontrolador

// --- OBJETOS E CONSTANTES GLOBAIS ---

// Inicializa o objeto lcd, especificando o endereço I2C (0x27 comum), e as dimensões (16 colunas, 2 linhas)
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Inicializa o objeto RTC para o chip DS1307
RTC_DS1307 RTC;

// Define o pino digital ao qual o pino DATA do sensor DHT está conectado
#define DHTPIN 12
// Define o tipo de sensor DHT (DHT11 neste caso)
#define DHTTYPE DHT11
// Inicializa o objeto dht com o pino e o tipo definidos
DHT dht(DHTPIN, DHTTYPE);

// Define o pino analógico ao qual o buzzer está conectado
int buzzer = A1;

// --- VARIÁVEIS GLOBAIS PARA CONFIGURAÇÕES E ESTADO ---

// Flag para determinar se a temperatura deve ser exibida em Fahrenheit (true) ou Celsius (false)
bool usarFarenhait = false;
// Endereços na EEPROM para armazenar configurações persistentes
int tempAdress = 0;         // Endereço para armazenar a preferência de unidade de temperatura
int fusoAdress = 1;         // Endereço para armazenar o fuso horário
int guardarAtualAdress = 3; // Endereço para armazenar o próximo local de escrita no log da EEPROM
int startAdress = 5;        // Endereço inicial para os logs de dados na EEPROM
int tamanhoDeLog = 10;      // Tamanho em bytes de cada entrada de log (timestamp, temp, umid, luz)
int maxSaves = 50;          // Número máximo de entradas de log que podem ser salvas
// Endereço final para os logs, calculado com base no número máximo de saves e tamanho do log
int endAdress = (maxSaves * tamanhoDeLog) + 5;

// Credenciais de administrador para funções protegidas (ex: limpar log)
String usuarioAdm = "Admin";
String senhaAdm = "1234";
// Flags para controlar o processo de login
bool aguardandoUsuario = false;
bool aguardandoSenha = false;
String usuarioDigitado = ""; // Armazena o nome de usuário digitado

// Variável para armazenar o fuso horário (offset em horas em relação ao UTC)
int fuso;
int UTC_OFFSET = fuso; // Usado para ajustar o tempo do RTC

// --- DEFINIÇÕES DE CARACTERES PERSONALIZADOS PARA O LCD ---
// Arrays de bytes que definem pixels para caracteres customizados
byte bode[] = {B00000, B10001, B11011, B11111, B10101, B11111, B01110, B00100}; // Ícone "bode"
byte chifreEsq[] = {B00000, B00111, B01100, B01001, B00110, B00000, B00000, B00000}; // Parte do logo
byte chifreDir[] = {B00000, B11100, B00110, B10010, B01100, B00000, B00000, B00000}; // Parte do logo

byte abaixoDosParametros[] = {B00000, B00100, B00100, B00100, B00100, B10101, B01110, B00000}; // Ícone: abaixo do normal
byte acimaDosParametros[] = {B00000, B01110, B10101, B00100, B00100, B00100, B00100, B00000};  // Ícone: acima do normal
byte dentroDosParametros[] = {B00000, B01010, B01010, B00000, B00000, B10001, B01110, B00000}; // Ícone: dentro do normal

// --- PINOS E PARÂMETROS DOS SENSORES E ATUADORES ---
int sensorLuz = A0; // Pino analógico para o sensor de luminosidade (LDR)
// Limites para a luminosidade (em porcentagem)
int minOkLuz = 30; // Mínimo aceitável
int maxOkLuz = 50; // Máximo aceitável
// Limites para o estado de alerta de luminosidade
int intervaloAlertaMinLuz = minOkLuz - 10;
int intervaloAlertaMaxLuz = maxOkLuz + 10;

// Limites para a temperatura 
int minOkTemp = 11;
int maxOkTemp = 15;
// Intervalos de alerta para temperatura (offset em relação aos limites Ok)
int intervaloAlertaMinTemp = 3; // Quanto abaixo do minOkTemp para alerta crítico
int intervaloAlertaMaxTemp = 3; // Quanto acima do maxOkTemp para alerta crítico

// Limites para a umidade (em porcentagem)
int minOkUmid = 60;
int maxOkUmid = 80;
// Intervalos de alerta para umidade
int intervaloAlertaMinUmid = 10;
int intervaloAlertaMaxUmid = 10;

// Pinos digitais para os LEDs de status
int ledVerm = 7;  // LED vermelho (alerta crítico)
int ledAmar = 6;  // LED amarelo (alerta moderado)
int ledVerd = 5;  // LED verde (status normal)

// Pino digital para o botão de mudança de HUD (display no LCD)
int botaoHud = 2;

// Estado atual do HUD (0: valores, 1: ícones de status, 2: data/hora)
int hudState = 0;

// --- VARIÁVEIS DE CONTROLE DE MENU (VIA SERIAL) ---
bool menuPrincipal = true;  // Flag: menu principal está ativo
bool menuTemp = false;      // Flag: menu de configuração de temperatura está ativo
bool menuFuso = false;      // Flag: menu de configuração de fuso horário está ativo
bool menuDataLogger = false; // Flag: menu do data logger está ativo

// String para armazenar a entrada do usuário via monitor serial
String entradaUsuario = "";

// --- FUNÇÃO SETUP: Executada uma vez no início ---
void setup() {
  Serial.begin(9600);     // Inicializa a comunicação serial
  EEPROM.begin();         // Inicializa a EEPROM
  mostrarMenuPrincipal(); // Exibe o menu principal no monitor serial

  // Verifica e corrige o endereço de início do log se estiver inválido
  EEPROM.get(guardarAtualAdress, startAdress); // Tenta ler o último endereço de log salvo
  if (startAdress < 5 || startAdress > endAdress) { // Se inválido ou primeira vez
    startAdress = 5; // Define para o início da área de log
    EEPROM.put(guardarAtualAdress, startAdress); // Salva na EEPROM
  }

  // Carrega a preferência de unidade de temperatura da EEPROM
  if (EEPROM.read(tempAdress) != 0xFF) { // 0xFF é o valor padrão de uma EEPROM não escrita
    EEPROM.get(tempAdress, usarFarenhait);
  } else {
    usarFarenhait = false; // Valor padrão se nada estiver salvo
    EEPROM.put(tempAdress, usarFarenhait); // Salva o padrão
  }

  // Carrega a configuração de fuso horário da EEPROM
  if (EEPROM.read(fusoAdress) != 0xFF) {
    EEPROM.get(fusoAdress, UTC_OFFSET);
    fuso = UTC_OFFSET;
  } else {
    fuso = 0; // Fuso padrão (UTC)
    UTC_OFFSET = 0;
    EEPROM.put(fusoAdress, UTC_OFFSET); // Salva o padrão
  }

  RTC.begin(); // Inicializa o RTC
  // Ajusta o RTC para a data e hora em que o código foi compilado.
  RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Configura os pinos
  pinMode(sensorLuz, INPUT);    // Sensor de luz como entrada
  pinMode(botaoHud, INPUT);     // Botão do HUD como entrada
  pinMode(buzzer, OUTPUT);      // Buzzer como saída
  // LEDs como saída
  pinMode(ledVerm, OUTPUT);     
  pinMode(ledAmar, OUTPUT);
  pinMode(ledVerd, OUTPUT);

  dht.begin(); // Inicializa o sensor DHT

  lcd.init();      // Inicializa o LCD
  lcd.backlight(); // Liga a luz de fundo do LCD

  // Cria os caracteres personalizados no LCD, associando os arrays de bytes a índices (0-7)
  lcd.createChar(0, chifreEsq);
  lcd.createChar(1, bode);
  lcd.createChar(2, chifreDir);
  lcd.createChar(4, abaixoDosParametros);
  lcd.createChar(5, acimaDosParametros);
  lcd.createChar(6, dentroDosParametros);

  // Animações iniciais no LCD
  animarLogo(0, 0, 100); // Exibe o logo "Dev Goat"
  delay(1000);

  animarBemVindo(3, 1, 100); // Exibe "Bem-Vindo"
  delay(3000);

  lcd.clear(); // Limpa o LCD

  // Faz uma leitura inicial dos sensores para exibir algo imediatamente
  float leituraBrutaLuz = analogRead(sensorLuz);
  // Mapeia a leitura analógica do LDR (0-1023) para uma porcentagem (0-100)
  int leituraPorcentagemLuz = map(leituraBrutaLuz, 0, 1000, 100, 0);
  float leituraPorcentagemUmid = dht.readHumidity(); // Lê a umidade
  float leituraCelsiusTemp = dht.readTemperature(usarFarenhait); // Lê a temperatura na unidade configurada

  DateTime now = RTC.now(); // Obtém a hora atual do RTC

  // Aplica o deslocamento do fuso horário
  int offsetSeconds = UTC_OFFSET * 3600; // Converte o fuso (em horas) para segundos
  // Cria um novo objeto DateTime adicionando o offset ao timestamp Unix do tempo atual
  DateTime adjustedTime = DateTime(now.unixtime() + offsetSeconds);

  // Exibe as informações iniciais no LCD
  apresentarInfo(leituraPorcentagemLuz, leituraPorcentagemUmid, leituraCelsiusTemp, hudState, adjustedTime);
}

// --- FUNÇÃO LOOP: Executada repetidamente ---
void loop() {
  // Variáveis para acumular leituras dos sensores para cálculo da média
  float somaLuz = 0;
  float somaUmid = 0;
  float somaTemp = 0;
  DateTime now = RTC.now(); // Obtém a hora atual do RTC

  // Aplica o deslocamento do fuso horário
  int offsetSeconds = UTC_OFFSET * 3600;
  now = now.unixtime() + offsetSeconds;
  DateTime adjustedTime = DateTime(now);

  // Loop para coletar 100 amostras de cada sensor e calcular a média
  // Dura 100 * 100ms = 10 segundos.
  for (int i = 0; i < 100; i++) {
    lerInput(); // Verifica e processa entrada do usuário via serial a cada iteração

    float leituraBrutaLuz = (analogRead(sensorLuz));
    // Soma a leitura a uma variavel externa do loop for
    somaLuz += leituraBrutaLuz;
    
    float leituraPorcentagemUmid = dht.readHumidity();
    // Soma a leitura a uma variavel externa do loop for
    somaUmid += leituraPorcentagemUmid;

    float leituraTemp = dht.readTemperature(usarFarenhait);
    // Soma a leitura a uma variavel externa do loop for
    somaTemp += leituraTemp;

    delay(100); // Pequena pausa entre as leituras

    // Verifica se o botão do HUD foi pressionado
    if (digitalRead(botaoHud) == HIGH) {
      // Lógica para ciclar entre os estados do HUD (0, 1, 2)
      if (hudState == 0) {
        hudState = 1;
      } else if (hudState == 1) {
        hudState = 2;
      } else if (hudState == 2) {
        hudState = 0;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mudando de ");
      lcd.setCursor(0, 1);
      lcd.print("hud...");
    }
  }
  // Calcula as médias das leituras
  float mediaLuzBruta = somaLuz / 100;
  int leituraPorcentagemLuz = map(mediaLuzBruta, 0, 1000, 100, 0); // Mapeia a média

  float mediaUmid = somaUmid / 100;
  float mediaTemp = somaTemp / 100;

  // Exibe as médias no LCD
  apresentarInfo(leituraPorcentagemLuz, mediaUmid, mediaTemp, hudState, adjustedTime);

  // Verifica se algum dos parâmetros está fora dos limites "OK" para registrar no log
  if (leituraPorcentagemLuz < minOkLuz || leituraPorcentagemLuz > maxOkLuz ||
      mediaUmid < minOkUmid || mediaUmid > maxOkUmid ||
      mediaTemp < minOkTemp || mediaTemp > maxOkTemp) {

    // Converte os valores para inteiros para economizar espaço na EEPROM
    int tempInt = (int)(mediaTemp);
    int umidInt = (int)(mediaUmid);
    int luzInt = (int)leituraPorcentagemLuz;

    // Verifica se o endereço de escrita do log atingiu o final da área reservada
    if (startAdress >= endAdress) {
      startAdress = 5; // Volta para o início, sobrescrevendo logs antigos (buffer circular)
    }

    // Grava os dados na EEPROM:
    // - Timestamp Unix (4 bytes)
    // - Temperatura (int, 2 bytes)
    // - Umidade (int, 2 bytes)
    // - Luz (int, 2 bytes)
    // Total: 10 bytes, conforme 'tamanhoDeLog'
    EEPROM.put(startAdress, now.unixtime()); // Usa 'now' (sem fuso) para consistência do timestamp
    EEPROM.put(startAdress + 4, tempInt);
    EEPROM.put(startAdress + 6, umidInt);
    EEPROM.put(startAdress + 8, luzInt);

    startAdress += tamanhoDeLog; // Avança o ponteiro para a próxima posição de log
    EEPROM.put(guardarAtualAdress, startAdress); // Salva o novo ponteiro na EEPROM
  }
}

// --- FUNÇÃO lerInput: Lê entrada do usuário via Serial ---
void lerInput() {
  if (Serial.available()) { // Verifica se há dados disponíveis para leitura na serial
    char c = Serial.read(); // Lê um caractere

    // Se o caractere for nova linha ('\n') ou ('\r'),
    // significa que o usuário terminou de digitar o comando.
    if (c == '\n' || c == '\r') {
      if (entradaUsuario.length() > 0) { // Processa apenas se algo foi digitado
          tratarOpcao(entradaUsuario); // Chama a função para tratar o comando
          entradaUsuario = ""; // Limpa o buffer de entrada para o próximo comando
      }
    } else {
      entradaUsuario += c; // Adiciona o caractere à string de entrada
    }
  }
}

// --- FUNÇÃO tratarOpcao: Processa os comandos recebidos via Serial ---
void tratarOpcao(String comando) {
  comando.trim(); // Remove espaços em branco e quebras de linha do início e fim do comando

  // Lógica de menu baseada nas flags de estado (menuPrincipal, menuTemp, etc.)
  if (menuPrincipal) {
    if (comando == "1") { // Opção para configurar temperatura
      menuTemp = true;
      menuFuso = false;
      menuPrincipal = false;
      menuDataLogger = false;
      mostrarMenuTemp();
    } else if (comando == "2") { // Opção para configurar fuso horário
      menuTemp = false;
      menuFuso = true;
      menuPrincipal = false;
      menuDataLogger = false;
      mostrarMenuFuso();
    } else if (comando == "3") { // Opção para acessar o Data Logger
      menuTemp = false;
      menuFuso = false;
      menuPrincipal = false;
      menuDataLogger = true;
      mostrarMenuDataLogger();
    } else {
      Serial.println("Opcao invalida no Menu Principal.");
    }
  }
  else if (menuTemp) {
    if (comando == "1") { // Configurar para Celsius
      usarFarenhait = false;
      EEPROM.put(tempAdress, usarFarenhait); // Salva a preferência na EEPROM
      Serial.println("Reinicie o sistema, para que as alterações sejam salvas!");
    } else if (comando == "2") { // Configurar para Fahrenheit
      usarFarenhait = true;
      EEPROM.put(tempAdress, usarFarenhait); // Salva a preferência na EEPROM
      // EEPROM.commit(); // Para ESP32
      Serial.println("Reinicie o sistema para que as alterações sejam salvas!");
    } else if (comando.equalsIgnoreCase("voltar")) { // Voltar ao menu principal
      menuTemp = false;
      menuFuso = false;
      menuDataLogger = false;
      menuPrincipal = true;
      mostrarMenuPrincipal();
    } else {
      Serial.println("Opcao invalida no Menu Temperatura.");
    }
  }
  else if (menuFuso) {
    if (comando.equalsIgnoreCase("voltar")) { // Voltar ao menu principal
      menuTemp = false;
      menuFuso = false;
      menuDataLogger = false;
      menuPrincipal = true;
      mostrarMenuPrincipal();
    } else { // Usuário digitou um valor para o fuso
      fuso = comando.toInt(); // Converte a string do comando para inteiro
      EEPROM.put(fusoAdress, fuso); // Salva o fuso na EEPROM
      UTC_OFFSET = fuso; // Atualiza a variável global de offset
      Serial.print("Fuso horario atualizado para: UTC");
      if(fuso >= 0) {Serial.print("+");}
      Serial.println(fuso);
      Serial.println("Reinicie o sistema para que as alterações sejam salvas!");
    }
  }
  else if (menuDataLogger) {
    if (aguardandoUsuario) { // Se o sistema está esperando o nome de usuário
        usuarioDigitado = comando;
        if (usuarioDigitado == usuarioAdm) {
            Serial.println("Digite a senha:");
            aguardandoUsuario = false;
            aguardandoSenha = true; // Próximo input será a senha
        } else {
            Serial.println("Usuario incorreto.");
            aguardandoUsuario = false; // Volta ao menu do data logger
            mostrarMenuDataLogger(); // Reexibe opções
        }
    } else if (aguardandoSenha) { // Se o sistema está esperando a senha
        if (comando == senhaAdm) {
            Serial.println("Limpando Data Logger...");
            // Percorre toda a área da EEPROM reservada para logs
            for (int addr = 5; addr <= endAdress; addr++) {
                EEPROM.write(addr, 0xFF); // Escreve 0xFF (valor "vazio" da EEPROM)
            }
            startAdress = 5; // Reseta o ponteiro de gravação para o início
            EEPROM.put(guardarAtualAdress, startAdress);
            Serial.println("Data Logger limpo!");
        } else {
            Serial.println("Senha incorreta.");
        }
        aguardandoSenha = false; // Finaliza o processo de login (sucesso ou falha)
        mostrarMenuDataLogger(); // Reexibe opções
    }
    // Opções normais do menu Data Logger (quando não está aguardando login)
    else if (comando == "1") { // Exibir Data Log
      Serial.println("Log: ");
      Serial.println("Data/Hora\t\tTemp\tUmid\tLuz");
      // Percorre a EEPROM lendo cada entrada de log
      // O endereço inicial dos logs é 5.

      for (int adress = 5; adress <= endAdress; adress += tamanhoDeLog) { // Itera pelos logs já escritos
        long timeStamp;
        int tempInt, umidInt, luzInt;

        EEPROM.get(adress, timeStamp); // Lê o timestamp
        // Verifica se o timestamp é válido (diferente de -1 ou 0xFFFFFFFF, que indica EEPROM vazia/apagada)
        if (timeStamp != 0xFFFFFFFF && timeStamp != 0) {
          EEPROM.get(adress + 4, tempInt); // Lê temperatura
          EEPROM.get(adress + 6, umidInt); // Lê umidade
          EEPROM.get(adress + 8, luzInt);  // Lê luz

          DateTime dt = DateTime(timeStamp); // Converte o timestamp para objeto DateTime
          Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
          Serial.print("\t");
          Serial.print(tempInt);
          if (usarFarenhait == false) { Serial.print(" C\t"); }
          else { Serial.print(" F\t"); }
          Serial.print(umidInt);
          Serial.print(" %\t");
          Serial.print(luzInt);
          Serial.println(" %");
        }
      }
    } else if (comando == "2") { // Apagar Data Logger (inicia processo de login)
      Serial.println("Digite o usuario:");
      aguardandoUsuario = true; // Prepara para receber o nome de usuário
    } else if (comando.equalsIgnoreCase("voltar")) { // Voltar ao menu principal
      menuTemp = false;
      menuFuso = false;
      menuDataLogger = false;
      menuPrincipal = true;
      mostrarMenuPrincipal();
    } else {
      Serial.println("Opcao invalida no Menu Data Logger.");
    }
  }
}


// --- FUNÇÕES DE EXIBIÇÃO DE MENU VIA SERIAL ---
void mostrarMenuPrincipal() {
  Serial.println("\n=== Sistema de controle ===");
  Serial.println("[1] Configuracoes de temperatura");
  Serial.println("[2] Configuracoes de fuso horario");
  Serial.println("[3] Data Logger");
}

void mostrarMenuTemp() {
  Serial.println("\n=== Configuracoes de temperatura ===");
  Serial.println("[1] Exibir temperatura em Celsius");
  Serial.println("[2] Exibir temperatura em Fahrenheit");
  Serial.println("[Voltar] Voltar ao menu inicial");
}

void mostrarMenuFuso() {
  Serial.println("\n=== Configuracoes de fuso ===");
  Serial.println("Digite o fuso horario (ex: -1, 0, 3 ...)");
  Serial.println("[Voltar] Voltar ao menu inicial");
}

void mostrarMenuDataLogger() {
  Serial.println("\n=== Data Logger ===");
  Serial.println("[1] Exibir Data Log");
  Serial.println("[2] Apagar Data Logger (requer login)");
  Serial.println("[Voltar] Voltar ao menu inicial");
}

// --- FUNÇÕES DE ANIMAÇÃO NO LCD ---
void animarLogo(int x, int y, int tempo) {
  lcd.setCursor(x, y); // Posição inicial no LCD

  // Exibe os caracteres customizados do logo (chifres e bode)
  lcd.write((byte)0); 
  lcd.write((byte)1);
  lcd.write((byte)2);
  lcd.print(" ");

  // Animação de texto "Dev Goat" letra por letra
  lcd.print("D"); delay(tempo);
  lcd.print("e"); delay(tempo);
  lcd.print("v"); delay(tempo);
  lcd.print(" "); delay(tempo);
  lcd.print("G"); delay(tempo);
  lcd.print("o"); delay(tempo);
  lcd.print("a"); delay(tempo);
  lcd.print("t"); delay(tempo);
  lcd.print(" ");

  lcd.write((byte)0);
  lcd.write((byte)1);
  lcd.write((byte)2);
}

void animarBemVindo(int x, int y, int tempo) {
  lcd.setCursor(x, y); // Posição inicial no LCD

  // Animação de texto "Bem-Vindo" letra por letra
  lcd.print("B"); delay(tempo);
  lcd.print("e"); delay(tempo);
  lcd.print("m"); delay(tempo);
  lcd.print("-"); delay(tempo);
  lcd.print("V"); delay(tempo);
  lcd.print("i"); delay(tempo);
  lcd.print("n"); delay(tempo);
  lcd.print("d"); delay(tempo);
  lcd.print("o"); delay(tempo);
}

// --- FUNÇÃO apresentarInfo: Exibe dados no LCD ---
void apresentarInfo(int luz, float umid, float temp, int hud, DateTime adjustedTime) {
  verificarStatus(luz, umid, temp); // Atualiza LEDs e buzzer com base nos valores atuais
  lcd.clear(); // Limpa o LCD antes de escrever novos dados

  // HUD 0: Mostra valores numéricos de Iluminação, Temperatura e Umidade
  if (hud == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Ilum  Temp  Umid"); // Cabeçalho
    lcd.setCursor(0, 1);
    lcd.print(luz);
    lcd.print("%");
    // Adiciona espaços para alinhar, dependendo do número de dígitos
    if (luz < 10) lcd.print("   "); else if (luz < 100) lcd.print("  "); else lcd.print(" ");

    lcd.print(temp, 1); // Temperatura com 1 casa decimal
    if (usarFarenhait == false) {
      lcd.print("C");
    } else {
      lcd.print("F");
    }
    // Adiciona espaços para alinhar
    if (abs(temp) < 10) lcd.print("   "); else if (abs(temp) < 100) lcd.print("  "); else lcd.print(" ");


    lcd.print(umid, 0); // Umidade com 0 casas decimais (inteiro)
    lcd.print("%");
  }
  // HUD 1: Mostra ícones de status para Iluminação, Temperatura e Umidade
  else if (hud == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Ilum  Temp  Umid"); // Cabeçalho
    lcd.setCursor(1, 1); // Posição para o ícone de luz
    if (luz > maxOkLuz) { lcd.write((byte)5); } // Ícone "acima dos parâmetros"
    else if (luz < minOkLuz) { lcd.write((byte)4); } // Ícone "abaixo dos parâmetros"
    else { lcd.write((byte)6); } // Ícone "dentro dos parâmetros"

    lcd.setCursor(7, 1); // Posição para o ícone de temperatura
    if (temp > maxOkTemp) { lcd.write((byte)5); }
    else if (temp < minOkTemp) { lcd.write((byte)4); }
    else { lcd.write((byte)6); }

    lcd.setCursor(13, 1); // Posição para o ícone de umidade
    if (umid > maxOkUmid) { lcd.write((byte)5); }
    else if (umid < minOkUmid) { lcd.write((byte)4); }
    else { lcd.write((byte)6); }
  }
  // HUD 2: Mostra Data e Hora atuais
  else if (hud == 2) {
    lcd.setCursor(0, 0);
    lcd.print("DATA: ");
    lcd.print(adjustedTime.day() < 10 ? "0" : ""); // Adiciona zero à esquerda se dia < 10
    lcd.print(adjustedTime.day());
    lcd.print("/");
    lcd.print(adjustedTime.month() < 10 ? "0" : ""); // Adiciona zero à esquerda se mês < 10
    lcd.print(adjustedTime.month());
    lcd.print("/");
    lcd.print(adjustedTime.year());

    lcd.setCursor(0, 1);
    lcd.print("HORA: ");
    lcd.print(adjustedTime.hour() < 10 ? "0" : ""); // Adiciona zero à esquerda se hora < 10
    lcd.print(adjustedTime.hour());
    lcd.print(":");
    lcd.print(adjustedTime.minute() < 10 ? "0" : ""); // Adiciona zero à esquerda se minuto < 10
    lcd.print(adjustedTime.minute());
  }
}

// --- FUNÇÃO verificarStatus: Controla LEDs e Buzzer com base nos parâmetros ---
void verificarStatus(int luz, float umid, float temp) {
  bool tudoOk = (luz >= minOkLuz && luz <= maxOkLuz) &&
                (temp >= minOkTemp && temp <= maxOkTemp) &&
                (umid >= minOkUmid && umid <= maxOkUmid);

  bool alertaCritico = luz < (minOkLuz - intervaloAlertaMinLuz) || luz > (maxOkLuz + intervaloAlertaMaxLuz) ||
                       temp < (minOkTemp - intervaloAlertaMinTemp) || temp > (maxOkTemp + intervaloAlertaMaxTemp) ||
                       umid < (minOkUmid - intervaloAlertaMinUmid) || umid > (maxOkUmid + intervaloAlertaMaxUmid);

  bool alertaModerado = ((luz >= (minOkLuz - intervaloAlertaMinLuz) && luz < minOkLuz) || (luz <= (maxOkLuz + intervaloAlertaMaxLuz) && luz > maxOkLuz) ||
                         (temp >= (minOkTemp - intervaloAlertaMinTemp) && temp < minOkTemp) || (temp <= (maxOkTemp + intervaloAlertaMaxTemp) && temp > maxOkTemp) ||
                         (umid >= (minOkUmid - intervaloAlertaMinUmid) && umid < minOkUmid) || (umid <= (maxOkUmid + intervaloAlertaMaxUmid) && umid > maxOkUmid));


  if (tudoOk) { // Todos os parâmetros dentro dos limites OK
    digitalWrite(ledVerd, HIGH);
    digitalWrite(ledAmar, LOW);
    digitalWrite(ledVerm, LOW);
    digitalWrite(buzzer, LOW);
  }
  else if (alertaCritico) { // Pelo menos um parâmetro em estado crítico
    digitalWrite(ledVerd, LOW);
    digitalWrite(ledAmar, LOW);
    digitalWrite(ledVerm, HIGH);
    digitalWrite(buzzer, HIGH); // Buzzer ativo em alerta crítico
  }
  else if (alertaModerado) { // Pelo menos um parâmetro em estado de alerta moderado
                             // (e nenhum em estado crítico, implicitamente pela ordem do if/else if)
    digitalWrite(ledVerd, LOW);
    digitalWrite(ledAmar, HIGH);
    digitalWrite(ledVerm, LOW);
    digitalWrite(buzzer, HIGH); // Buzzer também ativo em alerta moderado
  } 
  else { //se nenhuma condição anterior for atendida (deve ser raro, mas bom para segurança)
    digitalWrite(ledVerd, LOW);
    digitalWrite(ledAmar, LOW);
    digitalWrite(ledVerm, LOW);
    digitalWrite(buzzer, LOW);
  }
}