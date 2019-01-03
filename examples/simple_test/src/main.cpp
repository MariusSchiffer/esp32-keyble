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
#include <BLEDevice.h>

//#define WIFI_SSID ""
//#define WIFI_PASSWORD ""
//#define MQTT_HOST ""
//#define MQTT_USER ""
//#define MQTT_PASSWORD ""
//#define MQTT_TOPIC ""
//#define ADDRESS ""
//#define USER_KEY ""
//#define USER_ID 0
//#define CARD_KEY ""

// Include above defines:
#include "secrets.h"

#define WITH_WIFI
eQ3* keyble;
bool do_open = false;
#ifdef WITH_WIFI
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void reconnect() {
    while (!mqttClient.connected()) {
        mqttClient.connect("esp32client",MQTT_USER,MQTT_PASSWORD);
    }
    mqttClient.subscribe(MQTT_TOPIC);
    Serial.println("subscribed");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("got message");
    do_open = true;
}
#endif

void setup() {
    Serial.begin(115200);
    
    Serial.println("Starting...");
    BLEDevice::init("");
    keyble = new eQ3(ADDRESS, USER_KEY, USER_ID);
#ifdef WITH_WIFI
    mqttClient.setServer(MQTT_HOST,1883);
    mqttClient.setCallback(&callback);

    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#else
    keyble->open();
#endif
}

void loop() {
#ifdef WITH_WIFI
    if (WiFi.isConnected() && !mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();

    if (do_open) {
        keyble->open();
        do_open = false;
    }
#endif
}
