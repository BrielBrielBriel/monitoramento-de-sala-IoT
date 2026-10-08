#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "DHTesp.h"
#include <time.h>

// MACROS E DEFINIÇÕES DE PINOS
#define PINO_DHT 15
#define PINO_PIR 17
#define PINO_HCSR_TRIG 5
#define PINO_HCSR_ECHO 19
#define PINO_LDR 34
#define PINO_MQ2 35

// PARÂMETROS DE FUNCIONAMENTO
#define TAM_BUFFER_DISTANCIA 5
#define TAM_BUFFER_GERAL 1024
#define DISTANCIA_MAXIMA 400
#define DISTANCIA_MINIMA 50
#define MAX_TENTATIVAS_ENVIO 5
#define INTERVALO_DE_ENVIO_MS 5000
#define ATRASO_ENTRE_TENTATIVAS_MS 1000
#define FUSO_HORARIO_SEGUNDOS (-3 * 3600)
#define TEMPO_LIMITE_NTP_MS 10000

// ESTRUTURAS DE DADOS (TYPES)

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
} DataHora;

// CONSTANTES GLOBAIS 

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* FIREBASE_URL = "https://senai-led-id-default-rtdb.firebaseio.com";
const char* NTP_SERVIDOR_1 = "pool.ntp.org";
const char* NTP_SERVIDOR_2 = "time.nist.gov";

// Caminhos do banco de dados
const char* CAMINHO_DADOS = "/dispositivos/esp32_01.json"; // CORRIGIDO: Adicionado ponto e vírgula ausente
const char* CAMINHO_ALERTAS = "/dispositivos/esp32_01/alertas.json";
const char* CAMINHO_CLASSIFICACOES = "/dispositivos/esp32_01/classificacoes.json";
const char* CAMINHO_HIST_TEMP = "/dispositivos/esp32_01/historico_temp.json";
const char* CAMINHO_HIST_UMID = "/dispositivos/esp32_01/historico_umidade.json";
const char* CAMINHO_HIST_LUM = "/dispositivos/esp32_01/historico_lum.json";
const char* CAMINHO_HIST_OCUP = "/dispositivos/esp32_01/historico_ocup.json";
const char* CAMINHO_HIST_PRESENCA = "/dispositivos/esp32_01/historico_presenca.json";

// Formatos JSON para o histórico
const char* HIST_TEMP_FORMAT     = "{\"t%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_UMID_FORMAT     = "{\"u%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_LUM_FORMAT      = "{\"l%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_OCUP_FORMAT     = "{\"o%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";
const char* HIST_PRESENCA_FORMAT = "{\"p%d\":{\"timestamp\":{\".sv\":\"timestamp\"},\"valor\":%s}}";

// VARIÁVEIS GLOBAIS
DadosAmbiente dadosAmbiente = {};
Alertas alertas = {};
Classificacoes classificacoes = {};
DataHora dataHoraAtual = {};

int indiceHistTemp = 0;
int indiceHistUmid = 0;
int indiceHistLum = 0;
int indiceHistOcup = 0;
int indiceHistPresenca = 0;

unsigned long bufferDistancia[TAM_BUFFER_DISTANCIA] = {0};
char bufferRequisicao[TAM_BUFFER_GERAL] = {};
char bufferDadosJson[1024] = {}; // Ajustado para 1024 conforme calculado
unsigned long tempoUltimoEnvio = 0;

DHTesp dht;
WiFiClientSecure client;

// PROTÓTIPOS DAS FUNÇÕES

void conectarWiFi();
void enviarDadosParaBanco(const char *caminho, char json[], int tentativas, int msEntreTentativas, int (*metodoHttp) (HTTPClient*, char*));
int httpPut(HTTPClient *http, char json[]);
int httpPatch(HTTPClient *http, char json[]);

// Leitura de Sensores e Tratamento
unsigned long calcularMediana(unsigned long buffer[], size_t tamanho);
void preencherBufferDistancia(unsigned long buffer[], int pinoTrig, int pinoEcho, int repeticoes);
unsigned long lerHCSR04(int pinoTrig, int pinoEcho);
void lerDHT(DadosAmbiente *dados);
bool lerPIR(int pinoPir);
int lerSensorMQ2(int pinoMq2);
int lerSensorLDR(int pinoLdr);
void coletarDadosDoAmbiente(DadosAmbiente *dados);

// Classificações e alertas
void classificar(DadosAmbiente *dados, Classificacoes *classificacoes);
const char* classificarIluminacao(DadosAmbiente *dados);
const char* classificarTemperatura(DadosAmbiente *dados);
const char* classificarUmidade(DadosAmbiente *dados);
const char* classificarOcupacao(DadosAmbiente *dados);
const char* classificarAr(DadosAmbiente *dados);
const char* classificarICS(Classificacoes *classificacoes);
void calcularICS(Classificacoes *classificacoes, DadosAmbiente *dados);
void gerarAlertas(DadosAmbiente *dados, Alertas *alertas);

// Formatação JSON
void gerarJson(DadosAmbiente *dados, Alertas *alertas, Classificacoes *classificacoes, char *buffer, size_t tamanho);
void jsonDadosSensores(DadosAmbiente *dados, char buffer[], size_t tamanho);
void gerarJsonAlertas(Alertas *alertas, char buffer[], size_t tamanho);
void gerarJsonClassificacao(Classificacoes *classificacoes, char buffer[], size_t tamanho);
void gerarJsonHistorico(int *indice, void* valor, char buffer[], size_t tamanho, void (*funcaoFormatacao) (void*, char*, size_t), const char* formatoHist);

// Conversores Genéricos
void formatarFloat(void *ponteiro, char *buffer, size_t tamanho);
void formatarInt(void *ponteiro, char *buffer, size_t tamanho);
void formatarBool(void *ponteiro, char *buffer, size_t tamanho);
void formatarString(void *ponteiro, char *buffer, size_t tamanho);

// Utilitários de Relógio e Debug
void imprimirSerial(DataHora *dataHora, DadosAmbiente *dados, Classificacoes *classificacoes, Alertas *alertas);
bool atualizarDataHora(DataHora *dataHora);
void sincronizarHorario();


void setup() {
  Serial.begin(115200);
  
  pinMode(PINO_HCSR_TRIG, OUTPUT);
  pinMode(PINO_HCSR_ECHO, INPUT);
  pinMode(PINO_PIR, INPUT);
  pinMode(PINO_LDR, INPUT);
  pinMode(PINO_MQ2, INPUT);
  digitalWrite(PINO_HCSR_TRIG, LOW);
  
  dht.setup(PINO_DHT, DHTesp::DHT22);
  conectarWiFi();
  client.setInsecure();
  sincronizarHorario();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (millis() - tempoUltimoEnvio >= INTERVALO_DE_ENVIO_MS) {
    
    coletarDadosDoAmbiente(&dadosAmbiente);
    gerarAlertas(&dadosAmbiente, &alertas);
    classificar(&dadosAmbiente, &classificacoes);
    atualizarDataHora(&dataHoraAtual);
    imprimirSerial(&dataHoraAtual, &dadosAmbiente, &classificacoes, &alertas);

    gerarJson(&dadosAmbiente, &alertas, &classificacoes, bufferDadosJson, TAM_BUFFER_GERAL);
    enviarDadosParaBanco(CAMINHO_DADOS, bufferDadosJson, MAX_TENTATIVAS_ENVIO, ATRASO_ENTRE_TENTATIVAS_MS, httpPatch);

    gerarJsonHistorico(&indiceHistTemp, &dadosAmbiente.temperatura, bufferDadosJson, TAM_BUFFER_GERAL, formatarFloat, HIST_TEMP_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_TEMP, bufferDadosJson, MAX_TENTATIVAS_ENVIO, ATRASO_ENTRE_TENTATIVAS_MS, httpPatch);

    gerarJsonHistorico(&indiceHistOcup, &dadosAmbiente.ocupacao, bufferDadosJson, TAM_BUFFER_GERAL, formatarInt, HIST_OCUP_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_OCUP, bufferDadosJson, MAX_TENTATIVAS_ENVIO, ATRASO_ENTRE_TENTATIVAS_MS, httpPatch);

    gerarJsonHistorico(&indiceHistUmid, &dadosAmbiente.umidade, bufferDadosJson, TAM_BUFFER_GERAL, formatarFloat, HIST_UMID_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_UMID, bufferDadosJson, MAX_TENTATIVAS_ENVIO, ATRASO_ENTRE_TENTATIVAS_MS, httpPatch);

    gerarJsonHistorico(&indiceHistLum, &dadosAmbiente.luminosidade, bufferDadosJson, TAM_BUFFER_GERAL, formatarInt, HIST_LUM_FORMAT);
    enviarDadosParaBanco(CAMINHO_HIST_LUM, bufferDadosJson, MAX_TENTATIVAS_ENVIO, ATRASO_ENTRE_TENTATIVAS_MS, httpPatch);

    tempoUltimoEnvio = millis();
  } 
}

// IMPLEMENTAÇÃO DAS FUNÇÕES

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

void enviarDadosParaBanco(const char *caminho, char json[], int tentativas, int msEntreTentativas, int (*metodoHttp) (HTTPClient*, char*)) {
  snprintf(bufferRequisicao, TAM_BUFFER_GERAL, "%s%s", FIREBASE_URL, caminho);
  HTTPClient http;
  int codigoStatus;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

  for(int i = 1; i <= tentativas; i++){
    http.begin(client, bufferRequisicao);
    http.addHeader("Content-Type", "application/json");
    codigoStatus = metodoHttp(&http, json);
    http.end();

    if (codigoStatus >= 200 && codigoStatus < 300) {
      Serial.println("\nDados enviados:");
      Serial.println(json);
      return;
    }

    Serial.print("Erro ao enviar dados. HTTP: ");
    Serial.println(codigoStatus);
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

unsigned long calcularMediana(unsigned long buffer[], size_t tamanho) {
  unsigned long valor = 0;

  for (size_t i = 1; i < tamanho; i++) {
    valor = buffer[i];
    for (int x = i - 1; x >= 0; x--) {
      if (valor < buffer[x]) {
        buffer[x + 1] = buffer[x];
        buffer[x] = valor;
      }
    }
  }

  return buffer[tamanho / 2];
}

void preencherBufferDistancia(unsigned long buffer[], int pinoTrig, int pinoEcho, int repeticoes) {
  unsigned long duracao = 0;

  for (int i = 0; i < repeticoes; i++) {
    duracao = lerHCSR04(pinoTrig, pinoEcho);
    delay(60);

    if (duracao > 0) {
      buffer[i] = duracao / 58;
    } else {
      buffer[i] = DISTANCIA_MAXIMA;
    }
  }
}

unsigned long lerHCSR04(int pinoTrig, int pinoEcho) {
  digitalWrite(pinoTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinoTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinoTrig, LOW);

  return pulseIn(pinoEcho, HIGH, 30000);
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

bool lerPIR(int pinoPir) {
  return digitalRead(pinoPir);
}

int lerSensorMQ2(int pinoMq2) {
  return analogRead(pinoMq2);
}

int lerSensorLDR(int pinoLdr) {
  return analogRead(pinoLdr);
}

void coletarDadosDoAmbiente(DadosAmbiente *dados) {
  lerDHT(dados);
  preencherBufferDistancia(bufferDistancia, PINO_HCSR_TRIG, PINO_HCSR_ECHO, TAM_BUFFER_DISTANCIA);
  dados->presenca = lerPIR(PINO_PIR);
  dados->distancia = calcularMediana(bufferDistancia, TAM_BUFFER_DISTANCIA);
  dados->qualidadeDoAr = lerSensorMQ2(PINO_MQ2);
  dados->luminosidade = lerSensorLDR(PINO_LDR);

  if (dados->presenca) {
    dados->ocupacao = constrain(map(dados->distancia, DISTANCIA_MAXIMA, DISTANCIA_MINIMA, 0, 100), 0, 100);
  } else {
    dados->ocupacao = 0;
  }
}

void classificar(DadosAmbiente *dados, Classificacoes *classificacoes){
  classificacoes->temperatura = classificarTemperatura(dados);
  classificacoes->umidade = classificarUmidade(dados);
  classificacoes->iluminacao = classificarIluminacao(dados);
  classificacoes->ocupacao = classificarOcupacao(dados);
  classificacoes->ar = classificarAr(dados);
  calcularICS(classificacoes, dados);
  classificacoes->classificacaoIcs = classificarICS(classificacoes); 
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

// Implementação correta da função geradora do JSON mestre
void gerarJson(DadosAmbiente *dados, Alertas *alertas, Classificacoes *classificacoes, char *buffer, size_t tamanho){
  snprintf(buffer, tamanho, 
        "{"
            "\"sensores\":{\"temperatura\":%.1f,\"umidade\":%.1f,\"luminosidade\":%d,\"presenca\":%s,\"distancia\":%d,\"ocupacao\":%d,\"mq2\":%d},"
            "\"classificacoes\":{\"classificacao_ics\":\"%s\",\"ics\":%d,\"luminosidade\":\"%s\",\"ocupacao\":\"%s\",\"qualidade_do_ar\":\"%s\",\"temperatura\":\"%s\",\"umidade\":\"%s\"},"
            "\"alertas\":{\"iluminacao\":\"%s\",\"mq2\":\"%s\",\"ocupacao\":\"%s\",\"temperatura\":\"%s\",\"alerta_ativo\":%s}"
        "}",
        dados->temperatura, dados->umidade, dados->luminosidade,
        dados->presenca ? "true" : "false", dados->distancia, 
        dados->ocupacao, dados->qualidadeDoAr,
        classificacoes->classificacaoIcs, classificacoes->pontosIcs, classificacoes->iluminacao,
        classificacoes->ocupacao, classificacoes->ar, classificacoes->temperatura, classificacoes->umidade,
        alertas->iluminacao, alertas->mq2, alertas->ocupacao, alertas->temperatura,
        alertas->alertaAtivo ? "true" : "false"
    );
}

void jsonDadosSensores(DadosAmbiente *dados, char buffer[], size_t tamanho) {
  snprintf(buffer, tamanho,
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

void gerarJsonAlertas(Alertas *alertas, char buffer[], size_t tamanho){
  snprintf(buffer, tamanho, "{\"iluminacao\":\"%s\",\"mq2\":\"%s\",\"ocupacao\":\"%s\",\"temperatura\":\"%s\",\"alerta_ativo\":%s}",
    alertas->iluminacao,
    alertas->mq2,
    alertas->ocupacao,
    alertas->temperatura,
    alertas->alertaAtivo ? "true" : "false"
  );
}

void gerarJsonClassificacao(Classificacoes *classificacoes, char buffer[], size_t tamanho){
  snprintf(buffer, tamanho, "{\"classificacao_ics\":\"%s\",\"ics\":%d,\"luminosidade\":\"%s\",\"ocupacao\":\"%s\",\"qualidade_do_ar\":\"%s\",\"temperatura\":\"%s\",\"umidade\":\"%s\"}",
    classificacoes->classificacaoIcs,
    classificacoes->pontosIcs,
    classificacoes->iluminacao,
    classificacoes->ocupacao,
    classificacoes->ar,
    classificacoes->temperatura,
    classificacoes->umidade
  );
}

void gerarJsonHistorico(int *indice, void* valor, char buffer[], size_t tamanho, void (*funcaoFormatacao) (void*, char*, size_t), const char *formatoHist){
  char valorBuffer[10] = "";

  funcaoFormatacao(valor, valorBuffer, sizeof(valorBuffer));

  *indice = (*indice < 10) ? *indice : 0;

  snprintf(buffer, tamanho, formatoHist, *indice, valorBuffer);

  (*indice)++;
}

void formatarFloat(void *ponteiro, char *buffer, size_t tamanho){
  snprintf(buffer, tamanho, "%.1f", *(float*) ponteiro);
}

void formatarInt(void *ponteiro, char *buffer, size_t tamanho){
  snprintf(buffer, tamanho, "%d", *(int*) ponteiro);
}

void formatarBool(void *ponteiro, char *buffer, size_t tamanho){
  snprintf(buffer, tamanho, "%s", *(bool*) ponteiro ? "true" : "false");
}

void formatarString(void *ponteiro, char *buffer, size_t tamanho){
  snprintf(buffer, tamanho, "%s", (char*) ponteiro);
}

void imprimirSerial(DataHora *dataHora, DadosAmbiente *dados, Classificacoes *classificacoes, Alertas *alertas){
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

bool atualizarDataHora(DataHora *dataHora){
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
  configTime(FUSO_HORARIO_SEGUNDOS, 0, NTP_SERVIDOR_1, NTP_SERVIDOR_2);

  Serial.print("Sincronizando horario (NTP)");

  unsigned long inicio = millis();
  unsigned long ms = 0;
  unsigned long msAnterior = 0;
  int intervalo = 500;

  while (!atualizarDataHora(&dataHoraAtual) && (millis() - inicio) < TEMPO_LIMITE_NTP_MS) {
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