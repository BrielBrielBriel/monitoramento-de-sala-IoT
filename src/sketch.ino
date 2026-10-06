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
#define BUFFER_GERAL_LENGTH 512
#define DISTANCIA_MAX 400
#define DISTANCIA_MIN 50
#define TENTATIVAS_ENVIO 5
#define INTERVALO_DE_ENVIO 5000
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
  const char* mq2;
  const char* ocupacao;
  const char* iluminacao;
  const char* temperatura;
  bool alertaAtivo;
} Alertas;

typedef struct {
  const char* temperatura;
  const char* ocupacao;
  const char* iluminacao;
  const char* ar;
  const char* classificacaoIcs;
  
  int pontosIcs;
} Classificacoes;

DadosAmbiente dados = {}; 
Alertas alertas = {};
Classificacoes classificacoes = {};

unsigned long hcsrBuffer[BUFFER_LENGHT] = {0};

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* FIREBASE_URL = "https://senai-led-id-default-rtdb.firebaseio.com";
const char* CAMINHO_DADOS_SENSORES = "/dispositivos/esp32_01/sensores.json";
const char* CAMINHO_ALERTAS = "/dispositivos/esp32_01/alertas.json";
const char* CAMINHO_CLASSIFICACOES = "/dispositivos/esp32_01/classificacoes.json";
char bufferGeral[BUFFER_GERAL_LENGTH] = {};
char bufferJson[512] = {};
unsigned long msAnteriorEnvio = 0;
DHTesp dht;
WiFiClientSecure client;

void conectarWiFi();
void enviarDadosParaBanco(const char *caminho, char json[], int tentativas, int msEntreTentativas);
unsigned long mediana(unsigned long buffer[], size_t size);
void preencherBufferDistancia(unsigned long buffer[], int pinTrig, int pinEcho, int repeticoes);
unsigned long lerHCSR04(int pinTrig, int pinEcho);
void lerDHT(DadosAmbiente *dados);
bool lerPIR(int pirPin);
int lerSensorMQ2(int mq2Pin);
int lerSensorLDR(int ldrPin);
void coletarDadosDoAmbiente(DadosAmbiente *dados);
void gerarAvisos(DadosAmbiente *dados, Alertas *alertas);
void jsonDadosSensores(DadosAmbiente *dados, char buffer[], size_t size);
void classificar(DadosAmbiente *dados, Classificacoes *classificacoes);
const char* classificarIluminacao(DadosAmbiente *dados);
const char* classificarTemperatura(DadosAmbiente *dados);
const char* classificarOcupacao(DadosAmbiente *dados);
const char* classificarICS(Classificacoes *classificacoes);
const char* classificarAr(DadosAmbiente *dados);
void calcularICS(Classificacoes *classificacoes, DadosAmbiente *dados);
void gerarAlertas(DadosAmbiente *dados, Alertas *alertas);
void gerarJsonAlertas(Alertas *alertas, char buffer[], size_t size);
void gerarJsonClassificacao(Classificacoes *classificacoes, char buffer[], size_t size);

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
  unsigned long ms = millis();

  if (millis() - msAnteriorEnvio >= INTERVALO_DE_ENVIO){
    coletarDadosDoAmbiente(&dados);
    gerarAlertas(&dados, &alertas);
    classificar(&dados, &classificacoes);
    jsonDadosSensores(&dados, bufferJson, BUFFER_GERAL_LENGTH);
    enviarDadosParaBanco(CAMINHO_DADOS_SENSORES, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA);
    gerarJsonAlertas(&alertas, bufferJson, BUFFER_GERAL_LENGTH);
    enviarDadosParaBanco(CAMINHO_ALERTAS, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA);
    gerarJsonClassificacao(&classificacoes, bufferJson, BUFFER_GERAL_LENGTH);
    enviarDadosParaBanco(CAMINHO_CLASSIFICACOES, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA);

    msAnteriorEnvio = millis();
  }
}

void conectarWiFi() {
  Serial.print("Conectando ao WiFi");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  unsigned long ms = 0;
  unsigned long msAnterior = 0;
  int intervalo = 500;

  while (WiFi.status() != WL_CONNECTED) {
    ms = millis();
    if(ms - msAnterior >= intervalo){
        Serial.print(".");
        msAnterior = ms;
    }
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void enviarDadosParaBanco(const char *caminho, char json[], int tentativas, int msEntreTentativas) {
  snprintf(bufferGeral, BUFFER_GERAL_LENGTH, "%s%s", FIREBASE_URL, caminho);
  HTTPClient http;
  int codigo;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

    for(int i = 1; i <= tentativas; i++){
        http.begin(client, bufferGeral);
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

void classificar(DadosAmbiente *dados, Classificacoes *classificacoes){
    classificacoes->temperatura = classificarTemperatura(dados);
    classificacoes->iluminacao = classificarIluminacao(dados);
    classificacoes->ocupacao = classificarOcupacao(dados);
    classificacoes->classificacaoIcs = classificarICS(classificacoes);
    classificacoes->ar = classificarAr(dados);
    calcularICS(classificacoes, dados);
}

const char* classificarIluminacao(DadosAmbiente *dados){
    if (dados->luminosidade < 798){
      return "INSUFICIENTE";
    } else if (dados->luminosidade >= 798 && dados->luminosidade <= 1291){
      return "ADEQUADA";
    } 
    
    return "INTENSA";
}

const char* classificarTemperatura(DadosAmbiente *dados){
    if (dados->temperatura < 18){
     return "FRIA";
    } else if (dados->temperatura >= 18 && dados->temperatura <= 28){
      return "NORMAL";

   } 
    return "QUENTE";
}

const char* classificarOcupacao(DadosAmbiente *dados){
    if (dados->ocupacao >= 0 && dados->ocupacao <= 30){
        return "BAIXA";
      } else if (dados->ocupacao >= 31 && dados->ocupacao <= 70){
        return "MODERADA";
      } 
      return "ALTA";
}

const char* classificarAr(DadosAmbiente *dados){
  if (dados->qualidadeDoAr < 3665){
    return "NORMAL";
  } else if (dados->qualidadeDoAr >= 3665 && dados->qualidadeDoAr <= 3762){
    return "REGULAR";
  } 
  
  return "RUIM";
  
}
const char* classificarICS(Classificacoes *classificacoes){
    if (classificacoes->pontosIcs < 50){
      return "RUIM";
    } else if (classificacoes->pontosIcs >= 50 && classificacoes->pontosIcs <= 74){
      return "MEDIANA";
    }
    
    return "BOA";
}


void jsonDadosSensores(DadosAmbiente *dados, char buffer[], size_t size) {
    snprintf(buffer, size,
      "{\"temperatura\":%.1f,\"umidade\":%.1f,\"luminosidade\":%d,\"presenca\":%s,\"distancia\":%d,\"ocupacao\":%d,\"mq2\":%d}",
      dados->temperatura,
      dados->umidade,
      dados->luminosidade,
      dados->presenca ? "true" : "false",
      dados->distancia,
      dados->ocupacao,
      dados->qualidadeDoAr
    );
}

void calcularICS(Classificacoes *classificacoes, DadosAmbiente *dados){
  int t = (dados->temperatura >= 18 && dados->temperatura <= 28) ? 100 : 20;
  int l = (dados->luminosidade >= 798 && dados->luminosidade <= 1291) ? 100 : 20;
  int a = 0;
  int u = 0;
  int o = dados->ocupacao;

  if (dados->umidade >= 40 && dados->umidade <= 65){
    u = 100;
  } else if (dados->umidade >= 66 && dados->umidade <= 70){
    u = 70;
  } else {
    u = 10;
  } if (dados->qualidadeDoAr < 3665){
    a = 100;
  } else if (dados->qualidadeDoAr >= 3665 && dados->qualidadeDoAr <= 3762){
    a = 50;
  } else {
    a = 10;
  }

  classificacoes->pontosIcs = (t + u + l + a + (100 - o)) / 5;

}

void gerarAlertas(DadosAmbiente *dados, Alertas *alertas){
  if (dados->luminosidade > 1291){
    alertas->iluminacao = "Iluminação insuficiente!";
  } else {
    alertas->iluminacao = "\0";
  } if (dados->qualidadeDoAr > 3762){
    alertas->mq2 = "Nível de gases elevado!";
  } else {
    alertas->mq2 = "\0";
  } if (dados->temperatura > 34){
    alertas->temperatura = "Temperatura elevada!";
  } else {
    alertas->temperatura = "\0";
  } if (dados->ocupacao > 90){
    alertas->ocupacao = "Ocupação elevada!";
  } else {
    alertas->ocupacao = "\0";
  }
}

void gerarJsonAlertas(Alertas *alertas, char buffer[], size_t size){
  snprintf(buffer, size, "{\"iluminacao\":\"%s\",\"mq2\":\"%s\",\"ocupacao\":\"%s\",\"temperatura\":\"%s\",\"alerta_ativo\":%s}", 
    alertas->iluminacao,
    alertas->mq2, 
    alertas->ocupacao, 
    alertas->temperatura,
    alertas->alertaAtivo ? "true" : "false"
  );
}
void gerarJsonClassificacao(Classificacoes *classificacoes, char buffer[], size_t size){
    snprintf(buffer, size, "{\"classificacao_ics\":\"%s\",\"ics\":%d,\"luminosidade\":\"%s\",\"ocupacao\":\"%s\",\"qualidade_do_ar\":\"%s\",\"temperatura\":\"%s\"}", 
    classificacoes->classificacaoIcs,
    classificacoes->pontosIcs, 
    classificacoes->iluminacao,
    classificacoes->ocupacao,
    classificacoes->ar,
    classificacoes->temperatura
  );
}