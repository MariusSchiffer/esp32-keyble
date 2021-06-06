# esp32-keyble
ESP32 port of the keyble library
working, with additions!

Took tc-maxx/Rop09 last update that was posted here: https://www.mikrocontroller.net/topic/458856#6650683

Fiddled around a bit to get it to run stable.

Some changes/additions made:

- RSSI value for BLE connection
- battery state
- toggle function for lock
- AP-Mode and config Portal via AutoConnect
- MQTT endpoints for state, task and battery 
- OTA update to upload bin files
- removed hardcoded credentials
- serial outputs in english
- boot button on the ESP32 board toggles the lock
- changed partion table

How does it work now?

Erase the flash first because of SPIFFS usage and maybe stored old WiFi credentials.
Upload the project (I use Platformio).
Connect to the ESP32's WiFi network.
By default the SSID is "KeyBLEBridge" with default password "eqivalock".
After you have connected to the network, go to 192.168.4.1 with you browser.
At rootlevel you will just see a simple webpage.
Click on the gear to connect to your WiFi Network.
Now you will see the AutoConnect portal page.
Go to "Configure new AP" to enter your credentials.
AutoConnect scans for network, just choose the one you want to connect to.
I recommend you to disable DHCP and give the bridge a static IP.
The ESP32 reboots, so you have to access it with the new given IP.
Follow the link and enter MQTT and KeyBLE credentials.
After saving the credentials click on "Reset" to reboot.
You well be redirected to the main page of AutoConnect.
Click on "Home" to see the entered credentials.
Now everthing is set up the ESP publishs its states to the given MQTT Broker.
It puplishes messages once on startup and has to be triggert from outside.
I use Node Red to get the locks state every 25 seconds to check for changes.

The bridge puplishes to the given topic you entered at setup.

Endpoints are:

- /battery 1 for good, 0 for empty
- /command 1 for status, 2 to unlock, 3 to lock, 4 to open and 5 to toggle)
- /rssi signalstrength
- /state as strings; locked, unlocked, open, unknown
- /task working; if the lock has already received a command to be executed, waiting; ready to receive a command

The /task endpoint is usefull, because the ESP toggles between WiFi and BLE connections. If the bridge is connected to WiFi and recieves a command via MQTT, it disables WiFi, connects to the lock via BLE and reconnects after the BLE task has finished and publishes the new state to the mqtt endpoints.

TODO
- register user feature
- error handling
- a nice 3D prited housing

Have fun!







