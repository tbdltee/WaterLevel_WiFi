void monitorSerial (void) {
  String CMD = "";
  while (Serial.available()) {
    char a = Serial.read();
    if (a == '\0') continue;
    CMDSerial += a;
  }
  while (CMDSerial.indexOf("\r\n") >= 0) {
    CMD = CMDSerial.substring (0, CMDSerial.indexOf("\r\n"));
    CMDSerial.remove (0, CMD.length() + 2);
    if (CMD == "0,status") {               // request ESP8266 status
      CMD = "A,status," + String (ESPstatus);
      Serial.println (CMD);
      delay (80);
    } else if (CMD == "1,power-up") {       // Arduino powerup request
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println ("C,ESP init");
        Serial.println ("C,wifi connected");
      } else {
        delay (100);
        ESP.restart();
      }
    } else if (CMD.startsWith("2,")) {      // get ssid, pass, dhcp enable
      strCMD = getValue(CMD, ',', 1);       // ssid
      strCMD.toCharArray(ssid,32);
      strCMD = getValue(CMD, ',', 2);       // pass
      strCMD.toCharArray(pass,32);
      strCMD = getValue(CMD, ',', 3);       // profile
      devID = getValue(CMD, ',', 4);        // device ID
      SEQNr = getValue(CMD, ',', 5);        // 4 bytes of uint16_t seq number
      if (strCMD == "3G") {
        ESPstatus = 1;                      // wifi info recevied, connecting wifi
      } else if (strCMD == "W0") {          // connect using wifi
        ESPstatus = 6;                      // connect using wifi manager
      } else if (strCMD == "W1") {          // connect using wifi
        ESPstatus = 7;                      // connect using wifi manager
      }
    } else if (CMD == "3") {                // clear received data
      strCMD = "";
      strCMDidx = 0;
      if (ESPstatus > 3) ESPstatus = 3;
    } else if (CMD == "4,shutdown") {       // put ESP into deepsleep before power-off
      Serial.println ("A,Disconnect OK");
      delay (80);
      WiFi.disconnect (true);
      delay (1);
      ESP.deepSleep (10000000, WAKE_RF_DISABLED);
    } else if (CMD.startsWith("6,")) {                                // Data received
      if (ESPstatus >= 2) {                                           // Wifi connected state, receive data
        ESPstatus = 3;                                                // partial data received
        uint8_t CMDidx = (uint8_t) (getValue(CMD, ',', 1).toInt());
        if (CMDidx == 1) {
          strCMDidx = 0;                                              // first piece, reset index
          strCMD = "";
        }
        strCMDidx++;
        if (strCMDidx == CMDidx) {
          strCMD += getValue(CMD, ',', 2);
          Serial.println ("A,CMD RCV");
        } else {
          Serial.println ("B,1,wrong SEQ.");
        }
      } else {
        Serial.println ("B,4,No wifi-Data reject");                   // wifi not connect, reject data
      }
    } else if (CMD.startsWith("7,")) {                                // Executed command
      OTAFailcnt = (uint8_t) (getValue(CMD, ',', 1).toInt());
      if (ESPstatus == 3) {
        if (strCMD.length() > 0) {
          strCMD.replace ("\r", ",");
          sendTxt = strCMD;
          ESPstatus = 4;                                                // completed data received
        } else {
          ESPstatus = 2;
          sendTxt = "";
          Serial.println ("B,2,no data to send.");
        }
        strCMD = "";
        strCMDidx = 0;
      } else {
        Serial.println ("B,3,No data received");
      }
    } else {
      Serial.println ("B,9,Unknown CMD:" + CMD);
    }
  }
}

// ========================== Utility functions ==========================================
String getValue(String data, char separator, int index) {   //http://stackoverflow.com/questions/9072320/split-string-into-string-array
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
