# esp32-keyble
ESP32 port of the keyble library
working, with some additions!

Thanks go to <a href="https://github.com/RoP09">RoP09</a>, <a href="https://github.com/tc-maxx">tc-maxx</a>, <a href="https://github.com/henfri">henfri</a>, <a href="https://github.com/MariusSchiffer">MariusSchiffer</a> and of course <a href="https://github.com/oyooyo">oyooyo</a> for their brillant work!


Took RoP09/tc-maxx last update that was posted here: https://www.mikrocontroller.net/topic/458856#6650683 and here https://github.com/tc-maxx/esp32-keyble

Fiddled around a bit to get it to run more stable.

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

Step by step setup:

- Erase the flash first because of SPIFFS usage and maybe stored old WiFi credentials.
- Upload the project (I use Platformio).
- Connect to the ESP32's WiFi network.
- By default the SSID is "KeyBLEBridge" with default password "eqivalock".
- After you have connected to the network, go to 192.168.4.1 with you browser.
- At rootlevel you will just see a simple webpage.
- Click on the gear to connect to your WiFi Network.
- Now you will see the AutoConnect portal page.
- Go to "Configure new AP" to enter your credentials.
- AutoConnect scans for networks in reach, just choose the one you want to connect to.
- I recommend you to disable DHCP and give the bridge a static IP.
- The ESP32 reboots, so you have to access it with the new given IP.
- Follow the link and enter MQTT and KeyBLE credentials.
- After saving the credentials click on "Reset" to reboot.
- You well be redirected to the main page of AutoConnect.
- Click on "Home" to see the entered credentials.
- Now everthing is set up and the ESP publishes its states to the given MQTT Broker.
- It publishes the state once on startup and has then to be triggert from outside.
- I use Node Red to get the locks state every 25 seconds to check for changes.

The bridge publishes to the given topic you entered at setup.

Endpoints are:

- /battery true for good, false for low
- /command 1 for status, 2 to unlock, 3 to lock, 4 to open and 5 to toggle)
- /rssi signalstrength
- /state as strings; locked, unlocked, open, moving, timeout
- /task working; if the lock has already received a command to be executed, waiting; ready to receive a command

The /task endpoint is usefull, because the ESP toggles between WiFi and BLE connections. If the bridge is connected to WiFi and recieves a command via MQTT, it disables WiFi, connects to the lock via BLE, reconnects after the BLE task has finished and publishes the new state to the mqtt broker.

TODO
- register user feature
- error handling
- more endpoints like IP, uptime, etc.
- a command queue topic to get commands while the bridge was busy
- a nice 3D printed housing

Beside the ESP32 solutions, I have a modded verison of oyooyo's first implementaion running on a Pi Zero, with 5 locks permanently connected at the same time a (Batterys last around 2 month). The locks are managed with Node Red and integrated in a Loxone Smart Home with a Doorbird and an external RFID reader. If I have time I will upload this to github too.


Have fun!
