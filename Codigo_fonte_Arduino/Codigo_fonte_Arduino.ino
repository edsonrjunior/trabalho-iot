#include "WiFiEsp.h"
#include <LiquidCrystal.h>

//VARIAVEIS DO SHIELD WIFI
//Nome da rede wifi
char ssid[] = "Kindle";

//Senha da rede wifi
char pass[] = "****";

//Status da conexão wifi
int status = WL_IDLE_STATUS;

//CONEXÃO REALIZADA NA PORTA 80
WiFiEspServer server(80);

//BUFFER PARA AUMENTAR A VELOCIDADE E REDUZIR A ALOCAÇÃO DE MEMÓRIA
RingBuffer buf(8);


//VARIAVEIS DO SENSOR MQ-9
const int calibrationLed = 13;
const int MQ_PIN = A0;
int RL_VALUE = 5;
float RO_CLEAN_AIR_FACTOR = 9.83;

long iPPM_LPG = 0;
long iPPM_CO = 0;
long iPPM_Smoke = 0;

long sensorValueTest = 0;

int CALIBARAION_SAMPLE_TIMES = 50;
int CALIBRATION_SAMPLE_INTERVAL = 500;
int READ_SAMPLE_INTERVAL = 50;
int READ_SAMPLE_TIMES = 5;

#define GAS_LPG   0
#define GAS_CO    1
#define GAS_SMOKE 2

float LPGCurve[3]   = {2.3, 0.21, -0.47};
float COCurve[3]    = {2.3, 0.72, -0.34};
float SmokeCurve[3] = {2.3, 0.53, -0.44};
float Ro            = 10;

LiquidCrystal lcd(12, 11, 5, 4, 3 , 2 );

//Pinos da fan, do alarme e dos leds vermelho e verde.
const int wifiLedRed = 9;
const int wifiLedGreen = 8;
const int alarme = 10;
const int rele = 6;

void setup() {
  Serial.begin(9600);

  pinMode(wifiLedGreen, OUTPUT);
  pinMode(wifiLedRed, OUTPUT);
  pinMode(rele, OUTPUT);
  pinMode(alarme, OUTPUT);

  //Definir alarme e fan como desligados.
  digitalWrite(rele, LOW);
  digitalWrite(alarme, LOW);

  //Ligar led vermelho do wifi
  digitalWrite(wifiLedRed, HIGH);

  //Conectar a rede wifi
  conectaAoWifi();

  //Calibrar sensor MQ-9
  calibrarMq9();
}

void loop() {

  exibirNoDisplay();

  if (iPPM_LPG > 0) {
    digitalWrite(rele, HIGH);
    tone(alarme, 1000);
    delay(1000);
    noTone(alarme);
    delay(1000);
  } else {
    digitalWrite(rele, LOW);
  }


  WiFiEspClient client = server.available();
  if (client) {
    buf.init();
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        buf.push(c);

        //IDENTIFICA O FIM DA REQUISIÇÃO HTTP E ENVIA UMA RESPOSTA
        if (buf.endsWith("\r\n\r\n")) {
          sendHttpResponse(client);
          break;
        }
        if (buf.endsWith("GET /A")) {
          sendHttpResponse(client);
          break;
        }
      }
    }
    client.stop(); //FINALIZA A REQUISIÇÃO HTTP E DESCONECTA O CLIENTE
  }
}

void sendHttpResponse(WiFiEspClient client) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("\r\n");
  client.print("<!DOCTYPE HTML>\r\n");
  client.print("<html>\r\n");

  client.print("<head>");
  client.print("<meta charset=\"UTF-8\">\r\n");
  client.println("<link rel='stylesheet' type='text/css' href='https://blogmasterwalkershop.com.br/arquivos/artigos/sub_wifi/webpagecss.css' />");
  client.print("<title>Projeto Iot - Sensor de gás, fumaça e CO2</title>\r\n");
  client.print("</head>\r\n");
  client.print("<body>\r\n");

  client.print("<header><h1>Projeto Iot - Sensor de gás, fumaça e CO2</h1></header>\r\n");
  client.print("<main>\r\n");

  client.print("<aside>\r\n");
  client.print("NÍVEL DE GÁS    :  ");
  client.print(iPPM_LPG);
  client.print(" PPM");
  client.print("<br>\r\n");

  client.print("NÍVEL DE CO2     : ");
  client.print(iPPM_CO);
  client.print(" PPM");
  client.print("<br>\r\n");

  client.print("NÍVEL DE FUMAÇA :");
  client.print(iPPM_Smoke);
  client.print(" PPM");
  client.print("<br>\r\n");
  client.print("</aside>\r\n");

  client.print("<br>\r\n");
  client.print("Status do FAN");

  //Status da fan
  if (rele == HIGH) {
    client.print("<p style='line-height:2'><font color='green'>LIGADO</font></p>\r\n"); //ESCREVE "LIGADO" NA PÁGINA
  } else {
    client.print("<p style='line-height:2'><font color='red'>DESLIGADO</font></p>\r\n"); //ESCREVE "DESLIGADO" NA PÁGINA)
  }

  client.print("Status do Alarme");

  //Status do Alarme
  if (alarme == HIGH) {
    client.print("<p style='line-height:2'><font color='green'>LIGADO</font></p>\r\n"); //ESCREVE "LIGADO" NA PÁGINA
  } else {
    client.print("<p style='line-height:2'><font color='red'>DESLIGADO</font></p>\r\n"); //ESCREVE "DESLIGADO" NA PÁGINA)
  }

  client.print("<a href=\"/A\"> Atualizar</a>");

  client.print("</main>\r\n");

  client.print("<footer>\r\n");
  client.print("<p>O código do projeto você encontra em: https://github.com/edsonrjunior/trabalho-iot</p>\r\n");
  client.print("</footer>");

  client.print("</body>\r\n");
  client.print("</html>\r\n");
  delay(1); //INTERVALO DE 1 MILISSEGUNDO
}

void calibrarMq9() {
  lcd.begin(20, 4);
  pinMode(calibrationLed, OUTPUT);
  digitalWrite(calibrationLed, HIGH);
  lcd.print("Calibrando...");
  Serial.println("Calibrando sensor MQ...");
  Ro = MQCalibration(MQ_PIN);
  digitalWrite(calibrationLed, LOW);
  lcd.print("Pronto!");
  Serial.println("Sensor MQ calibrado.");
  lcd.setCursor(0, 1);
  lcd.print("Ro= ");
  lcd.print(Ro);
  lcd.print("kohm");
  delay(3000);
}

void conectaAoWifi() {

  Serial.println("Conectando ao WiFi...");

  //Inicializa a comunicação serial com shield WiFi ESP8266
  WiFi.init(&Serial);

  //DEFINICAO DO IP DA PLACA WIFI
  WiFi.config(IPAddress(192, 168, 0, 110));

  //Verificar se o shield está conectado ao arduino.
  if (WiFi.status() == WL_NO_SHIELD) {
    while (true);
  }
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
  }

  //Mudar a cor do led. verde = conectado, vermelho = desconectado.
  if (status == WL_CONNECTED) {
    digitalWrite(wifiLedRed, LOW);
    digitalWrite(wifiLedGreen, HIGH);
    Serial.println("Conectado ao WiFi   .");
  } else {
    digitalWrite(wifiLedGreen, LOW);
    digitalWrite(wifiLedRed, HIGH);
  }
  server.begin();
  delay(3000);
}

//Método para calibrar o sensor MQ-9.
float MQCalibration(int mq_pin) {
  int i;
  float val = 0;

  for (i = 0; i < CALIBARAION_SAMPLE_TIMES; i++) {
    val += MQResistanceCalculation(analogRead(mq_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val / CALIBARAION_SAMPLE_TIMES;
  val = val / RO_CLEAN_AIR_FACTOR;
  return val;
}

float MQResistanceCalculation(int raw_adc) {
  return ( ((float)RL_VALUE * (1023 - raw_adc) / raw_adc));
}

//Método de leitura do sensor MQ-9.
float MQRead(int mq_pin) {
  int i;
  float rs = 0;

  for (i = 0; i < READ_SAMPLE_TIMES; i++) {
    rs += MQResistanceCalculation(analogRead(mq_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
  rs = rs / READ_SAMPLE_TIMES;
  return rs;
}

//Obter o percentual de gás
long MQGetGasPercentage(float rs_ro_ratio, int gas_id) {
  if ( gas_id == GAS_LPG ) {
    return MQGetPercentage(rs_ro_ratio, LPGCurve);
  } else if ( gas_id == GAS_CO ) {
    return MQGetPercentage(rs_ro_ratio, COCurve);
  } else if ( gas_id == GAS_SMOKE ) {
    return MQGetPercentage(rs_ro_ratio, SmokeCurve);
  }
  return 0;
}

long  MQGetPercentage(float rs_ro_ratio, float * pcurve) {
  return (pow(10, ( ((log(rs_ro_ratio) - pcurve[1]) / pcurve[2]) + pcurve[0])));
}

//Método para exibir no monitor LCD os valores detectados pelo sensor.
void exibirNoDisplay() {
  iPPM_LPG = 0;
  iPPM_CO = 0;
  iPPM_Smoke = 0;

  iPPM_LPG = MQGetGasPercentage(MQRead(MQ_PIN) / Ro, GAS_LPG);
  iPPM_CO = MQGetGasPercentage(MQRead(MQ_PIN) / Ro, GAS_CO);
  iPPM_Smoke = MQGetGasPercentage(MQRead(MQ_PIN) / Ro, GAS_SMOKE);

  lcd.clear();
  lcd.setCursor( 0 , 0 );
  lcd.print("Concentracao de gas ");

  lcd.setCursor( 0 , 1 );
  lcd.print("LPG: ");
  lcd.print(iPPM_LPG);
  lcd.print(" ppm");

  lcd.setCursor( 0, 2 );
  lcd.print("CO: ");
  lcd.print(iPPM_CO);
  lcd.print(" ppm");

  lcd.setCursor( 0, 3 );
  lcd.print("Smoke: ");
  lcd.print(iPPM_Smoke);
  lcd.print(" ppm");
}


//Soar alarme
void soarAlarme() {
  unsigned char i;
  while (1) {
    //Frequência 1
    for (i = 0; i < 80; i++) {
      digitalWrite (alarme, HIGH) ;
      delay (1) ;
      digitalWrite (alarme, LOW) ;
      delay (1) ;
    }
    //Frequência 2
    for (i = 0; i < 100; i++)    {
      digitalWrite (alarme, HIGH) ;
      delay (2) ;
      digitalWrite (alarme, LOW) ;
      delay (2) ;
    }
  }
}
