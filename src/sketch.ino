#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "DHTesp.h"

#define DHT_PIN 15
#define PIR_PIN 17
#define HCSR_PIN_TRIG 5
#define HCSR_PIN_ECHO 19
#define LDR_PIN 34
#define MQ2_PIN 35
#define BUFFER_LENGHT 5
#define DISTANCIA_MAX 400
#define DISTANCIA_MIN 50
#define TENTATIVAS_ENVIO 5
#define MS_ENVIO_ENTRE_TENTATIVA 1000

typedef struct {
  int distancia;
  float umidade;
  float temperatura;
  int luminosidade;
  bool presenca;
  int qualidadeDoAr;
  int ocupacao;
} DadosAmbiente;

typedef struct {
  bool mq2;
  bool ocupacao;
  bool iluminacao;
  bool temperatura;
} Alertas;


DadosAmbiente dados = {}; 
Alertas alertas = {};

unsigned long hcsrBuffer[BUFFER_LENGHT] = {0};

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* CAMINHO_DADOS_SENSORES = "/dispositivos/esp32_01/sensores.json";
const char* FIREBASE_URL = "https://senai-led-id-default-rtdb.firebaseio.com";
DHTesp dht;
WiFiClientSecure client;


void conectarWiFi();
void enviarDadosParaBanco(const char *caminho, String json, int tentativas, int msEntreTentativas);
unsigned long mediana(unsigned long buffer[], size_t size);
void preencherBufferDistancia(unsigned long buffer[], int pinTrig, int pinEcho, int repeticoes);
unsigned long lerHCSR04(int pinTrig, int pinEcho);
void lerDHT(DadosAmbiente *dados);
bool lerPIR(int pirPin);
int lerSensorMQ2(int mq2Pin);
int lerSensorLDR(int ldrPin);
void coletarDadosDoAmbiente(DadosAmbiente *dados);
void enviarDadosDoSensores(DadosAmbiente *dados);
String jsonDadosSensores(DadosAmbiente *dados);

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);
  pinMode(HCSR_PIN_TRIG, OUTPUT);
  pinMode(HCSR_PIN_ECHO, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);
  digitalWrite(HCSR_PIN_TRIG, LOW);
  dht.setup(DHT_PIN, DHTesp::DHT22);
  conectarWiFi();
  client.setInsecure();

}



void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  coletarDadosDoAmbiente(&dados);
  enviarDadosParaBanco(CAMINHO_DADOS_SENSORES, jsonDadosSensores(&dados), TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA);
  delay(5000);
}

void conectarWiFi() {
  
  Serial.print("Conectando ao WiFi");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  unsigned long ms = millis();
  unsigned long msAnterior = 0;
  int intervalo = 500;

  while (WiFi.status() != WL_CONNECTED) {
    if(ms - msAnterior >= intervalo){
        Serial.print(".");
        msAnterior = millis();
    }
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void enviarDadosParaBanco(const char *caminho, String json, int tentativas, int msEntreTentativas) {
  String url = String(FIREBASE_URL) + caminho;
  HTTPClient http;
  int codigo;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

    for(int i = 1; i <= tentativas; i++){
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");
        codigo = http.PUT(json);
        http.end();

        if (codigo >= 200 && codigo < 300) {
          Serial.println("\nDados enviados:");
          Serial.println(json);
          return;
        }

        Serial.print("Erro ao enviar dados. HTTP: ");
        Serial.println(codigo);
        client.stop();

       if(i < tentativas) delay(msEntreTentativas);
    }
}

unsigned long mediana(unsigned long buffer[], size_t size) {
  unsigned long value = 0;

  for (size_t i = 1; i < size; i++) {
    value = buffer[i];
    for (int x = i - 1; x >= 0; x--) {
      if (value < buffer[x]) {
        buffer[x + 1] = buffer[x];
        buffer[x] = value;
      }
    }
  }

  return buffer[size / 2];
}

void preencherBufferDistancia(unsigned long buffer[], int pinTrig, int pinEcho, int repeticoes) {
  unsigned long duracao = 0;

  for (int i = 0; i < repeticoes; i++) {
    duracao = lerHCSR04(pinTrig, pinEcho);
    delay(60);

    if (duracao > 0) {
      buffer[i] = duracao / 58;
    } else {
      buffer[i] = DISTANCIA_MAX;   
    }
  }
}

unsigned long lerHCSR04(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);

  return pulseIn(pinEcho, HIGH, 30000);
}


void lerDHT(DadosAmbiente *dados) {
  TempAndHumidity temperaturaeUmidade = dht.getTempAndHumidity();

  if (isnan(temperaturaeUmidade.temperature) || isnan(temperaturaeUmidade.humidity)) {
    Serial.println("Erro ao ler DHT22");
    return;
  }

  dados->temperatura = temperaturaeUmidade.temperature;
  dados->umidade = temperaturaeUmidade.humidity;
}

bool lerPIR(int pirPin) {
  return digitalRead(pirPin);
}

int lerSensorMQ2(int mq2Pin) {
  return analogRead(mq2Pin);
}

int lerSensorLDR(int ldrPin) {
  return analogRead(ldrPin);
}


void coletarDadosDoAmbiente(DadosAmbiente *dados) {
  lerDHT(dados);
  preencherBufferDistancia(hcsrBuffer, HCSR_PIN_TRIG, HCSR_PIN_ECHO, BUFFER_LENGHT);
  dados->presenca = lerPIR(PIR_PIN);
  dados->distancia = mediana(hcsrBuffer, BUFFER_LENGHT);
  dados->qualidadeDoAr = lerSensorMQ2(MQ2_PIN);
  dados->luminosidade = lerSensorLDR(LDR_PIN);

  if (dados->presenca) {
    dados->ocupacao = constrain(map(dados->distancia, DISTANCIA_MAX, DISTANCIA_MIN, 0, 100), 0, 100);
  } else {
    dados->ocupacao = 0;
  }
}

String jsonDadosSensores(DadosAmbiente *dados) {
  String json = "{";
  json += "\"temperatura\":" + String(dados->temperatura, 1) + ",";
  json += "\"umidade\":" + String(dados->umidade, 1) + ",";
  json += "\"luminosidade\":" + String(dados->luminosidade) + ",";
  json += "\"presenca\":" + String(dados->presenca ? "true" : "false") + ",";
  json += "\"distancia\":" + String(dados->distancia) + ",";
  json += "\"ocupacao\":" + String(dados->ocupacao) + ",";
  json += "\"mq2\":" + String(dados->qualidadeDoAr);
  json += "}";

  return json;
}



