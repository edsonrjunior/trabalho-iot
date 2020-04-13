#include "WiFiEsp.h"
#include <LiquidCrystal.h>

//VARIAVEIS DO SHIELD WIFI
//Nome da rede wifi
char ssid[] = "Kindle";      

//Senha da rede wifi
char pass[] = "**090029**";  

//STATUS TEMPORÁRIO ATRIBUÍDO QUANDO O WIFI É INICIALIZADO E PERMANECE ATIVO ATÉ QUE O NÚMERO DE TENTATIVAS EXPIRE (RESULTANDO EM WL_NO_SHIELD) OU QUE UMA CONEXÃO SEJA ESTABELECIDA (RESULTANDO EM WL_CONNECTED)
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

//DEMAIS VARIÁVEIS

//Led de status da conexão wifi.
int wifiLedRed = 9;
int wifiLedGreen = 8;

//Buzzer
int buzzer = 10;

void setup() {
  Serial.begin(9600);

  pinMode(wifiLedGreen, OUTPUT);
  pinMode(wifiLedRed, OUTPUT);

//Ligar led vermelho do wifi
  digitalWrite(wifiLedRed, HIGH);

//Conectar a rede wifi
  conectaAoWifi();

//Calibrar sensor MQ-9
  calibrarMq9();
}

void loop() {

  exibirNoDisplay();

  WiFiEspClient client = server.available(); 
  if (client) {                             
    buf.init();                             
    boolean currentLineIsBlank = true;
    while (client.connected()) {             
      if (client.available()) {             
        char c = client.read();             
        Serial.write(c);
        buf.push(c);                        
        if (c == '\n' && currentLineIsBlank) {
    
          client.print("<!DOCTYPE HTML>\r\n");
          client.print("<html>\r\n");
          client.print("<head>\r\n");
          client.print("<meta charset=""UTF-8"">\r\n");
          client.print("</head>\r\n");
          client.print("<body>\r\n");

          client.print("<br>\r\n");

          client.print("Nivel de Gas    :  ");
          client.print(iPPM_LPG);
          client.print(" ppm");
          client.print("<br>\r\n");

          client.print("Nivel de CO     : ");
          client.print(iPPM_CO);
          client.print(" ppm");
          client.print("<br>\r\n");

          client.print("Nível de Fumaca :");
          client.print(iPPM_Smoke);
          client.print(" ppm");
          client.print("<br>\r\n");

          client.print("</body>\r\n");
          client.print("</html>\r\n");
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(10);
    // client.stop(); //FINALIZA A REQUISIÇÃO HTTP E DESCONECTA O CLIENTE
    Serial.println("Cliente disconectado");
  }
}

void calibrarMq9() {
  lcd.begin(20, 4);
  pinMode(calibrationLed, OUTPUT);
  digitalWrite(calibrationLed, HIGH);
  lcd.print("Calibrando...");
  Ro = MQCalibration(MQ_PIN);
  digitalWrite(calibrationLed, LOW);
  lcd.print("Pronto!");
  lcd.setCursor(0, 1);
  lcd.print("Ro= ");
  lcd.print(Ro);
  lcd.print("kohm");
  delay(3000);
}

void conectaAoWifi() {
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

long  MQGetPercentage(float rs_ro_ratio, float *pcurve) {
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


//Alarme do buzzer
void soarAlarme() {
  unsigned char i;
  while (1) {
    //Frequência 1
    for (i = 0; i < 80; i++) {
      digitalWrite (buzzer, HIGH) ;
      delay (1) ;
      digitalWrite (buzzer, LOW) ;
      delay (1) ;
    }
    //Frequência 2
    for (i = 0; i < 100; i++)    {
      digitalWrite (buzzer, HIGH) ;
      delay (2) ;
      digitalWrite (buzzer, LOW) ;
      delay (2) ;
    }
  }
}
