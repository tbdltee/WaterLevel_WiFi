void sendData2iNet (void) {
  String CMD = "", FWver = "";
  
  CMD = sentTCPdata ();
  if (CMD.startsWith ("B,")||CMD.startsWith ("A,Sent OK,15,")) {  // tcp error try UDP
    uint8_t retryNr = 1;
    while (retryNr < 3) {
      CMD = sentUDPdata (retryNr);
      if (CMD.startsWith("A,Sent OK,15,")) {        // already wait 5s to response, no further delay required.
        retryNr++;
      } else {
        retryNr = 10;                               // exit while {}
      }
    }
  }
  if (CMD.startsWith ("A,Sent OK,1,")) {            // required OTA
    String rcvKey = getValue (CMD, ',', 3);         // check received key
    FWver = getValue (CMD, ',', 4);                 // new FW version
    if (rcvKey.startsWith(SEQNr)) {
      if ((FWver.length() == 5) && (FWver.charAt(0) == FW_version.charAt(0))) {
        if (OTAFailcnt > 0) {
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
  }
  CMD.replace ("A,Sent OK,","A,Sent OK-done,");
  Serial.println (CMD);
  if (CMD.startsWith ("A,Sent OK-OTA,")) fwUpgrade (FWver);
  sendTxt = "";
}

String sentTCPdata (void) {                                 // return sever response
  WiFiClient tcpClient;

  if (!tcpClient.connect(ServerHost, dataPort)) {           // host connected
    Serial.println ("C,Host connect error.");
    return "B,6,Host connect error";
  }
  
  Serial.println ("C,Host connected,Sending TCP data...");
  String tcpData = SEQNr + "5," + FW_version + "," + sendTxt + "\n";
  tcpClient.print (tcpData);                               // send tcp data
  
  String result = "";
  uint32_t prevtime = millis();
  while ((millis() - prevtime) < 5000) {                    // wait for TCP packet received for 5 sec
    if (tcpClient.available()) {
      char a = tcpClient.read();
      if (a =='\0') continue;
      if (a =='\n') break;
      result += a;
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
  WiFiUDP udpClient;
  char incomingUDP[255];                                          // buffer for incoming UDP packets
  
  udpClient.begin(udpEchoPort);
  Serial.print ("C,Sending UDP data-" + String(RetryNr) + "..");
  
  String udpData = SEQNr + String(RetryNr+5) + "," + FW_version + "," + sendTxt;

  udpClient.beginPacket(Host_addr, dataPort);
  udpClient.write(udpData.c_str());
  if (!udpClient.endPacket()) {
    Serial.println ("failed");
    return "B,5,Sent fail";
  }
  Serial.println ("ok");
  
  uint32_t prevtime = millis();
  int packetSize = 0;
  while ((millis() - prevtime) < 3000) {                          // wait for UDP packet received for 3 sec
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
    while ((retry < 3) && (ESPstatus == 10)) {
      if (WiFi.hostByName(ServerHost, newHost_addr)) {          // Resolving host name
        if (Host_addr == newHost_addr) {
          Serial.println ("C,resolve ok-" + String (retry)+",same Host IP: " + Host_addr.toString());
        } else {
          Host_addr = newHost_addr;                             // Update Host IP with new IP address
          SaveHostAddr();
          Serial.println ("C,resolve ok-" + String (retry)+",new Host IP: " + Host_addr.toString());
        }
        ESPstatus = 11;
      } else {
        if (retry < 2) delay (5000);
      }
      retry++;
    }
    if (ESPstatus == 10) {
      if (Host_addr == IPAddress(255,255,255,255)) {
        Serial.println ("B,6,Hostname lookup error, no Host IP.");
      } else {
        ESPstatus = 11;
        Serial.println ("C,resolve error,use " + Host_addr.toString());
      }
    }
  } else {
    Serial.println ("B,4,Hostname lookup error,ESPstate-" + String(ESPstatus));
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
      return;
    } else {
      Serial.println ("C,cannot resolve OTA hostname. Using " + Host_addr.toString() + "\r\n");
      otaHost_addr = Host_addr;                               // Update Host IP with new IP address
    }
  }
  t_httpUpdate_return ret = ESPhttpUpdate.update(otaHost_addr.toString(), fotaPort, "/.fota/ESP" + newFW_version + ".bin", devID);
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("C,OTA err," + ESPhttpUpdate.getLastErrorString());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("C,OTA err,skipped.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("C,OTA OK.");            // may not called we reboot the ESP
      delay (500);
      ESP.restart();
      break;
  }
}
