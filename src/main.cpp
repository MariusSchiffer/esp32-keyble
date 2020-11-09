#include <Arduino.h>
#include <esp_log.h>
#include <sstream>
#include <queue>
#include <string>
#include "eQ3.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include <esp_wifi.h>
#include <BLEDevice.h>

/*#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "xxx"
#define MQTT_HOST "xxx.myfritz.net"
#define MQTT_USER "xxx"
#define MQTT_PASSWORD "xxx"
#define MQTT_TOPIC_SUB "SmartLock/command"
#define MQTT_TOPIC_PUB "SmartLock"
#define ADDRESS "00:00:00:00:00:00"
#define USER_KEY "00000000000000000000000000000000"
#define USER_ID 6
#define CARD_KEY "M001AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"*/


eQ3* keyble;
bool do_open = false;
bool do_lock = false;
bool do_unlock = false;
bool wifiActive = true;
int status = 0;

// -----------------------------------------------------------------------------
// ---[MqttCallback]------------------------------------------------------------
// -----------------------------------------------------------------------------
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("# Nachricht empfangen: ");
  if (payload[0] == '4') //open
    do_open = true;
  if (payload[0] == '3') //lock
    do_lock = true;
  if (payload[0] == '2') //unlock
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
  Serial.println("# WIFI: verbinde...");
  WiFi.persistent(false);    // Verhindert das neuschreiben des Flashs
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() == WL_CONNECTED)
    Serial.println("# WIFI: Verbindung wiederhergestellt: SSiD: " + WiFi.SSID());
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("# WIFI: prüfe SSiD: " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(500);
      if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("# WIFI: verbunden!");
      break;
    }
  }
  Serial.println("# WIFI: Signalqualität: "+ String(GetWifiSignalQuality()) + "%");
  Serial.print("# WIFI: IP-Adresse: "); Serial.println(WiFi.localIP());
}

// -----------------------------------------------------------------------------
// ---[MQTT-Setup]--------------------------------------------------------------
// -----------------------------------------------------------------------------
void SetupMqtt() {
  while (!mqttClient.connected()) { // Loop until we're reconnected to the MQTT server
  	Serial.print("# Mit MQTT-Broker verbinden... ");
  	if (mqttClient.connect("SmartLock", MQTT_USER, MQTT_PASSWORD)) {
  		Serial.println("\tverbunden!");
  		mqttClient.subscribe(MQTT_TOPIC_SUB);
  	}
  	else {
      Serial.print("\t Fehler, rc=");
      Serial.println(mqttClient.state());
      //abort();
    }
  }
}

// -----------------------------------------------------------------------------
// ---[Setup]-------------------------------------------------------------------
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("Starte...");
    Serial.setDebugOutput(true);

    SetupWifi();

    //Bluetooth
    BLEDevice::init("");
    keyble = new eQ3(ADDRESS, USER_KEY, USER_ID);

    //MQTT
    SetupMqtt();
}


// -----------------------------------------------------------------------------
// ---[SetWifi]-----------------------------------------------------------------
// -----------------------------------------------------------------------------
void SetWifi(bool active) {
  wifiActive = active;
  if (active) {
    WiFi.mode(WIFI_STA);
    Serial.println("# WiFi aktiviert");
  }
  else {
    WiFi.mode(WIFI_OFF);
    Serial.println("# WiFi detiviert");
  }
}

// -----------------------------------------------------------------------------
// ---[loop]--------------------------------------------------------------------
// -----------------------------------------------------------------------------
void loop() {
    // Wifi reconnect
    if (WiFi.status() != WL_CONNECTED && wifiActive) {
      Serial.println("# WiFi getrennt, wiederverbinden...");
      SetupWifi();
    }

    // MQTT connected?
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected() && wifiActive) {
      Serial.println("# MQTT getrennt, wiederverbinden...");
      SetupMqtt();
    }
    else
      mqttClient.loop();

    if (keyble->_LockStatus != status){
      status = keyble->_LockStatus;
      // convert status for publish
      char tmp[1];
      String tmp_str = String(status);
      tmp_str.toCharArray(tmp, tmp_str.length() + 1);
      // publish
      mqttClient.publish(MQTT_TOPIC_PUB, tmp);
    }

    if (do_open) {
      Serial.println("Öffnen + Schlossfalle");
      SetWifi(false);
      keyble->open();
      SetWifi(true);
      do_open = false;
    }

    if (do_lock) {
      Serial.println("Schließen");
      SetWifi(false);
      keyble->lock();
      SetWifi(true);
      do_lock = false;
    }

    if (do_unlock) {
      Serial.println("Öffnen");
      SetWifi(false);
      keyble->unlock();
      SetWifi(true);
      do_unlock = false;
    }
}
