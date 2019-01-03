#include <Arduino.h>


#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <esp_log.h>
#include <sstream>
#include <queue>
#include <string>
#include "eQ3.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include <esp_wifi.h>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define MQTT_HOST ""
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_TOPIC ""

WiFiClient espClient;
PubSubClient mqttClient(espClient);



bool do_open = false;

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("connected");

    while (!mqttClient.connected()) {
        mqttClient.connect("esp32client",MQTT_USER,MQTT_PASSWORD);
    }
    mqttClient.subscribe(MQTT_TOPIC);
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("got message");
    do_open = true;
}


void setup() {
    Serial.begin(115200);
    
    Serial.println("Starting...");

    mqttClient.setServer(MQTT_HOST,1883);
    mqttClient.setCallback(&callback);

    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

}

void loop() {
    if (do_open) {

    }
}
