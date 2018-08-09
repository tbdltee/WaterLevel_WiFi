void updateData (void) {      // true: update success
  String sendURL = "";
  uint8_t result = 0;
  
  iNetTx.iNETattempt++;
  iNetTx.seqNr++;
  if (iNETPowerUp() == false) {
    TxData.distanceIdx = 0;
    return;
  }
  iNETSerialmsg = "";
  getSensorData();
  TxData.distanceIdx = 0;
  
  // URL: deviceID,distance,batt,TempC/RH,RainMMx10,NodeStatus/Attempt/DNSerr/NoResp/Senterr/WifiErr/Hosterr
  sendURL = sysPara.DevID + "\r" + String(iNetTx.distanceCM) + "\r" + BYTE2HEX(iNetTx.BattLvl);
#if (SENSOR_W > 0)
  sendURL += BYTE2HEX(iNetTx.TempC) + BYTE2HEX(iNetTx.RH);
#else
  sendURL += "FFFF";
#endif
  
  TxData.RainCount        = RainCount;
  iNetTx.RainCount = (uint16_t)(calcRainMM(TxData.RainCount) * 10);    // RainMM x10
  
//==================================== Dummy Code ====================================
  iNetTx.RainCount = random(0, 50);
//====================================================================================

#if (SENSOR_RAIN == 1)
  sendURL += uint16_t2HEX (iNetTx.RainCount);
#else
  sendURL += "FFFF";
#endif

  sendURL += "\r" + BYTE2HEX(sysPara.NodeStatus);
  sendURL += BYTE2HEX(iNetTx.iNETattempt);
  sendURL += BYTE2HEX(iNetTx.iNETDNSfail);
  sendURL += BYTE2HEX(iNetTx.iNETNoResp);
  sendURL += BYTE2HEX(iNetTx.iNETSentfail);
  sendURL += BYTE2HEX(iNetTx.iNETWififail);
  sendURL += BYTE2HEX(iNetTx.iNETHostfail);
  sendURL = String("6,1,") + sendURL;
  sysPara.NodeStatus &= 0xF3;                     // Clear Rapid Update/No Reponse flag (bit 2,3)
  
  printDEBUG ("[N] Upload data...");
  
  uint8_t ESPstate = 0, counters = 0;
  uint32_t prevtime = millis(), totalTime = 60000;
  while (((millis() - prevtime) < totalTime) && (ESPstate < 10)) {    // allow 60s to send data
    if (ESPstate == 4) {                          // DNS ok, wait for response, triple flash
      result = monitoriNET (1000);
      LEDflash(); delay (100); LEDflash(); delay (100); LEDflash();
    } else if (ESPstate == 5) {                   // OTA state, fast flash
      result = monitoriNET (250);
      LEDflash();
    } else {                                      // wait for DNS,double flash
      result = monitoriNET (1000);
      LEDflash(); delay (100); LEDflash();
    }
    if (ESPstate == 0) {                          // sending data state
      iNETSerial.println (sendURL);
      ESPstate = 1;                               // data sent completed
      counters = 0;
    } else if (ESPstate == 1) {                   // sending data state
      if (result == 15) {                         // "A,CMD RCV"
        ESPstate = 2;
      } else {
        counters++;                               // increase counter
      }
      if (counters >= 5) return false;
    } else if (ESPstate == 2) {                   // sending data state
      printDEBUG ("[N] Sending data...");
      iNETSerial.println ("7," + String(OTAallowCnt));
      ESPstate = 3;
    } else if (ESPstate == 3) {                   // data sent, wait for dns resolver
      if (result == 17) {                         // DNS resolve ok
        ESPstate = 4;
      } else if (inRange (result, 21, 28)) {      // Error result
        iNetTx.iNETHostfail++;
        iNetTx.iNETSentfail++;
        printDEBUG ("[N] Send Error");
        ESPstate = 12;                            // exit while {} with error
      }
    } else if (ESPstate == 4) {                   // DNS ok, wait for server response
      if (result == 16) {                         // sent ok
        ESPstate = 11;                            // exit while {} with success
        RainCount -= TxData.RainCount;
        resetTxData ();
        sysPara.NodeStatus &= 0x7F;               // clear node reboot flag
        printDEBUG ("[N] Send OK");
      } else if (result == 31) {                  // OTA
        ESPstate = 5;
        RainCount -= TxData.RainCount;
        resetTxData ();
        sysPara.NodeStatus &= 0x7F;               // clear node reboot flag
        printDEBUG ("[N] Send OK");
        cntRemaining = 620;                       // ensure no watchdog reset during OTA
        totalTime    = 600000;                    // change total wait time to 300s
      } else if (inRange (result, 21, 28)) {      // Error result
        iNetTx.iNETHostfail++;
        iNetTx.iNETSentfail++;
        printDEBUG ("[N] Send Error");
        ESPstate = 12;                            // exit while {} with error
      }
    } else if (ESPstate == 5) {                   // OTA
      if ((result == 33) && (OTAallowCnt > 0)) {  // OTA failed
        OTAallowCnt--;
        ESPstate = 11;
        printDEBUG ("[N] OTA error.");
      } else if ((result == 11) || (result == 32)) {    // OTA success
        OTAallowCnt = OTA_ATTEMPT_ALLOW;
        ESPstate = 13;
        printDEBUG ("[N] OTA done.");
      }
    }
  }
  iNETPowerDown(5);                             // Power-down internet Module
}

bool iNETPowerUp (void) {                       // Power-up ESP and wait for connection
  uint8_t ESPstate = 0, timeCnt = 0, monResult = 0, hostErr = 0;
  uint32_t PwrUpTime = (uint32_t)ModemPwrUpTime * 1000.0;
  
  printDEBUG ("[N] Power-up modem.");
  if (Device_Profile=="3G") digitalWrite(MODEM_ENpin, HIGH);
  ESPstate = 1;                             // Powering-up modem
  timeCnt = ModemWaitTime;                  // wait for a while before start ESP
  uint32_t starttime = millis();
  while (((millis() - starttime) < PwrUpTime) && (ESPstate < 10)) {   // loop for 50sec
    LEDflash ();
    if (ESPstate == 4) {
      delay (100); LEDflash ();
    }
    monResult = monitoriNET (1000);
    if (ESPstate == 1) {                    // power-up modem state
      if (timeCnt == 0) {
        digitalWrite(ESP_ENpin, HIGH);
        timeCnt = 3;
        ESPstate = 2;                       // move to Powering-up ESP state
      }
    } else if (inRange(ESPstate,2,3)) {     // Powering-up ESP state
      if ((timeCnt == 0) && (ESPstate == 2)) {    // iNet monitor time-out
        iNETSerial.println ("0,status");    // request status
        timeCnt = 5;
      }
      if (monResult == 11) {                // ESP init received
        String txt = sysPara.ssid+","+sysPara.pass+","+((Device_Profile=="WL")?((sysPara.WiFiConfTime>0)?"W1":"W0"):Device_Profile)+"," + sysPara.DevID+","+uint16_t2HEX(iNetTx.seqNr);
        iNETSerial.println ("2," + txt);
        printDEBUG ("[N] Config sent...");
        ESPstate = 3;
      } else if (monResult == 12) {         // wifi connected received
        ESPstate = 11;                      // exit while {}
      } else if (monResult == 41) {         // ESP enter config mode
        ESPstate = 4;
      }
    } else if (ESPstate == 4) {             // ESP config mode
      if (sysPara.WiFiConfTime > 0) {       // if enter WiFi config for first use
        PwrUpTime = (uint32_t)sysPara.WiFiConfTime * 1000.0;     // increase modem PwrUp Time
        cntRemaining = sysPara.WiFiConfTime;                     // increase watchdog
      }
      if (monResult == 11) {                // ESP init received
        String txt = sysPara.ssid+","+sysPara.pass+","+((Device_Profile=="WL")?((sysPara.WiFiConfTime>0)?"W1":"W0"):Device_Profile)+"," + sysPara.DevID+","+uint16_t2HEX(iNetTx.seqNr);
        iNETSerial.println ("2," + txt);
        printDEBUG ("[N] Config sent...");
        ESPstate = 3;
      } else if (monResult == 12) {         // Wifi connected
        ESPstate = 11;                      // exit while {}
      } else if (monResult == 42) {         // Config portal time-out
        ESPstate = 21;
      }
    }
    timeCnt--;
  }
  
  if (ESPstate == 2) {
    printDEBUG ("[N] ESP init error.");
  } else if ((ESPstate == 3)||(ESPstate > 20)) {
    printDEBUG ("[N] Wifi connect error.");
    iNetTx.iNETWififail++;
  } else if (ESPstate == 11) {             // modem power-on successfully
    sysPara.WiFiConfTime = 0;
    if (CMDdelayTime > 0) {
      printDEBUG ("[N] pause " + String(CMDdelayTime) +"s");
      timeCnt = CMDdelayTime;
      while (timeCnt > 0) {
        LEDflash ();
        monResult = monitoriNET (1000);
        timeCnt--;
      }
    }
    LEDoff ();                             // turn-off LED
    return true;
  } else {
    printDEBUG ("[N] Error ESP state: " + String (ESPstate));
  }
  iNetTx.iNETSentfail++;
  iNETPowerDown(ESPstate);
  return false;
}

void iNETPowerDown (uint8_t ESPstate) {
  if (ESPstate > 1) iNETSerial.println("4,shutdown"); // send ESP shutdown command
  uint8_t result = monitoriNET (1000);                // wait 1 sec
  delay (200);                                        // ensure ESP in deep sleep
  digitalWrite(ESP_ENpin, LOW);
  digitalWrite(MODEM_ENpin, LOW);
  LEDoff();                                           // turn-off LED
  printDEBUG ("[N] iNET Power-down.");
  printDEBUG ("[N] Attempt: "+String (iNetTx.iNETattempt)+", NoResp: "+String(iNetTx.iNETNoResp)+", failed: "+String(iNetTx.iNETSentfail)+" [Wifi: "+String(iNetTx.iNETWififail)+", Host: "+String (iNetTx.iNETHostfail)+"]");
}

uint8_t monitoriNET (uint32_t monitorTime) {         // monitor iNET Serial for monitorPeriod sec
  String iNETmsg = "", strCMD = "";
  uint32_t starttime = millis();

  while (((millis() - starttime) < monitorTime) || (iNETSerial.available())) {
    while (iNETSerial.available()) {
      char a = iNETSerial.read();
      if (a == '\0') continue;
      iNETSerialmsg += a;
    }
    while (iNETSerialmsg.indexOf("\r\n") >= 0) {
      iNETmsg = iNETSerialmsg.substring (0, iNETSerialmsg.indexOf("\r\n"));
      iNETSerialmsg.remove (0, iNETmsg.length() + 2);
      if (iNETmsg.length() > 0) printDEBUG ("[R] " + iNETmsg);

      if (iNETmsg.startsWith ("C,ESP init"))                return 11;
      if (iNETmsg.startsWith ("C,wifi ready"))              return 12;
      
      if (iNETmsg.startsWith ("A,Disconnect OK"))           return 14;
      if (iNETmsg.startsWith ("A,CMD RCV"))                 return 15;
      if (iNETmsg.startsWith ("A,Sent OK-done")) {
        if (iNETmsg.startsWith ("A,Sent OK-done,15")) {
          iNetTx.iNETNoResp++;
          sysPara.NodeStatus |= 0x40;                       // set No response bit
        }
        return 16;
      }
      if (iNETmsg.startsWith ("C,resolve ok"))              return 17;
      if (iNETmsg.startsWith ("C,resolve error")) {         // resolve error, use default Host IP
        iNetTx.iNETDNSfail++;
        return 17;
      }
      
      if (iNETmsg.startsWith ("B,")) {
        if (iNETmsg.startsWith ("B,6,")) iNetTx.iNETDNSfail++;
        return 21;
      }
      
      if (iNETmsg.startsWith ("A,Sent OK-OTA,"))            return 31;
      if (iNETmsg.startsWith ("C,OTA OK"))                  return 32;
      if (iNETmsg.startsWith ("C,OTA err"))                 return 33;

      if (iNETmsg.startsWith ("C,Enter wifi config mode"))  return 41;
      if (iNETmsg.startsWith ("C,Config time-out"))         return 42;
      if (iNETmsg.startsWith ("C,Config ok")) {
        sysPara.ssid = getValue (iNETmsg, ',', 2);
        sysPara.pass = getValue (iNETmsg, ',', 3);
        return 43;
      }
      
      if (iNETmsg.startsWith ("A,status,")) {
        uint8_t result = (uint8_t)getValue (iNETmsg, ',', 2).toInt();
        if (result == 0) return 11;                         // ESP inited
        if (result == 2) return 12;                         // wifi connected
      }
    }
  }
  return 99;  // timeout
}
