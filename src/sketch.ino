#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "DHTesp.h"
#include <time.h>

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
#define FUSO_HORARIO_SEG (-3 * 3600)
#define NTP_TIMEOUT_MS 10000

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
  const char* umidade;
  const char* ocupacao;
  const char* iluminacao;
  const char* ar;
  const char* classificacaoIcs;
  int pontosIcs;
} Classificacoes;

typedef struct {
  int dia;
  int mes;
  int ano;
  int hora;
  int minutos;
  int segundos;
} Data_E_Hora;

DadosAmbiente dados = {};
Alertas alertas = {};
Classificacoes classificacoes = {};
Data_E_Hora dataHoraAtual = {};

int indexHistTemp = 0;
int indexHistUmid = 0;
int indexHistLum = 0;
int indexHistOcup = 0;
int indexHistPresenca = 0;

unsigned long hcsrBuffer[BUFFER_LENGHT] = {0};

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* FIREBASE_URL = "https://senai-led-id-default-rtdb.firebaseio.com";
const char* NTP_SERVIDOR_1 = "pool.ntp.org";
const char* NTP_SERVIDOR_2 = "time.nist.gov";

const char* CAMINHO_DADOS_SENSORES = "/dispositivos/esp32_01/sensores.json";
const char* CAMINHO_ALERTAS = "/dispositivos/esp32_01/alertas.json";
const char* CAMINHO_CLASSIFICACOES = "/dispositivos/esp32_01/classificacoes.json";
const char* CAMINHO_HIST_TEMP = "/dispositivos/esp32_01/historico_temp.json";
const char* CAMINHO_HIST_UMID = "/dispositivos/esp32_01/historico_umidade.json";
const char* CAMINHO_HIST_LUM = "/dispositivos/esp32_01/historico_lum.json";
const char* CAMINHO_HIST_OCUP = "/dispositivos/esp32_01/historico_ocup.json";
const char* CAMINHO_HIST_PRESENCA = "/dispositivos/esp32_01/historico_presenca.json";

const char* HIST_TEMP_FORMAT     = "{\"t%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_UMID_FORMAT     = "{\"u%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_LUM_FORMAT      = "{\"l%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_OCUP_FORMAT     = "{\"o%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_PRESENCA_FORMAT = "{\"p%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";

char bufferGeral[BUFFER_GERAL_LENGTH] = {};
char bufferJson[512] = {};
unsigned long msAnteriorEnvio = 0;
DHTesp dht;
WiFiClientSecure client;

void conectarWiFi();
void enviarDadosParaBanco(const char *caminho, char json[], int tentativas, int msEntreTentativas, int (*httpMetodoEnvio) (HTTPClient*, char*));
int httpPut(HTTPClient *http, char json[]);
int httpPatch(HTTPClient *http, char json[]);
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
const char* classificarUmidade(DadosAmbiente *dados);
const char* classificarOcupacao(DadosAmbiente *dados);
const char* classificarICS(Classificacoes *classificacoes);
const char* classificarAr(DadosAmbiente *dados);
void calcularICS(Classificacoes *classificacoes, DadosAmbiente *dados);
void gerarAlertas(DadosAmbiente *dados, Alertas *alertas);
void gerarJsonAlertas(Alertas *alertas, char buffer[], size_t size);
void gerarJsonClassificacao(Classificacoes *classificacoes, char buffer[], size_t size);
void gerarJsonHistorico(int *index, void* valor, char buffer[], size_t size, void (*voidToX) (void*, char*, size_t), const char* histFormat);
void voidToFloat(void *p, char *buffer, size_t size);
void voidToInt(void *p, char *buffer, size_t size);
void voidToBool(void *p, char *buffer, size_t size);
void voidToString(void *p, char *buffer, size_t size);
void imprimirSerial(Data_E_Hora *dataHora, DadosAmbiente *dados, Classificacoes *classificacoes, Alertas *alertas);
bool atualizarDataHora(Data_E_Hora *dataHora);
void sincronizarHorario();

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
  sincronizarHorario();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (millis() - msAnteriorEnvio >= INTERVALO_DE_ENVIO){
    coletarDadosDoAmbiente(&dados);
    gerarAlertas(&dados, &alertas);
    classificar(&dados, &classificacoes);
    atualizarDataHora(&dataHoraAtual);
    imprimirSerial(&dataHoraAtual, &dados, &classificacoes, &alertas);

    jsonDadosSensores(&dados, bufferJson, BUFFER_GERAL_LENGTH);
    enviarDadosParaBanco(CAMINHO_DADOS_SENSORES, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPut);

    gerarJsonClassificacao(&classificacoes, bufferJson, BUFFER_GERAL_LENGTH);
    enviarDadosParaBanco(CAMINHO_CLASSIFICACOES, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPut);

    gerarJsonAlertas(&alertas, bufferJson, BUFFER_GERAL_LENGTH);
    enviarDadosParaBanco(CAMINHO_ALERTAS, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPut);

    gerarJsonHistorico(&indexHistTemp, &dados.temperatura, bufferJson, BUFFER_GERAL_LENGTH, voidToFloat, HIST_TEMP_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_TEMP, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPatch);

    gerarJsonHistorico(&indexHistOcup, &dados.ocupacao, bufferJson, BUFFER_GERAL_LENGTH, voidToInt, HIST_OCUP_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_OCUP, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPatch);

    gerarJsonHistorico(&indexHistUmid, &dados.umidade, bufferJson, BUFFER_GERAL_LENGTH, voidToFloat, HIST_UMID_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_UMID, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPatch);

    gerarJsonHistorico(&indexHistLum, &dados.luminosidade, bufferJson, BUFFER_GERAL_LENGTH, voidToInt, HIST_LUM_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_LUM, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPatch);

    gerarJsonHistorico(&indexHistPresenca, &dados.presenca, bufferJson, BUFFER_GERAL_LENGTH, voidToBool, HIST_PRESENCA_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_PRESENCA, bufferJson, TENTATIVAS_ENVIO, MS_ENVIO_ENTRE_TENTATIVA, httpPatch);

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
    if(ms - msAnterior >= (unsigned long)intervalo){
        Serial.print(".");
        msAnterior = ms;
    }
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void enviarDadosParaBanco(const char *caminho, char json[], int tentativas, int msEntreTentativas, int (*httpMetodoEnvio) (HTTPClient*, char*)) {
  snprintf(bufferGeral, BUFFER_GERAL_LENGTH, "%s%s", FIREBASE_URL, caminho);
  HTTPClient http;
  int codigo;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

  for(int i = 1; i <= tentativas; i++){
    http.begin(client, bufferGeral);
    http.addHeader("Content-Type", "application/json");
    codigo = httpMetodoEnvio(&http, json);
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

int httpPut(HTTPClient *http, char json[]){
  return http->PUT(json);
}

int httpPatch(HTTPClient *http, char json[]){
  return http->PATCH(json);
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
  classificacoes->umidade = classificarUmidade(dados);
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

const char* classificarUmidade(DadosAmbiente *dados){
  if (dados->umidade < 40){
    return "BAIXA";
  } else if (dados->umidade >= 40 && dados->umidade <= 65){
    return "MEDIA";
  }

  return "ALTA";
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
  }

  if (dados->qualidadeDoAr < 3665){
    a = 100;
  } else if (dados->qualidadeDoAr >= 3665 && dados->qualidadeDoAr <= 3762){
    a = 50;
  } else {
    a = 10;
  }

  classificacoes->pontosIcs = (t + u + l + a + (100 - o)) / 5;
}

void gerarAlertas(DadosAmbiente *dados, Alertas *alertas){
  alertas->alertaAtivo = false;

  if (dados->luminosidade > 1291){
    alertas->iluminacao = "Iluminação insuficiente!";
    alertas->alertaAtivo = true;
  } else {
    alertas->iluminacao = "\0";
  }

  if (dados->qualidadeDoAr > 3762){
    alertas->mq2 = "Nível de gases elevado!";
    alertas->alertaAtivo = true;
  } else {
    alertas->mq2 = "\0";
  }

  if (dados->temperatura > 34){
    alertas->temperatura = "Temperatura elevada!";
    alertas->alertaAtivo = true;
  } else {
    alertas->temperatura = "\0";
  }

  if (dados->ocupacao > 90){
    alertas->ocupacao = "Ocupação elevada!";
    alertas->alertaAtivo = true;
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
  snprintf(buffer, size, "{\"classificacao_ics\":\"%s\",\"ics\":%d,\"luminosidade\":\"%s\",\"ocupacao\":\"%s\",\"qualidade_do_ar\":\"%s\",\"temperatura\":\"%s\",\"umidade\":\"%s\"}",
    classificacoes->classificacaoIcs,
    classificacoes->pontosIcs,
    classificacoes->iluminacao,
    classificacoes->ocupacao,
    classificacoes->ar,
    classificacoes->temperatura,
    classificacoes->umidade
  );
}

void gerarJsonHistorico(int *index, void* valor, char buffer[], size_t size, void (*voidToX) (void*, char*, size_t), const char *histFormat){
  char valorBuffer[10] = "";

  voidToX(valor, valorBuffer, sizeof(valorBuffer));

  *index = (*index < 10) ? *index : 0;

  snprintf(buffer, size, histFormat, *index, valorBuffer);

  (*index)++;
}

void voidToFloat(void *p, char *buffer, size_t size){
  snprintf(buffer, size, "%.1f", *(float*) p);
}

void voidToInt(void *p, char *buffer, size_t size){
  snprintf(buffer, size, "%d", *(int*) p);
}

void voidToBool(void *p, char *buffer, size_t size){
  snprintf(buffer, size, "%s", *(bool*) p ? "true" : "false");
}

void voidToString(void *p, char *buffer, size_t size){
  snprintf(buffer, size, "%s", (char*) p);
}

void imprimirSerial(Data_E_Hora *dataHora, DadosAmbiente *dados, Classificacoes *classificacoes, Alertas *alertas){
  Serial.println("==================================================");
  Serial.println("          SALA DE AULA INTELIGENTE");
  Serial.println("==================================================");
  Serial.printf("Data: %02d/%02d/%04d\n", dataHora->dia, dataHora->mes, dataHora->ano);
  Serial.printf("Hora: %02d:%02d:%02d\n", dataHora->hora, dataHora->minutos, dataHora->segundos);
  Serial.println();

  Serial.println("--- CONFORTO ---");
  Serial.printf("Temperatura: %.1f °C\n", dados->temperatura);
  Serial.printf("Umidade: %.1f %%\n", dados->umidade);
  Serial.println();

  Serial.println("--- ILUMINACAO ---");
  Serial.printf("LDR: %d\n", dados->luminosidade);
  Serial.printf("Classificacao: ILUMINACAO %s\n", classificacoes->iluminacao);
  Serial.println();

  Serial.println("--- PRESENCA ---");
  Serial.printf("PIR: %s\n", dados->presenca ? "DETECTADA" : "NAO DETECTADA");
  Serial.println();

  Serial.println("--- OCUPACAO ---");
  Serial.printf("Distancia: %d cm\n", dados->distancia);
  Serial.printf("Ocupacao estimada: %d %%\n", dados->ocupacao);
  Serial.printf("Classificacao: %s\n", classificacoes->ocupacao);
  Serial.println();

  Serial.println("--- QUALIDADE DO AR ---");
  Serial.printf("MQ2: %d\n", dados->qualidadeDoAr);
  Serial.printf("Status: %s\n", classificacoes->ar);
  Serial.println();

  Serial.println("--- INDICE DE CONFORTO ---");
  Serial.printf("ICS: %d\n", classificacoes->pontosIcs);
  Serial.printf("Classificacao: %s\n", classificacoes->classificacaoIcs);
  Serial.println();

  Serial.println("--- ALERTAS ---");
  if (!alertas->alertaAtivo){
    Serial.println("Nenhum alerta ativo.");
    return;
  }

  if (alertas->temperatura && alertas->temperatura[0] != '\0') Serial.printf("⚠ ALERTA: %s\n", alertas->temperatura);
  if (alertas->iluminacao && alertas->iluminacao[0] != '\0') Serial.printf("⚠ ALERTA: %s\n", alertas->iluminacao);
  if (alertas->ocupacao && alertas->ocupacao[0] != '\0') Serial.printf("⚠ ALERTA: %s\n", alertas->ocupacao);
  if (alertas->mq2 && alertas->mq2[0] != '\0') Serial.printf("⚠ ALERTA: %s\n", alertas->mq2);
}

bool atualizarDataHora(Data_E_Hora *dataHora){
  time_t agora = time(NULL);
  struct tm info;
  localtime_r(&agora, &info);

  if (info.tm_year < (2016 - 1900)){
    return false;
  }

  dataHora->dia = info.tm_mday;
  dataHora->mes = info.tm_mon + 1;
  dataHora->ano = info.tm_year + 1900;
  dataHora->hora = info.tm_hour;
  dataHora->minutos = info.tm_min;
  dataHora->segundos = info.tm_sec;

  return true;
}

void sincronizarHorario(){
  configTime(FUSO_HORARIO_SEG, 0, NTP_SERVIDOR_1, NTP_SERVIDOR_2);

  Serial.print("Sincronizando horario (NTP)");

  unsigned long inicio = millis();
  unsigned long ms = 0;
  unsigned long msAnterior = 0;
  int intervalo = 500;

  while (!atualizarDataHora(&dataHoraAtual) && (millis() - inicio) < NTP_TIMEOUT_MS) {
    ms = millis();
    if(ms - msAnterior >= (unsigned long)intervalo){
        Serial.print(".");
        msAnterior = ms;
    }
  }

  if (atualizarDataHora(&dataHoraAtual)){
    Serial.println("\nHorario sincronizado!");
  } else {
    Serial.println("\nFalha ao sincronizar o horario.");
  }
}