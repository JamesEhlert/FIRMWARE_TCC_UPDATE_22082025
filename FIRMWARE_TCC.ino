/**
 * @file FIRMWARE_TCC.ino
 * @author Seu Nome (Seu Email)
 * @brief Firmware aprimorado para sistema de irrigação inteligente com ESP32 e AWS IoT.
 * @version 3.1 - Versão de Compatibilidade (Sem Watchdog Timer)
 * @date 2025-08-22
 *
 * @details Esta versão remove o Watchdog Timer para garantir a compilação em ambientes
 * com inconsistências no ESP32 Core. A funcionalidade principal com FreeRTOS permanece.
 */

//==============================================================================
// INCLUDES E DEPENDÊNCIAS
//==============================================================================
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <Adafruit_AHTX0.h>
#include <time.h>
// #include <esp_task_wdt.h> // Watchdog removido para compatibilidade

//==============================================================================
// CONSTANTES E DEFINIÇÕES
//==============================================================================
const int LED_PIN = 2;
const char* AWS_IOT_PUBLISH_TOPIC   = "esp32/pub";
const char* AWS_IOT_SUBSCRIBE_TOPIC = "esp32/sub";
const int MQTT_PORT = 8883;
const unsigned long PUBLISH_INTERVAL_MS = 10 * 60 * 1000;

//==============================================================================
// OBJETOS E VARIÁVEIS GLOBAIS
//==============================================================================
Adafruit_AHTX0 aht;
WiFiClientSecure netClient;
PubSubClient mqttClient(netClient);
volatile bool isIrrigationOn = false;
volatile unsigned long irrigationOffTimeMs = 0;

//==============================================================================
// PROTÓTIPOS DE FUNÇÕES
//==============================================================================
void setupWiFi();
void initTime();
void connectToAWS();
void publishSensorData();
void messageHandler(char* topic, byte* payload, unsigned int length);
void taskNetworkManager(void *pvParameters);
void taskSensorPublisher(void *pvParameters);

//==============================================================================
// FUNÇÃO SETUP
//==============================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("======================================================"));
  Serial.println(F("Iniciando Sistema de Irrigação - v3.1 (Compatibilidade)"));
  Serial.println(F("======================================================"));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (!aht.begin()) {
    Serial.println(F("[ERRO] Sensor AHT não detectado! Reiniciando..."));
    delay(10000);
    ESP.restart();
  }
  Serial.println(F("[INFO] Sensor AHT inicializado."));

  setupWiFi();
  initTime();
  
  // A inicialização do Watchdog Timer foi removida daqui.

  xTaskCreatePinnedToCore(
      taskNetworkManager, "NetworkManagerTask", 8192, NULL, 1, NULL, 1);

  xTaskCreatePinnedToCore(
      taskSensorPublisher, "SensorPublisherTask", 4096, NULL, 0, NULL, 1);
      
  Serial.println(F("[INFO] Tarefas do FreeRTOS iniciadas."));
}

//==============================================================================
// FUNÇÃO LOOP
//==============================================================================
void loop() {
  // esp_task_wdt_reset(); // Reset do Watchdog removido.

  if (isIrrigationOn && millis() > irrigationOffTimeMs) {
    Serial.println(F("[CONTROLE] Tempo de irrigação esgotado. Desligando..."));
    digitalWrite(LED_PIN, LOW);
    isIrrigationOn = false;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
}

//==============================================================================
// TAREFAS E FUNÇÕES (sem alterações)
//==============================================================================
void taskNetworkManager(void *pvParameters) {
  Serial.println(F("[RTOS] Tarefa de Rede iniciada."));
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("[REDE] Wi-Fi desconectado. Reconectando..."));
      setupWiFi();
    } else if (!mqttClient.connected()) {
      Serial.println(F("[MQTT] Desconectado da AWS. Reconectando..."));
      connectToAWS();
    } else {
      mqttClient.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void taskSensorPublisher(void *pvParameters) {
  Serial.println(F("[RTOS] Tarefa de Sensor/Publicação iniciada."));
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
      publishSensorData();
    } else {
      Serial.println(F("[SENSOR] Pular publicação. Sem conexão."));
    }
    vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
  }
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("[REDE] Conectando ao Wi-Fi..."));
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("."));
    delay(500);
  }
  Serial.println(F("\n[REDE] Wi-Fi conectado!"));
  Serial.print(F("[REDE] IP: "));
  Serial.println(WiFi.localIP());
}

void initTime() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print(F("[NTP] Sincronizando hora..."));
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(F("\n[NTP] Hora sincronizada."));
}

void connectToAWS() {
  netClient.setCACert(AWS_CERT_CA);
  netClient.setCertificate(AWS_CERT_CRT);
  netClient.setPrivateKey(AWS_CERT_PRIVATE);
  mqttClient.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
  mqttClient.setCallback(messageHandler);

  Serial.print(F("[MQTT] Conectando ao AWS IoT..."));
  if (mqttClient.connect(THINGNAME)) {
    Serial.println(F(" Conectado!"));
    mqttClient.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
    Serial.printf("[MQTT] Inscrito em: %s\n", AWS_IOT_SUBSCRIBE_TOPIC);
  } else {
    Serial.printf("[MQTT] Falha, estado: %d\n", mqttClient.state());
  }
}

void publishSensorData() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  float h = humidity.relative_humidity;
  float t = temp.temperature;

  if (isnan(h) || isnan(t)) {
    Serial.println(F("[SENSOR] Erro ao ler sensor!"));
    return;
  }
  Serial.printf("[SENSOR] Leitura -> Umidade: %.2f%% | Temp: %.2f C\n", h, t);

  StaticJsonDocument<256> doc;
  char humidityStr[8];
  char temperatureStr[8];
  dtostrf(h, 1, 2, humidityStr);
  dtostrf(t, 1, 2, temperatureStr);
  doc["deviceId"] = THINGNAME;
  doc["humidity"] = humidityStr;
  doc["temperature"] = temperatureStr;
  
  time_t now;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
      time(&now);
      doc["timestamp"] = now;
      char iso8601Buff[25];
      strftime(iso8601Buff, sizeof(iso8601Buff), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
      doc["timestamp_iso8601"] = iso8601Buff;
  } else {
      doc["timestamp"] = 0;
      doc["timestamp_iso8601"] = "1970-01-01T00:00:00Z";
  }

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  if (mqttClient.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) {
      Serial.printf("[MQTT] Mensagem publicada em %s\n", AWS_IOT_PUBLISH_TOPIC);
  } else {
      Serial.println(F("[MQTT] Falha ao publicar."));
  }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.printf("[MQTT] Mensagem recebida: %s\n", topic);
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);
  const char* command = doc["command"];
  int duration_min = doc["duration"];

  Serial.printf("[CONTROLE] Comando: %s | Duração: %d min\n", command, duration_min);

  if (strcmp(command, "on") == 0) {
    digitalWrite(LED_PIN, HIGH);
    isIrrigationOn = true;
    irrigationOffTimeMs = millis() + (duration_min * 60000UL);
    Serial.printf("[CONTROLE] Válvula LIGADA por %d min.\n", duration_min);
  } else if (strcmp(command, "off") == 0) {
    digitalWrite(LED_PIN, LOW);
    isIrrigationOn = false;
    Serial.println(F("[CONTROLE] Válvula DESLIGADA por comando."));
  }
}