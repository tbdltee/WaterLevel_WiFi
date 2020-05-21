void sendData2iNet (void) {
  String CMD = "", FWver = "";
  
  CMD = sentTCPdata ();
  if (CMD.startsWith ("B,")||CMD.startsWith ("A,Sent OK,15,")) {  // tcp error or no response try UDP
    uint8_t retryNr = 1;
    while (retryNr < 3) {
      CMD = sentUDPdata (retryNr);
      if (CMD.startsWith("A,Sent OK,15,")) {        // UDP response timeout retry
        retryNr++;
      } else {                                      // UDP sent success or error, exit
        break;
      }
    }
  }
/* CMD = "A,Sent OK,result
 * result: 
 *   0,sKey,0 --> OK
 *   1,sKey,fotaFW,0 --> ok with fota
 *   2,sKey,0 --> OK-node restart
 *   11,invalid parameter,0
 *   13,device not found,0
 */
  String rcvKeyOK = "";
  if (CMD.startsWith ("A,Sent OK,")) {
    rcvKeyOK = getValue (CMD, ',', 3);         // check received key
    if (rcvKeyOK.startsWith(SEQNr)) rcvKeyOK = "OK";
  }
  if (CMD.startsWith ("A,Sent OK,1,")) {            // required OTA
    FWver = getValue (CMD, ',', 4);                 // new FW version
    if (rcvKeyOK == "OK") {
      if ((FWver.length() == 5) && (FWver.charAt(0) == FW_version.charAt(0))) {
        if (OTAallowCnt > 0) {
          CMD.replace ("A,Sent OK,","A,Sent OK-OTA,");
        } else {
          CMD.replace ("A,Sent OK,","A,Sent OK-done,OTA too many attempts,");
        }
      } else {
        CMD.replace ("A,Sent OK,","A,Sent OK-done,OTA invalid version,");
      }
    } else {
      CMD.replace ("A,Sent OK,","A,Sent OK-done,OTA key mismatch,");
    }
  } else if ((CMD.startsWith ("A,Sent OK,2,")) && (rcvKeyOK=="OK")) {    // required node restart
    CMD.replace ("A,Sent OK,","A,Sent OK-done,restart,");
  } else if ((CMD.startsWith ("A,Sent OK,3,")) && (rcvKeyOK=="OK")) {    // re-init ADC4mA
    CMD.replace ("A,Sent OK,","A,Sent OK-done,resetADC4mA,");
  }
  CMD.replace ("A,Sent OK,","A,Sent OK-done,");
  Serial.println (CMD);
  if (CMD.startsWith ("A,Sent OK-OTA,")) fwUpgrade (FWver);
  sendTxt = "";
}

String sentTCPdata (void) {                                 // return sever response
  WiFiClient tcpClient;

  if (!tcpClient.connect(Host_addr, dataPort)) {            // host connected
    Serial.println ("C,Host connect error.");
    return "B,6,Host connect error";
  }
  
  Serial.println ("C,Host connected,Sending TCP data...");
  String tcpData = SEQNr + "5," + FW_version + "," + sendTxt + "\n";
  tcpClient.print (tcpData);                               // send tcp data
  
  String result = "";
  uint32_t prevtime = millis();
  while ((millis() - prevtime) < 15000) {                  // wait for TCP packet received for 15 sec
    if (tcpClient.available()) {
      char a = (char)tcpClient.read();
      if (a =='\n') break;
      if (a =='\0') continue;
      result += a;
      delay(10);                                             // delay will make esp to be able to detect \n
    }
  }
  tcpClient.stop();
  
  if (result.length() > 0) {                                // data received
    Serial.println("C,TCP received: " + result);
    return "A,Sent OK," + result;
  }
  return "A,Sent OK,15,no resposne";                        // no response from TCP server
}

String sentUDPdata (uint8_t RetryNr) {
  char incomingUDP[255];                                    // buffer for incoming UDP packets
  
  Serial.print ("C,Sending UDP data-" + String(RetryNr) + "..");
  
  String udpData = SEQNr + String(RetryNr+5) + "," + FW_version + "," + sendTxt;

  udpClient.beginPacket(Host_addr, dataPort);
  udpClient.write(udpData.c_str());
  if (!udpClient.endPacket()) {
    Serial.println ("failed");
    return "B,7,Sent UDP fail";
  }
  Serial.println ("ok");
  
  uint32_t prevtime = millis();
  int packetSize = 0;
  while ((millis() - prevtime) < 5000) {                          // wait for UDP packet received for 5 sec
    packetSize = udpClient.parsePacket();
    if (packetSize > 0) {
      int len = udpClient.read(incomingUDP, 255);
      if (len > 0) incomingUDP[len] = 0;                          // terminate last char with char(0)
      String respText = String(incomingUDP);
      Serial.println("C,UDP received: " + respText);
      return "A,Sent OK," + respText;
    }
  }
  return "A,Sent OK,15,no resposne";                               // no response from UDP server
}

void ResolveHostName (void) {
  IPAddress newHost_addr;
  if (ESPstatus >= 10) {                                        // if wifi connected and CMD RCV
    ESPstatus = 10;
    uint8_t retry = 0;
    Serial.println ("C,Resolving Hostname...");
    uint32_t starttime = millis();
    while ((retry < 3) && (ESPstatus == 10)) {
      if (WiFi.hostByName(ServerHost, newHost_addr)) {          // Resolving host name OK
        if ((millis() - starttime) < 250) delay (250);          // resolve success too fast. add some delay
        if (Host_addr == newHost_addr) {
          Serial.println ("C,resolve ok-" + String (retry) + ",same HostIP: " + Host_addr.toString());
        } else {
          Serial.println ("C,resolve ok-" + String (retry) + ",new HostIP: " + Host_addr.toString());
          Host_addr = newHost_addr;                             // Update Host IP with new IP address
          SaveHostAddr();
        }
        ESPstatus = 11;
        break;
      } else {
        if (retry < 2) delay (5000);
      }
      retry++;
    }
    if (ESPstatus == 10) {
      if (Host_addr == IPAddress(255,255,255,255)) {
        Serial.println ("B,5,Host lookup error, no Host IP");
      } else {
        ESPstatus = 11;
        Serial.println ("C,resolve error,use " + Host_addr.toString());
      }
    }
  } else {
    Serial.println ("B,5,Host lookup error,ESPstate-" + String(ESPstatus));
  }
}

void SaveHostAddr (void) {          // save host address on byte 0..15
  EEPROM.begin(80);
  for (uint8_t i = 0; i<4; i++) {
    EEPROM.write (i, Host_addr[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void ReadHostAddr (void) {          // read host address on byte 0..15
  EEPROM.begin(80);
  for (uint8_t i = 0; i<4; i++) {
    Host_addr[i] = EEPROM.read (i);
  }
  EEPROM.end();
}

void fwUpgrade (String newFW_version) {
  IPAddress otaHost_addr;
  
  Serial.println ("C,OTA begin: " + FW_version +" -> " + newFW_version);
  if (!WiFi.hostByName(fotaUrlBase, otaHost_addr)) {          // Resolving host name failed
    if (Host_addr == IPAddress(255,255,255,255)) {            // also no default host address
      Serial.println ("C,OTA err resolve hostname,skipped.");
      printDebug ("OTA,OTA error,"+ FW_version +"," + newFW_version + ",resolve DNS");
      return;
    } else {
      Serial.println ("C,cannot resolve OTA hostname. Use " + Host_addr.toString() + "\r\n");
      otaHost_addr = Host_addr;                               // Update Host IP with new IP address
    }
  }
  printDebug ("OTA,OTA begin," + FW_version +"," + newFW_version + ",started");
  t_httpUpdate_return ret = ESPhttpUpdate.update(otaHost_addr.toString(), fotaPort, "/.api/.fota/ESP" + newFW_version + ".bin", devID);
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("C,OTA err," + ESPhttpUpdate.getLastErrorString());
      printDebug ("OTA,OTA error,"+ FW_version +"," + newFW_version + "," + ESPhttpUpdate.getLastErrorString());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("C,OTA err,skipped.");
      printDebug ("OTA,OTA error,"+ FW_version +"," + newFW_version + ",no update");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("C,OTA OK.");            // may not called we reboot the ESP
      printDebug ("OTA,OTA success,"+ FW_version +"," + newFW_version + ",restart");
      delay (500);
      WiFi.disconnect ();
      delay (1);
      ESP.deepSleep (10000000L, WAKE_RF_DISABLED);
      WiFi.forceSleepBegin();
      delay (30000);
      ESP.restart();
      break;
  }
}

void printDebug (String txt) {
  txt = "debug,"+devID + ","+ txt;
  udpClient.beginPacket(ServerHost, dataPort);
  udpClient.write(txt.c_str());
  udpClient.endPacket();
}
