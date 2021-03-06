# Wireless Water Level measurement
This project is using HC-SR04p ultrasonic to measure the water level and send to internet dashboard (using node-red).

## Current status: up
 * dashboard url: www.tbdltee.cloudns.cl:8890
 * Linenoify service was inactive. Plan to migrate to LINE-bot for notification. 
 
## Firmware Upload Status (Arduino/ESP8266)

* IOT001D0001 - 00410/00410/SDK2.2.1-Boot1.7/Core 2.6.3
* IOT001D0002 - 0040E/0040E/SDK2.2.1-Boot1.7/Core 2.4.2
* IOT001D0003 - 0040E/0040E/SDK2.2.1-Boot1.7/Core 2.6.3

## Release History
* 0040G
  * change server from tbdltee.thddns.net to myiot.tbdltee.cloudns.cl
  * dashboard url: www.tbdltee.cloudns.cl:8890
* 0040F
  * improve TCP response handling speed.
  * change iot server from tbdltee.duckddns.org:10722 to tbdltee.thddns.net:8895
  * change OTA server from tbdltee.duckddns.org:8890 to tbdltee.thddns.net:8890
  * BME280 auto reset if cannot read data for consecutive 5 times
  * Support Submersible 4-20mA Water Level Sensor
* 0040E
  * Support remote reboot device from Dashboard
* 0040D
  * fixed: WiFiConfig portal handling
	* WiFiConfig redirect page to root if page not found.
	* WiFiConfig portal show WiFi signal strength bar instead of text
	* ESP is now graceful shutdown. Make WiFi connection more stable.
	* Increase TCP timeout from 10s to 15s
	* Increase UDP timeout from 3s to 5s
	* restart node if cannot connect to internet for 1 day.
  * 3G TxiNET_LowBatt from 10 hrs to 6 hrs.
  * bugs fixed (Arduino) in RainCount debounce using millis(). millis() is not working while sleep. Change millis() to Debounce_counter
	* bugs fixed (Arduino) in %Batt calculation. limited min/max Volt to 3400/4100.
	* Arduino: Sent lastCM to internet instead of average value. Last 5 values use for STD DV calculation.
  * Rain_debounce_counter set to 150ms
  * Timer adjustment
  * Change method to get Batt level. Measure Vcc for stability and use Max A2-value as 100%. Decrease maxADC 50mV for every 7 days.
	* Increase Batt_0% from 3.4v to 3.6v to increase stability at low batt mode
* 0040C
  * Measure ADC of internal reference 1.1v and calc back to A2-ADC.
  * Support Battery auto-calibration.
  * Bug fixed when Tx come back from Power-Off state
* 0040B
  * Use hardware jumpper to select between 3G and WiFi profile
  * Change WiFi update interval from 5 to 10 min
  * Update with TCP, then use UDP if TCP failed.
  * Change device key to Sequence Number
  * device is sent with raw distance data. Water level is calculated on the server.
  * Support line notification
  * Support only BME280 to simplify the code. 
  * Support hPA measurement from BME280
  * Change tempC value from 0..100 to tempC+40 (0..125 -> -40..85) to support full range of BME280
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
* Security Type: WPA2-Personal, WPA-algorithm: TKIP+AES
* DHCP: off
* 3G dongle IP/Subnet: 192.168.8.1/255.255.255.0

Configure Line Notification (de-preciated. Replaced by LINE-bot):
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
