# Sistema de Monitoramento Ambiental com LCD e Data Logger

Este projeto implementa um sistema de monitoramento ambiental utilizando um Arduino para ler dados de luminosidade, temperatura e umidade. As informações são exibidas em um display LCD I2C, e dados anormais são registrados na memória EEPROM para posterior análise. O sistema também permite a configuração de unidades de temperatura (Celsius ou Fahrenheit) e do fuso horário através do monitor serial.

## Funcionalidades

* **Leitura de Sensores:**
    * Sensor de luminosidade (LDR).
    * Sensor de temperatura e umidade DHT11.
    * Média de 100 leituras para suavizar os dados.
* **Display LCD I2C:**
    * Exibição em tempo real das leituras de luminosidade (%), temperatura (°C ou °F) e umidade (%).
    * Três modos de visualização (HUDs):
        * Valores numéricos.
        * Ícones de status (dentro, abaixo ou acima dos parâmetros definidos).
        * Data e hora atuais (obtidas do RTC).
    * Animação de logo e mensagem de boas-vindas na inicialização.
* **RTC (Real Time Clock):**
    * Utiliza um módulo RTC DS1307 para manter a data e hora, mesmo sem energia.
    * Ajuste inicial da hora com base na hora de compilação do código.
    * Configuração do fuso horário via monitor serial.
* **Data Logger:**
    * Registro automático na EEPROM quando as leituras de luminosidade, temperatura ou umidade estão fora dos limites definidos.
    * Armazenamento de timestamp (Unix), temperatura (inteiro), umidade (inteiro) e luminosidade (inteiro).
    * Capacidade para até 50 logs.
    * Buffer circular: logs mais recentes sobrescrevem os mais antigos quando a memória está cheia.
    * Exibição dos logs via monitor serial.
    * Função para apagar todos os logs da EEPROM, protegida por um simples sistema de login (usuário: `Admin`, senha: `1234`).
* **Alertas Visuais e Sonoros:**
    * LED verde: Status normal (todos os parâmetros dentro dos limites).
    * LED amarelo: Alerta moderado (algum parâmetro fora dos limites "OK", mas dentro de uma faixa de alerta).
    * LED vermelho: Alerta crítico (algum parâmetro significativamente fora dos limites "OK").
    * Buzzer: Ativado em ambos os estados de alerta (moderado e crítico).
* **Interface Serial:**
    * Menu interativo para configurar:
        * Unidade de temperatura (Celsius ou Fahrenheit).
        * Fuso horário (offset em relação ao UTC).
    * Opção para exibir os logs de dados armazenados na EEPROM.
    * Opção para apagar os logs (requer autenticação).
* **Botão de Mudança de HUD:**
    * Um botão conectado a um pino digital permite alternar entre os diferentes modos de visualização no LCD.

## Componentes Necessários

* Placa Arduino.
* Display LCD 16x2 com interface I2C.
* Módulo RTC DS1307 (ou DS3231).
* Sensor de temperatura e umidade DHT11.
* Sensor de luminosidade (LDR).
* Buzzer ativo ou passivo.
* 3 LEDs de cores diferentes (verde, amarelo, vermelho).
* Resistores para os LEDs (geralmente 220Ω).
* Botão momentâneo.
* Jumpers para as conexões.
* Fonte de alimentação para o Arduino.

## Diagrama de Conexão (Exemplo)
Arduino         | Componente
----------------|--------------------------
A0              | Sensor de Luz (pino analógico)
A1              | Buzzer
D2              | Botão HUD
D5              | LED Verde
D6              | LED Amarelo
D7              | LED Vermelho
D12             | DHT11 (pino de dados)
A4 (SDA)        | LCD I2C (SDA) / RTC (SDA)
A5 (SCL)        | LCD I2C (SCL) / RTC (SCL)
5V              | LCD I2C / RTC / DHT11 / LEDs (com resistores) / Sensor de Luz / Buzzer
GND             | LCD I2C / RTC / DHT11 / LEDs / Sensor de Luz / Buzzer / Botão

## Como Usar

1.  **Instale as Bibliotecas:**
    * `RTClib` (para o módulo RTC): Pode ser instalada através do Library Manager na IDE do Arduino (Pesquisar por "RTClib by adafruit").
    * `LiquidCrystal_I2C` (para o display LCD I2C): Também pode ser instalada pelo Library Manager (Pesquisar por "LiquidCrystal I2C by Francisco Malpartida").
    * `DHT sensor library` (para o sensor DHT): Instale via Library Manager (Pesquisar por "DHT sensor library by Adafruit").
    * `Wire` (geralmente já está incluída na IDE do Arduino, necessária para comunicação I2C).
    * `EEPROM` (também geralmente incluída).

2.  **Conecte os Componentes:** Siga o diagrama de conexão (ou ajuste conforme suas necessidades).

3.  **Carregue o Código:** Copie o código fornecido (`.ino`) para a IDE do Arduino e carregue-o na sua placa.

4.  **Monitore a Saída:**
    * O LCD exibirá as leituras dos sensores e o status.
    * Abra o Monitor Serial da IDE do Arduino (geralmente em `Ferramentas > Monitor Serial`) para interagir com o menu de configuração (baud rate de 9600).
    * Use as opções no Monitor Serial para configurar a unidade de temperatura (digite `1` ou `2` e pressione Enter) e o fuso horário (digite o offset UTC, como `-3` para São Paulo, e pressione Enter).
    * A opção `3` no menu principal permite acessar as funcionalidades do Data Logger (exibir logs, apagar logs com login).
    * Pressione o botão conectado ao pino `D2` para alternar entre os diferentes modos de visualização no LCD.

## Configurações no Código

Você pode personalizar as seguintes configurações diretamente no código:

* `tempAdress`, `fusoAdress`, `guardarAtualAdress`, `startAdress`, `tamanhoDeLog`, `maxSaves`, `endAdress`: Endereços e tamanhos relacionados ao armazenamento na EEPROM.
* `usuarioAdm`, `senhaAdm`: Credenciais de administrador para apagar o log.
* `minOkLuz`, `maxOkLuz`, `intervaloAlertaMinLuz`, `intervaloAlertaMaxLuz`: Limites e intervalos de alerta para a luminosidade (em porcentagem).
* `minOkTemp`, `maxOkTemp`, `intervaloAlertaMinTemp`, `intervaloAlertaMaxTemp`: Limites e intervalos de alerta para a temperatura.
* `minOkUmid`, `maxOkUmid`, `intervaloAlertaMinUmid`, `intervaloAlertaMaxUmid`: Limites e intervalos de alerta para a umidade (em porcentagem).
* `DHTPIN`, `DHTTYPE`: Pino e tipo do sensor DHT.
* `buzzer`, `ledVerm`, `ledAmar`, `ledVerd`, `botaoHud`, `sensorLuz`: Pinos conectados aos componentes.
* `lcd(0x27, 16, 2)`: Endereço I2C do LCD (pode variar, verifique o seu módulo com um scanner I2C se necessário).
