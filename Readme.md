# Wireless Water Level measurement
This project is using HC-SR04p ultrasonic to measure the water level and send to internet dashboard (using node-red).

## Release History
* 0040B
  * Use hardware jumpper to select between 3G and WiFi profile
  * Change WiFi update interval from 5 to 10 min
  * Update with TCP, then use UDP if TCP failed.
  * Change device key to Sequence Number
  * device is sent with raw distance data. Water level is calculated on the server.
  * Support line notification
  * Support only BME280 to simplify the code. 
* 00400
  * release based on Water_Level_v4

## About the code
The code consists of 4 sketches:
* Arduino_WiFi:       this is a code for Arduino Pro-Mini. Upload the code using USB-TTL adapter.
* ESP_NodeRed_WiFi:   this is a code for ESP8266-01. Upload the code using USB-TTL adapter or HTTP OTA.
* esp_fota.js:        NodeJS app for http OTA update.
* node-red_flow.json: Node-Red flow

Configure 3G-WiFi USB dongle for deivceID IOT-xxxxDyyy:
* ssid: IOT-xxxxDyyy-3G
* pass: IOT-xxxxDyyy-3G
* ssid broadcast: off
* DHCP: off
* 3G dongle IP/Subnet: 192.168.8.1/255.255.255.0

Configure Line Notification:
* Add 'Line Notify' as friend in LINE app.
* goto https://notify-bot.line.me/th/ and log-in with your LINE id
* Click menu on  the top-right of the page and select  'My Page'
* Click 'Generate Token'
* Enter 'Token name' and chat group to notify.
  * Token Name: My IOT
  * Chat Group: 1-on-1 chat with Line Notify
  * Click 'Generate Token'
* copy the token and update in the Deivce_list databased
* if no line token is define, LINE nitification will be disable for those devices.

## How to use:
* Each device has it own Device ID (IOT-xxxxDyyy)
  * Group ID: IOT-xxxx
  * ID: Dyyy
* Select internet access profile using Jumper:
  * open: 3G profile
  * short: WiFi profile
  * when WiFi profile is select, the previous WiFi configuration is used.
* 3G configutation: none
* WiFi configuration:
  * Power-off and then on the device
  * Wait until device enter config mode
  *	Connect to ssid: iotConfAP, pass:iot1234 (if ask)
  *	Open browser and goto http://192.168.4.1
  * Enter wifi info, SAVE and wait for device to reboot
* LED status:
  * Fast blink:   OTA
  * Flash:        modem power up
  * Double flash: Connecting to internet
  * Triple flash:	sending data
  
## Author

Teerapong S. –  [tbdltee@gmail.com]

Patent Pending (Reg.1701003801)
