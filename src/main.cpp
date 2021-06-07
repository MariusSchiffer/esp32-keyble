#include <Arduino.h>
#include <esp_log.h>
#include <sstream>
#include <queue>
#include <string>
#include "eQ3.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include <esp_wifi.h>
#include <WiFiClient.h>
#include <BLEDevice.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include "AutoConnect.h"

#define PARAM_FILE      "/keyble.json"
#define AUX_SETTING_URI "/keyble_setting"
#define AUX_SAVE_URI    "/keyble_save"
//#define AUX_CLEAR_URI   "/mqtt_clear"

//AP Mode Settings
#define AP_IP     192,168,4,1
#define AP_ID     "KeyBLEBridge"
#define AP_PSK    "eqivalock"
#define AP_TITLE  "KeyBLEBridge"

#define MQTT_SUB "/command"
#define MQTT_PUB "/state"
#define MQTT_PUB2 "/task"
#define MQTT_PUB3 "/battery"
#define MQTT_PUB4 "/rssi"

#define CARD_KEY "M001AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"

// ---[Variables]---------------------------------------------------------------
#pragma region
WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig config;
fs::SPIFFSFS& FlashFS = SPIFFS;

eQ3* keyble;

bool do_open = false;
bool do_lock = false;
bool do_unlock = false;
bool do_status = false;
bool do_toggle = false;
bool do_pair = false;
bool wifiActive = false;
bool cmdTriggered=false;
unsigned long timeout=0;
bool statusUpdated=false;
bool waitForAnswer=false;
bool KeyBLEConfigured = false;
unsigned long starttime=0;
int status = 0;
int rssi = 0;

const int PushButton = 0;

String  MqttServerName;
String  MqttPort;
String  MqttUserName;
String  MqttUserPass;
String  MqttTopic;

String KeyBleMac;
String KeyBleUserKey;
String KeyBleUserId;

String mqtt_sub  = "";
String mqtt_pub  = "";
String mqtt_pub2 = "";
String mqtt_pub3 = "";
String mqtt_pub4 = "";


WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
#pragma endregion
// ---[Add Menue Items]---------------------------------------------------------
#pragma region
static const char AUX_keyble_setting[] PROGMEM = R"raw(
[
  {
    "title": "KeyBLE Settings",
    "uri": "/keyble_setting",
    "menu": true,
    "element": [
      {
        "name": "style",
        "type": "ACStyle",
        "value": "label+input,label+select{position:right;left:120px;width:250px!important;box-sizing:border-box;}"
      },
      {
        "name": "MqttServerName",
        "type": "ACInput",
        "value": "",
        "label": "MQTT Broker IP",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "MQTT broker IP"
      },
      {
        "name": "MqttPort",
        "type": "ACInput",
        "label": "MQTT Broker Port"
      },
      {
        "name": "MqttUserName",
        "type": "ACInput",
        "label": "MQTT User Name"
      },
      {
        "name": "MqttUserPass",
        "type": "ACInput",
        "label": "MQTT User Password",
        "apply": "password"
      },
      {
        "name": "MqttTopic",
        "type": "ACInput",
        "value": "",
        "label": "MQTT Topic",
        "pattern": "^([a-zA-Z0-9]([a-zA-Z0-9-])*[a-zA-Z0-9]){1,24}$"
      },
      {
        "name": "KeyBleMac",
        "type": "ACInput",
        "label": "KeyBLE MAC"
      },
      {
        "name": "KeyBleUserKey",
        "type": "ACInput",
        "label": "KeyBLE User Key"
      },
      {
        "name": "KeyBleUserId",
        "type": "ACInput",
        "label": "KeyBLE User ID"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "Save&amp;Start",
        "uri": "/keyble_save"
      },
      {
        "name": "discard",
        "type": "ACSubmit",
        "value": "Discard",
        "uri": "/_ac"
      }
    ]
  },
  {
    "title": "MQTT Settings",
    "uri": "/keyble_save",
    "menu": false,
    "element": [
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Parameters saved as:</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      },
      {
        "name": "clear",
        "type": "ACSubmit",
        "value": "OK",
        "uri": "/_ac#rdlg"
      }
    ]
  }
]
)raw";

#pragma endregion
// ---[getParams]---------------------------------------------------------------
void getParams(AutoConnectAux& aux) {

  MqttServerName = aux["MqttServerName"].value;
  MqttServerName.trim();
  MqttPort = aux["MqttPort"].value;
  MqttPort.trim();
  MqttUserName = aux["MqttUserName"].value;
  MqttUserName.trim();
  MqttUserPass = aux["MqttUserPass"].value;
  MqttUserPass.trim();
  MqttTopic = aux["MqttTopic"].value;
  MqttTopic.trim();
  KeyBleMac = aux["KeyBleMac"].value;
  KeyBleMac.trim();
  KeyBleUserKey = aux["KeyBleUserKey"].value;
  KeyBleUserKey.trim();
  KeyBleUserId = aux["KeyBleUserId"].value;
  KeyBleUserId.trim();
 }
// ---[loadParams]--------------------------------------------------------------
String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = FlashFS.open(PARAM_FILE, "r");
  if (param) {
    if (aux.loadElement(param)) {
      getParams(aux);
      Serial.println("# " PARAM_FILE " loaded");
      KeyBLEConfigured = true;
    }
    else
    {
      Serial.println("# " PARAM_FILE " failed to load");
      KeyBLEConfigured = false;
    }
    param.close();
  }
  else
    Serial.println("# " PARAM_FILE " open failed");
  return String("");
}
// ---[saveParams]--------------------------------------------------------------
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  AutoConnectAux&   keyble_setting = *Portal.aux(Portal.where());
  getParams(keyble_setting);
  AutoConnectInput& mqttserver = keyble_setting["MqttServerName"].as<AutoConnectInput>();

  File param = FlashFS.open(PARAM_FILE, "w");
  keyble_setting.saveElement(param, { "MqttServerName", "MqttPort", "MqttUserName", "MqttUserPass", "MqttTopic", "KeyBleMac", "KeyBleUserKey", "KeyBleUserId" });
  param.close();

  AutoConnectText&  echo = aux["parameters"].as<AutoConnectText>();
  echo.value = "Server: " + MqttServerName;
  echo.value += mqttserver.isValid() ? String(" (OK)") : String(" (ERR)");
  echo.value += "<br>MQTT Port: " + MqttPort + "<br>";
  echo.value += "MQTT User Name: " + MqttUserName + "<br>";
  echo.value += "MQTT User Pass: <hidden><br>";
  echo.value += "MQTT Topic: " + MqttTopic + "<br>";
  echo.value += "KeyBLE MAC: " + KeyBleMac + "<br>";
  echo.value += "KeyBLE User Key: " + KeyBleUserKey + "<br>";
  echo.value += "KeyBLE User ID: " + KeyBleUserId + "<br>";

  return String("");
}
// ---[MqttCallback]------------------------------------------------------------
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("# Message received: ");
    /*
  //pair
  if (payload[0] == '6')
  {
    do_pair = true;
    mqtt_sub = "*** pair ***";
    Serial.println(mqtt_sub");
    
  }
  */
  //toggle
  if (payload[0] == '5')
  {
    do_toggle = true;
    mqtt_sub = "*** toggle ***";
    Serial.println(mqtt_sub);
  }
  //open
  if (payload[0] == '4')
  {
    do_open = true;
    mqtt_sub = "*** open ***";
    Serial.println(mqtt_sub);
  }
  //lock  
  if (payload[0] == '3')
  {
    do_lock = true;
    mqtt_sub = "*** lock ***";
    Serial.println(mqtt_sub);
  }
  //unlock
  if (payload[0] == '2')
  { 
    do_unlock = true;
    mqtt_sub = "*** unlock ***";
    Serial.println(mqtt_sub);
  }
  //status
  if (payload[0] == '1')
  {
    do_status = true;
    mqtt_sub = "*** status ***";
    Serial.println(mqtt_sub);
  }
}
// ---[MQTTpublish]-------------------------------------------------------------
void MqttPublish()
{
  statusUpdated = false;
  //MQTT_PUB status
  status = keyble->_LockStatus;
  String str_status="";
  char charBuffer1[9];
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
  strBuffer.toCharArray(charBuffer1, 9);
  mqttClient.publish((String(MqttTopic + MQTT_PUB).c_str()), charBuffer1);
  Serial.print("# published ");
  Serial.print((String(MqttTopic + MQTT_PUB).c_str()));
  Serial.print("/");
  Serial.println(charBuffer1);
  mqtt_pub = charBuffer1;

  delay(100);

  //MQTT_PUB2 task
  String str_task="waiting";
  char charBuffer2[8];
  str_task.toCharArray(charBuffer2, 8);
  mqttClient.publish((String(MqttTopic + MQTT_PUB2).c_str()), charBuffer2);
  Serial.print("# published ");
  Serial.print((String(MqttTopic + MQTT_PUB2).c_str()));
  Serial.print("/");
  Serial.println(charBuffer2);
  mqtt_pub2 = charBuffer2;

  //MQTT_PUB3 battery
  if(keyble->raw_data[1] == 0x81)
  {  
    mqttClient.publish((String(MqttTopic + MQTT_PUB3).c_str()), "false");
    Serial.print("# published ");
    Serial.print((String(MqttTopic + MQTT_PUB3).c_str()));
    Serial.print("/");
    Serial.println("0");
    mqtt_pub3 = true;
  }
  if(keyble->raw_data[1] == 0x01)
  {  
    mqttClient.publish((String(MqttTopic + MQTT_PUB3).c_str()), "true");
    Serial.print("# published ");
    Serial.print((String(MqttTopic + MQTT_PUB3).c_str()));
    Serial.print("/");
    Serial.println("1");
    mqtt_pub3 = true;
  }

  //MQTT_PUB3 rssi
  rssi = keyble->_RSSI;
  char charBuffer3[4];
  String strRSSI =  String(rssi);
  
  strRSSI.toCharArray(charBuffer3, 4);
  mqttClient.publish((String(MqttTopic + MQTT_PUB4).c_str()), charBuffer3);
  mqtt_pub4 = charBuffer3;
  Serial.print("# published ");
  Serial.print((String(MqttTopic + MQTT_PUB4).c_str()));
  Serial.print("/");
  Serial.println(charBuffer3);
         
  Serial.println("# waiting for command...");
}
// ---[MQTT-Setup]--------------------------------------------------------------
void SetupMqtt() {
  while (!mqttClient.connected()) { // Loop until we're reconnected to the MQTT server
    mqttClient.setServer(MqttServerName.c_str(), MqttPort.toInt());
    mqttClient.setCallback(&MqttCallback);
  	Serial.println("# Connect to MQTT-Broker... ");
    if (mqttClient.connect(MqttTopic.c_str(), MqttUserName.c_str(), MqttUserPass.c_str())) {
  		Serial.println("# Connected!");
      mqttClient.subscribe((String(MqttTopic + MQTT_SUB).c_str()));
  	}
  	else {
      Serial.print("!!! error, rc=");
      Serial.println(mqttClient.state());
    }
  }
}
// ---[RootPage]----------------------------------------------------------------
void rootPage()
{
  String  content =
   "<html>"
    "<head>"
     "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  if (mqtt_sub != "\0")
  {
    content +=
    "<meta http-equiv=\"refresh\" content=\"30\"/>";
  }
  content +=
   "</head>"
   "<body>"
   "<h2 align=\"center\" style=\"color:blue;margin:20px;\">KeyBLEBridge Config</h2>"
   "<p></p><p style=\"padding-top:15px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>";

  if (!WiFi.localIP())
  {
    content +=
    "<h2 align=\"center\" style=\"color:blue;margin:20px;\">Please click on gear to setup WiFi.</h2>";
  }
  else
  {
    content +=
    "<h2 align=\"center\" style=\"color:green;margin:20px;\">Connected to: " + WiFi.SSID() + "</h2>"
    "<h2 align=\"center\" style=\"color:green;margin:20px;\">Bride local IP: " + WiFi.localIP().toString() + "</h2>"
    "<h2 align=\"center\" style=\"color:green;margin:20px;\">Gateway IP: " + WiFi.gatewayIP().toString() + "</h2>"
    "<h2 align=\"center\" style=\"color:green;margin:20px;\">Netmask: " + WiFi.subnetMask().toString()  + "</h2>";
  }
  if (!KeyBLEConfigured && WiFi.localIP())
  {
    content +=
    "<h2 align=\"center\" style=\"color:red;margin:20px;\">Enter MQTT and KeyBLE credentials.</h2>"
    "<div style=\"text-align:center;\"><a href=\"/keyble_setting\">Click here to configure MQTT and KeyBLE</a></div>";
  }
  if (KeyBLEConfigured && WiFi.localIP())
  {
    content +=
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">MQTT Server: " + MqttServerName + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">MQTT Port: " + MqttPort + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">MQTT User Name: " + MqttUserName + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">MQTT Topic: " + MqttTopic + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE MAC Address: " + KeyBleMac + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE User Key: " + KeyBleUserKey + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE User ID: " + KeyBleUserId + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE last battery state: " + mqtt_pub3 + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE last command received: " + mqtt_sub + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE last rssi: " + mqtt_pub4 + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE last state: " + mqtt_pub + "</h2>"
     "<h2 align=\"center\" style=\"color:green;margin:20px;\">KeyBLE last task: " + mqtt_pub2 + "</h2>"
     "<br>"
     "<h2 align=\"center\" style=\"color:blue;margin:20px;\">page refresh every 30 seconds</h2>";
  }

  content +=
    "</body>"
    "</html>";
  Server.send(200, "text/html", content);
}
// ---[Wifi Signalquality]-----------------------------------------------------
int GetWifiSignalQuality() {
float signal = 2 * (WiFi.RSSI() + 100);
if (signal > 100)
  return 100;
else
  return signal;
}
// ---[SetWifi]-----------------------------------------------------------------
void SetWifi(bool active) {
  wifiActive = active;
  if (active) {
    WiFi.mode(WIFI_STA);
    Serial.println("# WiFi enabled");
  }
  else {
    WiFi.mode(WIFI_OFF);
    Serial.println("# WiFi disabled");
  }
}
// ---[SetupWiFi]---------------------------------------------------------------
void SetupWifi()
{
  if (Portal.begin())
  {
   if (WiFi.status() == WL_CONNECTED)
   {
     Serial.println("# WIFI: connected to SSiD: " + WiFi.SSID());
   } 
   int maxWait=100;
   while (WiFi.status() != WL_CONNECTED) {
    Serial.println("# WIFI: checking SSiD: " + WiFi.SSID());
    delay(500);
    
    if (maxWait <= 0)
        ESP.restart();
      maxWait--;
  }
  Serial.println("# WIFI: connected!");
  Serial.println("# WIFI: signalquality: " + String(GetWifiSignalQuality()) + "%");
  Serial.println("# WiFi connected to IP: " + WiFi.localIP().toString());
  }
}
// ---[Setup]-------------------------------------------------------------------
void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("---Starting up...---");
  Serial.setDebugOutput(true);
  FlashFS.begin(true);
  Serial.println("---AP Settings---");
  config.apip = IPAddress(AP_IP);
  Serial.print("---AP IP: ");
  Serial.print(config.apip);
  Serial.println("---");
  config.apid = AP_ID;
  Serial.print("---AP SSID: ");
  Serial.print(config.apid);
  Serial.println("---");
  config.psk = AP_PSK;
  config.title = AP_TITLE;
  config.gateway = IPAddress(AP_IP);
  config.ota = AC_OTA_BUILTIN;
  Server.on("/", rootPage);
  Portal.config(config);
  
  if (Portal.load(FPSTR(AUX_keyble_setting))) {
     AutoConnectAux& keyble_setting = *Portal.aux(AUX_SETTING_URI);
     PageArgument  args;
     loadParams(keyble_setting, args);
     config.homeUri = "/_ac";

     Portal.on(AUX_SETTING_URI, loadParams);
     Portal.on(AUX_SAVE_URI, saveParams);
   }
   else
   {
     Serial.println("load error");
   }
   SetupWifi();
   //Portal.config(config);

  
  //MQTT
  if(KeyBLEConfigured)
  {
    SetupMqtt();
    //Bluetooth
    BLEDevice::init("");
    keyble = new eQ3(KeyBleMac.c_str(), KeyBleUserKey.c_str(), KeyBleUserId.toInt());
    //get lockstatus on boot
    do_status = true;
  }
  else
  {
    Serial.println("# Please fill in MQTT and KeyBLE credentials first!");

  }
}
// ---[loop]--------------------------------------------------------------------
void loop() {

Portal.handleClient();  

// This statement will declare pin 0 as digital input 
pinMode(PushButton, INPUT);
// digitalRead function stores the Push button state 
// in variable push_button_state
int Push_button_state = digitalRead(PushButton);
// if condition checks if push button is pressed
// if pressed Lock will toggle state

if (Push_button_state == LOW && WiFi.status() == WL_CONNECTED)
{ 
  do_toggle = true;
   
}
// Wifi reconnect
if (wifiActive)
{
  if (WiFi.status() != WL_CONNECTED)
  {
   Serial.println("# WiFi disconnected, reconnect...");
  SetupWifi();
  }
  else
  {
   // MQTT connected?
   if(!mqttClient.connected())
   {
     if (WiFi.status() == WL_CONNECTED) 
     {
      if(KeyBLEConfigured)
      {
        Serial.println("# MQTT disconnected, reconnect...");
        SetupMqtt();
        if (statusUpdated)
        {
          MqttPublish();
        }
      }
      else
      {
        Serial.println("# Please fill in MQTT and KeyBLE credentials first!");
      }
    }
   }
    else if(mqttClient.connected())
    {
      mqttClient.loop();
    }
  }
}
if (do_open || do_lock || do_unlock || do_status || do_toggle || do_pair) 
{
  String str_task="working";
  char charBuffer4[8];
  str_task.toCharArray(charBuffer4, 8);
  mqttClient.publish((String(MqttTopic + MQTT_PUB2).c_str()), charBuffer4);
  Serial.print("# published ");
  Serial.print((String(MqttTopic + MQTT_PUB2).c_str()));
  Serial.print("/");
  Serial.println(charBuffer4);
  mqtt_pub2 = charBuffer4;
  delay(200);
  SetWifi(false);
  yield();
  waitForAnswer=true;
  keyble->_LockStatus = -1;
  starttime = millis();

  if (do_open)
  {
    Serial.println("*** open ***");
    keyble->open();
    do_open = false;
  }

  if (do_lock)
  {
    Serial.println("*** lock ***");
    keyble->lock();
    do_lock = false;
  }

  if (do_unlock)
  {
    Serial.println("*** unlock ***");
    keyble->unlock();
    do_unlock = false;
  }
  
  if (do_status)
  {
    Serial.println("*** get state ***");
    keyble->updateInfo();
    do_status = false;
  }
  
  if (do_toggle)
  {
    Serial.println("*** toggle ***");
    if ((status == 2) || (status == 4))
    {
      keyble->lock();
      do_lock = false;
    }
    if (status == 3)
    {
      keyble->unlock();
      do_unlock = false;
    }
    do_toggle = false;
   }
   
   if (do_pair)
   {
     Serial.println("*** pair ***");
     //Parse key card data
     std::string cardKey = CARD_KEY;
     if(cardKey.length() == 56)
     {
      Serial.println(cardKey.c_str());
      std::string pairMac = cardKey.substr(1,12);
      
      pairMac = pairMac.substr(0,2)
                + ":" +pairMac.substr(2,2)
                + ":" +pairMac.substr(4,2)
                + ":" +pairMac.substr(6,2)
                + ":" +pairMac.substr(8,2)
                + ":" +pairMac.substr(10,2);
      Serial.println(pairMac.c_str());
      std::string pairKey = cardKey.substr(14,32);
      Serial.println(pairKey.c_str());
      std::string pairSerial = cardKey.substr(46,10);
      Serial.println(pairSerial.c_str());
     }
     else
     {
      Serial.println("# invalid CardKey! Pattern example:");
      Serial.println("  M followed by KeyBLE MAC length 12");
      Serial.println("  K followed by KeyBLE CardKey length 32");
      Serial.println("  Serialnumber");
      Serial.println("  MxxxxxxxxxxxxKxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxSSSSSSSSSS");
     }
     do_pair = false;
   }
  }
  
  if(waitForAnswer)
  {
    bool timeout=(millis() - starttime > LOCK_TIMEOUT *2000 +1000);
    bool finished=false;

    if ((keyble->_LockStatus != -1) || timeout)
    {
      if(keyble->_LockStatus == 1)
      {
        //Serial.println("Lockstatus 1");
        if(timeout)
        {
          finished=true;
          Serial.println("!!! Lockstatus 1 - timeout !!!");
        }
      }
      else if(keyble->_LockStatus == -1)
      {
        //Serial.println("Lockstatus -1");
        if(timeout)
        {
          keyble->_LockStatus = 9; //timeout
          finished=true;
          Serial.println("!!! Lockstatus -1 - timeout !!!");
        }
      }
      else if(keyble->_LockStatus != 1)
      {
        finished=true;
        //Serial.println("Lockstatus != 1");
      }

      if(finished)
      {
        Serial.println("# Done!");
        do
        {
          keyble->bleClient->disconnect();
          delay(100);
        }
        while(keyble->state.connectionState != DISCONNECTED && !timeout);

        delay(100);
        yield();
          
        SetWifi(true);
        
        statusUpdated=true;
        waitForAnswer=false;
      }
    }
  }
}
