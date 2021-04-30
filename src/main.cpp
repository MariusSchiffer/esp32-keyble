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

#define WIFI_SSID "FRITZ!Box"
#define WIFI_PASSWORD "xxx"
#define MQTT_HOST "192.168.178.50"
//#define MQTT_USER ""
//#define MQTT_PASSWORD ""
#define MQTT_TOPIC_SUB "Smartlock/command"
#define MQTT_TOPIC_PUB "Smartlock"
#define MQTT_TOPIC_PUB2 "Smartlock/raw"
#define ADDRESS "xxx"
#define USER_KEY "xxxx"
#define USER_ID 2
//#define CARD_KEY "3fcac062775f34a244605c071930cea8"

eQ3* keyble;
bool do_open = false;
bool do_lock = false;
bool do_unlock = false;
bool do_status = false;
bool wifiActive = true;
bool cmdTriggered=false;
unsigned long timeout=0;
bool statusUpdated=false;
bool waitForAnswer=false;
unsigned long starttime=0;
int status = 0;

void MqttCallback(char* topic, byte* payload, unsigned int length);

WiFiClient wifiClient;
PubSubClient mqttClient(MQTT_HOST, 1882, &MqttCallback, wifiClient);

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
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("# WiFi deaktiviert");
  }
  delay(100);
  yield();
}

// -----------------------------------------------------------------------------
// ---[MqttCallback]------------------------------------------------------------
// -----------------------------------------------------------------------------
void MqttCallback(char* topic, byte* payload, unsigned int length) {

  Serial.println("# Nachricht empfangen: ");
 

  if (payload[0] == '4') //open
    do_open = true;
  if (payload[0] == '3') //lock
    do_lock = true;
  if (payload[0] == '2') //unlock
    do_unlock = true;
  if (payload[0] == '1') //status
    do_status = true;
}


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
  IPAddress ip(192, 168, 178, 93);
  IPAddress gateway(192, 168, 178, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 178, 1);
  WiFi.config(ip, dns, gateway, subnet);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("# WIFI: Verbindung wiederhergestellt: SSiD: " + WiFi.SSID());

  int maxWait=100;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("# WIFI: prüfe SSiD: " + String(WIFI_SSID));
    //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(500);
    
    if (maxWait <= 0)
        ESP.restart();
      maxWait--;
  }
  Serial.println("# WIFI: verbunden!");
  Serial.println("# WIFI: Signalqualität: "+ String(GetWifiSignalQuality()) + "%");
  Serial.print("# WIFI: IP-Adresse: "); Serial.println(WiFi.localIP());
}

// -----------------------------------------------------------------------------
// ---[MQTT-Setup]--------------------------------------------------------------
// -----------------------------------------------------------------------------
void SetupMqtt() {
  while (!mqttClient.connected()) { // Loop until we're reconnected to the MQTT server
  	Serial.print("# Mit MQTT-Broker verbinden... ");
  	if (mqttClient.connect(MQTT_TOPIC_PUB)) {
  		Serial.println("\tverbunden!");
  		mqttClient.subscribe(MQTT_TOPIC_SUB);
  	}
  	else {
      Serial.print("\t Fehler, rc=");
      Serial.println(mqttClient.state());
      //abort();
    }
    delay(100);
    yield();
  }
}

// -----------------------------------------------------------------------------
// ---[Setup]-------------------------------------------------------------------
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("Starte...");
    //Serial.setDebugOutput(true);

    SetupWifi();

    //Bluetooth
    BLEDevice::init("esp32ble");
    keyble = new eQ3(ADDRESS, USER_KEY, USER_ID);

    //MQTT
    SetupMqtt();
    //delay(500);
}



// -----------------------------------------------------------------------------
// ---[loop]--------------------------------------------------------------------
// -----------------------------------------------------------------------------
void loop() {
    // Wifi reconnect
    if( wifiActive)
    {
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("# WiFi getrennt, wiederverbinden...");
          SetupWifi();
        }
        else{
          // MQTT connected?
          if(!mqttClient.connected())
          {
            if (WiFi.status() == WL_CONNECTED) 
            {
              Serial.println("# MQTT getrennt, wiederverbinden...");
              SetupMqtt();
            }
          }
          else if(mqttClient.connected())
              mqttClient.loop();
        }
        

      if (statusUpdated && mqttClient.connected())
      {
        statusUpdated = false;
        status = keyble->_LockStatus;
        String str_status="";
        char charBuffer[16];
        char charBuffer2[8];
        if(status == 1)
          str_status = "moving";
        else if(status == 2)
          str_status = "unlocked";
        else if(status == 3)
          str_status = "locked";
        else if(status == 4)
          str_status = "open";
        else if(status == 9)
          str_status = "timeout";
        else
          str_status = "unknown";
        String strBuffer =  String(str_status);
        strBuffer.toCharArray(charBuffer, 16);
        mqttClient.publish(MQTT_TOPIC_PUB, charBuffer);

        if(keyble->raw_data[0] == 0x72){  

        sprintf (charBuffer2, "%02X %02X %02X %02X %02X %02X %02X %02X", keyble->raw_data.c_str()[0]
        , keyble->raw_data.c_str()[1]
        , keyble->raw_data.c_str()[2]
        , keyble->raw_data.c_str()[3]
        , keyble->raw_data.c_str()[4]
        , keyble->raw_data.c_str()[5]
        , keyble->raw_data.c_str()[6]
        , keyble->raw_data.c_str()[7]);
        
        mqttClient.publish(MQTT_TOPIC_PUB2, charBuffer2);
        }
        // if(status == 9)
        // {
        //   Serial.println("ESP Restarting...");
        //   mqttClient.loop();
        //   yield();
        //   delay(2000);
          
        //   //ESP.restart();
        // }
      }
    }
    if (do_open || do_lock || do_unlock || do_status) 
    {
      SetWifi(false);
      //btStart();
      yield();
      waitForAnswer=true;
      keyble->_LockStatus = -1;
      starttime = millis();

      if (do_open) {
        Serial.println("Öffnen + Schlossfalle");
        //SetWifi(false);
        keyble->open();
        //SetWifi(true);
        do_open = false;
      }

      if (do_lock) {
        Serial.println("Schließen");
        //SetWifi(false);
        keyble->lock();
        //SetWifi(true);
        do_lock = false;
      }

      if (do_unlock) {
        Serial.println("Öffnen");
        //SetWifi(false);
        keyble->unlock();
        //SetWifi(true);
        do_unlock = false;
      }
       if (do_status) {
        Serial.println("Status");
        //SetWifi(false);
        keyble->updateInfo();
        //SetWifi(true);
        do_status = false;
      }
    }
     if(waitForAnswer)
     {
      
      bool timeout=(millis() - starttime > LOCK_TIMEOUT *1000 +1000);
      bool finished=false;

      if ((keyble->_LockStatus != -1) || timeout)
      {
        if(keyble->_LockStatus == 1)
        {
            //Serial.println("Lockstatus 1");
            if(timeout)
            {
              finished=true;
              Serial.println("Lockstatus 1 - timeout");
            }
        }
        else if(keyble->_LockStatus == -1)
        {
          //Serial.println("Lockstatus -1");
          if(timeout){
            keyble->_LockStatus = 9; //timeout
            finished=true;
            Serial.println("Lockstatus -1 - timeout");
          }
        }
        else if(keyble->_LockStatus != 1)
        {
           finished=true;
           Serial.println("Lockstatus != 1");
        }

        if(finished)
        {
          Serial.println("finshed.");
          
          do
          {
            keyble->bleClient->disconnect();
            delay(100);
          }
          while(keyble->state.connectionState != DISCONNECTED && !timeout);

          //btStop();
          delay(100);
          yield();
          Serial.println("# Lock status changed or timeout ...");
         // Serial.print("Data received: ");
         

          
          SetWifi(true);
          waitForAnswer=false;
          statusUpdated=true;
        }


      }
     }

}
