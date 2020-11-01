#include <Arduino.h>
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>
#include <sstream>
#include <queue>
#include <string>
#include "eQ3.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include <esp_wifi.h>
#include <BLEDevice.h>

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "xxx"
#define MQTT_HOST "xxx.myfritz.net"
#define MQTT_USER "xxx"
#define MQTT_PASSWORD "xxx"
#define MQTT_TOPIC "SmartLock"
#define ADDRESS "00:00:00:00:00:00"
#define USER_KEY "00000000000000000000000000000000"
#define USER_ID 6
#define CARD_KEY "M001AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"

eQ3* keyble;
bool do_open = false;
bool do_lock = false;
bool do_unlock = false;

// -----------------------------------------------------------------------------
// ---[MqttCallback]------------------------------------------------------------
// -----------------------------------------------------------------------------
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println(" # Message received");
  if (payload[0] == 'o')
    do_open = true;
  if (payload[0] == 'l')
    do_lock = true;
  if (payload[0] == 'u')
    do_unlock = true;
}

WiFiClient wifiClient;
PubSubClient mqttClient(MQTT_HOST, 1883, &MqttCallback, wifiClient);

// -----------------------------------------------------------------------------
// ---[Wifi Signalqualität]-----------------------------------------------------
// -----------------------------------------------------------------------------
int GetWifiSignalQuality() {
float signal = 2 * (WiFi.RSSI() + 100);
if (signal > 100)
  return 100;
else
  return signal;
}

// -----------------------------------------------------------------------------
// ---[Start WiFi]--------------------------------------------------------------
// -----------------------------------------------------------------------------
void SetupWifi() {
  Serial.println("# WIFI: Connecting...");
  WiFi.persistent(false);    // Verhindert das neuschreiben des Flashs
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() == WL_CONNECTED)
    Serial.println("# WIFI: Connection restored: SSiD: " + WiFi.SSID());
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("# WIFI: Check SSiD: " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(500);
      if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("# WIFI: Connected");
      break;
    }
  }
  Serial.println("# WIFI: Signal quality: "+ String(GetWifiSignalQuality()) + " %");
  Serial.print("# WIFI: IP address: "); Serial.println(WiFi.localIP());
}

// -----------------------------------------------------------------------------
// ---[MQTT-Setup]--------------------------------------------------------------
// -----------------------------------------------------------------------------
void SetupMqtt() {
  while (!mqttClient.connected()) { // Loop until we're reconnected to the MQTT server
  	Serial.print("# Try to connect MQTT-Broker...");
  	if (mqttClient.connect("SmartLock", MQTT_USER, MQTT_PASSWORD)) {
  		Serial.println("\tconnected!");
  		mqttClient.subscribe(MQTT_TOPIC);
  	}
  	else {
      Serial.print("\tfailed, rc=");
      Serial.println(mqttClient.state());
      abort();
    }
  }
}

// -----------------------------------------------------------------------------
// ---[Setup]-------------------------------------------------------------------
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");
    Serial.setDebugOutput(true);

    SetupWifi();

    //Bluetooth
    BLEDevice::init("");
    keyble = new eQ3(ADDRESS, USER_KEY, USER_ID);

    //MQTT
    SetupMqtt();
}

// -----------------------------------------------------------------------------
// ---[loop]--------------------------------------------------------------------
// -----------------------------------------------------------------------------
void loop() {
    // Wifi reconnect
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("# WiFi disconnected, reconnecting...");
      SetupWifi();
    }

    // MQTT connected?
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
      Serial.println("# MQTT disconnected, reconnecting...");
      SetupMqtt();
    }
    mqttClient.loop();

    if (do_open) {
        Serial.println("open");
        keyble->open();
        do_open = false;
    }

    if (do_lock) {
        Serial.println("lock");
        keyble->lock();
        do_lock = false;
    }

    if (do_unlock) {
        Serial.println("unlock");
        keyble->unlock();
        do_unlock = false;
    }
}
