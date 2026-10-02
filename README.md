# SALA DE AULA INTELIGENTE
HTML5 + CSS | JAVASCRIPT | IoT

## Proposta
- Desenvolver e simular no Wokwi um sistema de monitoramento inteligente de uma sala de aula, coletando dados de
temperatura, umidade, iluminação, presença, ocupação e qualidade do ar, processando essas informações no ESP32
e apresentando os resultados em um dashboard web.

## Objetivo
- Desenvolver e simular no Wokwi um sistema de monitoramento inteligente de uma sala de aula utilizando
ESP32 e sensores. Os dados deverão ser processados pelo ESP32 e enviados para um dashboard web,
que apresentará valores atuais, gráficos, histórico, classificações, alertas e um Índice de Conforto da Sala.

## Integração
SENSORES → ESP32 → PROCESSAMENTO → COMUNICAÇÃO → DASHBOARD

## Componentes
1. ESP32: Controla o sistema, processa as leituras e envia os dados.
2. DHT22: Mede temperatura e umidade relativa.
3. LDR: Mede a intensidade luminosa do ambiente.
4. PIR: Detecta movimento/presença.
5. HC-SR04: Mede distância e será utilizado para estimar a ocupação.
6. MQ2: Detecta variações de gases; nesta atividade, use a leitura como indicador relativo.

## Tecnologias utilizadas
- Visual Studio Code
- Wowki
- Firebase Spark
- Git Bash
