/*
  DoinoCoin_Esp_Master.ino
  created 10 05 2021
  by Luiz H. Cassettari
  
  Modified by JK Rolling

  Some changes to get a good performance in H/s. 
  Modified by feltoxxx
  01/03/2025

*/

#include <ArduinoOTA.h>
#include <StreamString.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void wire_setup();
void wire_readAll();
boolean wire_exists(byte address);
void wire_sendJob(byte address, String lastblockhash, String newblockhash, int difficulty);
void Wire_sendln(byte address, String message);
void Wire_send(byte address, String message);
String wire_readLine(int address);
boolean wire_runEvery(unsigned long interval);

#define MINER "AVR I2C v4.3"
#define JOB "AVR"
// #define JOB "ARM"
// #define JOB "DUE"   // For RP2040 slaves

const char* ssid = " ";
const char* password = " ";
const char* ducouser = " ";
const char* rigIdentifier = "I2C MINER";
const char* mDNSRigIdentifier = "esp32";
String mining_key = " ";

#define REPORT_INTERVAL 60000
#define CHECK_MINING_KEY false
#define REPEATED_WIRE_SEND_COUNT 1

void handleSystemEvents() {
  ArduinoOTA.handle();
  yield();
}

void SetupWifi() {
  Serial.printf("Connecting to: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
}

void SetupOTA() {
  ArduinoOTA.onStart([]() { Serial.println("Start"); });
  ArduinoOTA.onEnd([]() { Serial.println("End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  char hostname[32];
  sprintf(hostname, "Miner32-Async-%06x", ESP.getEfuseMac());
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();
}

void blink(uint8_t count, uint8_t pin = LED_BUILTIN) {
  for (uint8_t x = 0; x < (count << 1); ++x) {
    digitalWrite(pin, !digitalRead(pin));
    delay(50);
  }
}

void RestartESP(const char* msg) {
  Serial.println(msg);
  Serial.println("Resetting ESP...");
  blink(5);
  ESP.restart();
}

void TaskClientConnector(void *pvParameters) {
  while (true) {
    ClientConnector();
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void TaskMiner(void *pvParameters) {
  while (true) {
    clients_loop();
    if (runEvery(REPORT_INTERVAL)) {
      // Serial.printf("[ ] FreeRam: %d %s\n", ESP.getFreeHeap(), clients_string().c_str());
      Serial.println(" ");
      periodic_report(REPORT_INTERVAL);
      Serial.println(" ");
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void TaskNetwork(void *pvParameters) {
  while (true) {
    ArduinoOTA.handle();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.printf("\nDuino-Coin %s\n", MINER);

  wire_setup();
  SetupWifi();
  SetupOTA();
  
  if (!MDNS.begin(mDNSRigIdentifier)) Serial.println("mDNS unavailable");
  MDNS.addService("http", "tcp", 80);
  Serial.printf("Configured mDNS at http://%s.local (or http://%s)\n", mDNSRigIdentifier, WiFi.localIP().toString().c_str());

  UpdatePool();
  if (CHECK_MINING_KEY) UpdateMiningKey();
  else SetMiningKey(mining_key);
  blink(2);
  
  
  xTaskCreatePinnedToCore(TaskClientConnector, "ClientConnector", 10000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskMiner, "TaskMiner", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskNetwork, "TaskNetwork", 4096, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}

boolean runEvery(unsigned long interval) {
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
