void updateData (void) {
  String sendURL = "";
  uint8_t result = 0;
  
  iNetTx.iNETattempt++;               //  increase attempt counter
  iNetTx.seqNr++;                     //  increase Seq Nr.
  iNETSerialmsg = "";                 //  Clear serial buffer
  getSensorData();
  TxData.distanceIdx = 0;
  if (iNETPowerUp() == false) {       // powerup error, increase system restart counter
    SYScnt.iNetRst++;
    printDEBUG (F("[N] => Power-up modem..ERROR"));
    return;
  }
  printDEBUG (F("[N] => Power-up modem..OK"));
  SYScnt.iNetRst = 0;              // powerup ok, reset system restart counter
  
  // URL: deviceID,distance/batt/TempC/RH/hPaX100,RainMMx10,NodeStatus/Attempt/DNSerr/NoResp/Senterr/WifiErr/Hosterr
  sendURL = sysVar.DevID + "\r" + packLevel(iNetTx.distanceCM) + BYTE2HEX(iNetTx.BattLvl);
  if ((sysVar.NodeStatus&0x20) > 0) {          // BME280 presented
    sendURL += BYTE2HEX(iNetTx.TempC) + BYTE2HEX(iNetTx.RH) + uint16_t2HEX (iNetTx.hPAx100);
  } else {
    sendURL += "FFFFFFFF";
  }

  uint16_t sendRainCnt  = RainCount;                    // freeze RainCount value
  TxData.accRainCount  += sendRainCnt;                  // accumulated rain counter
  iNetTx.accRainCount   = TxData.accRainCount;          // 0x8000 is overflow flag
  TxData.accRainCount   = iNetTx.accRainCount & 0x7FFF; // clear overflow flag
  RainCount            -= sendRainCnt;
  
#if (SENSOR_RAIN == 1)
  sendURL += uint16_t2HEX (iNetTx.accRainCount);
#else
  sendURL += "FFFF";
#endif

  sendURL += "\r" + BYTE2HEX(sysVar.NodeStatus);
  sendURL += BYTE2HEX(iNetTx.iNETattempt);
  sendURL += BYTE2HEX(iNetTx.iNETDNSfail);
  sendURL += BYTE2HEX(iNetTx.iNETNoResp);
  sendURL += BYTE2HEX(iNetTx.iNETSentfail);
  sendURL += BYTE2HEX(iNetTx.iNETWififail);
  sendURL += BYTE2HEX(iNetTx.iNETHostfail);
  sendURL = String("6,1,") + sendURL;
  
  sysVar.NodeStatus &= 0xF3;                     // Clear Rapid Update/No Reponse flag (bit 2,3)
  printDEBUG ("[N] Upload data...");
  
  uint8_t ESPstate = 0, counters = 0;
  uint32_t prevtime = millis(), totalTime = 60000L;
  while (((millis() - prevtime) < totalTime) && (ESPstate < 10)) {    // allow 60s to send data
    result = monitoriNET ((ESPstate == 5)?250:1000);
    LEDflash();
    if (ESPstate != 5) {                          // wait for response, double flash
      delay (100); LEDflash();
      if (ESPstate == 4) {                        // DNS ok, wait for response, triple flash
        delay (100); LEDflash();
      }
    }
    if (ESPstate == 0) {                          // sending data state
      iNETSerial.println (sendURL);
      ESPstate = 1;                               // data sent completed
      counters = 0;
    } else if (ESPstate == 1) {                   // sending data state, wait for A,CMD RCV for 5 sec
      if (result == 15) ESPstate = 2;             // "A,CMD RCV"
      else counters++;                            // increase counter
      if (counters >= 5) {
        iNETPowerDown(2);                         // Power-down internet Module
        return;
      }
    } else if (ESPstate == 2) {                   // sending data state
      printDEBUG ("[N] Sending data...");
      iNETSerial.println ("7," + String((SYScnt.OTAallow > OTA_ATTEMPT_ALLOW)?0:SYScnt.OTAallow));
      ESPstate = 3;
    } else if (ESPstate == 3) {                   // data sent, wait for DNS response
      if (result == 17) ESPstate = 4;
      else if (inRange (result, 21, 28)) {        // Error result
        printDEBUG ("[N] Send Error");
        ESPstate = 12;                            // exit while {} with error
      }
    } else if (ESPstate == 4) {                   // data sent, wait for esp response
      if ((result == 16)||(result == 18)||(result == 19)) {     // sent ok with no OTA or OTA rejected
        if (SYScnt.OTAallow > OTA_ATTEMPT_ALLOW) SYScnt.OTAallow--;        // if OTAallow in grace period, decrease OTA counter
        ESPstate = 11;                            // exit while {} with success
        resetTxData ();
        sysVar.NodeStatus &= 0x7F;                // clear node reboot flag
        printDEBUG ("[N] Send OK");
      } else if (result == 31) {                  // sent ok with OTA in-progress
        ESPstate = 5;
        resetTxData ();
        sysVar.NodeStatus &= 0x7F;                // clear node reboot flag
        printDEBUG ("[N] Send OK-OTA");
        cntRemaining = 620;                       // ensure no watchdog reset during OTA
        totalTime    = 600000L;                   // change total wait time to 300s
      } else if (inRange (result, 21, 28)) {      // Error result
        ESPstate = 12;                            // exit while {} with error
        printDEBUG ("[N] Send Error");
      }
    } else if (ESPstate == 5) {                   // OTA in-progress
      if ((result == 33) && (SYScnt.OTAallow > 0)) {  // OTA failed
        SYScnt.OTAallow--;
        ESPstate = 11;
        if (SYScnt.OTAallow == 0) {               // too many OTA error, wait for i1 day to reset OTA allow counter
          SYScnt.OTAallow = iNET_ERR_RST;
          printDEBUG ("[N] OTA error. Wait 1 day.");
        } else printDEBUG ("[N] OTA error.");
      } else if ((result == 11) || (result == 32)) {    // OTA success
        SYScnt.OTAallow = OTA_ATTEMPT_ALLOW;
        ESPstate = 13;
        printDEBUG ("[N] OTA done.");
      }
    }
  }
  /* ESPstate
   *  11 - Send OK
   *  12 - Send Error
   *  13 - Send OK with OTA success
   */
  iNETPowerDown(5);                             // Power-down internet Module
  if (result == 19) {delay(500); resetFunc();}
}

bool iNETPowerUp (void) {                       // Power-up ESP and wait for connection
  uint8_t ESPstate = 0, timeCnt = 0, monResult = 0, hostErr = 0;
  uint32_t PwrUpTime = (uint32_t)ModemPwrUpTime * 1000.0;
/* ESPstate
 * 0  - init
 * 1  - ESP enabled.
 * 2  - Wait for ESP init messaage
 * 3  - Wait for WiFi connection
 * 4  - Enter Wifi Config mode
 * 11 - Wifi Connected
 * 21 - Wifi Config portal time-out
 * 22 - WiFi connect error
 */
  printDEBUG ("[N] => Power-up modem.");
  if (Device_Profile=="3G") digitalWrite(MODEM_ENpin, HIGH);
  ESPstate = 1;                             // Powering-up modem
  timeCnt = ModemWaitTime;                  // wait for a while before start ESP
  uint32_t starttime = millis();
  while (((millis() - starttime) < PwrUpTime) && (ESPstate < 10)) {   // loop for 50sec
    if (ESPstate == 4) {                    // if ESP in config mode, toggle LED
      LEDtoggle ();
    } else {
      LEDflash ();
    }
    monResult = monitoriNET (1000);
    if (monResult == 14) {
      ESPstate = 22; break;
    }
    if (ESPstate == 1) {                          // power-up modem state
      if (timeCnt == 0) {
        digitalWrite(ESP_ENpin, HIGH);
        timeCnt = 3;
        ESPstate = 2;                             // move to Powering-up ESP state
      }
    } else if (inRange(ESPstate,2,3)) {           // Powering-up ESP state
      if ((timeCnt == 0) && (ESPstate == 2)) {    // iNet monitor time-out
        iNETSerial.println ("0,status");          // request status
        timeCnt = 5;
      }
      if (monResult == 11) {                      // ESP init received
        String txt = sysVar.ssid+","+sysVar.pass+","+((Device_Profile=="WL")?((sysVar.WiFiConfTime>0)?"W1":"W0"):Device_Profile)+"," + sysVar.DevID+","+uint16_t2HEX(iNetTx.seqNr);
        iNETSerial.println ("2," + txt);
        printDEBUG ("[N] Config sent...");
        ESPstate = 3;
      } else if (monResult == 12) {                             // wifi connected received
        ESPstate = 11;                                          // exit while {}
      } else if (monResult == 41) {                             // ESP enter config mode
        ESPstate = 4;
      }
    } else if (ESPstate == 4) {                                 // ESP in config mode
      if (sysVar.WiFiConfTime > 0) {                            // if enter WiFi config for first use
        PwrUpTime = (uint32_t)sysVar.WiFiConfTime * 1000.0;     // increase modem PwrUp Time
        cntRemaining = sysVar.WiFiConfTime;                     // increase watchdog
      }
      if (monResult == 11) {                                    // ESP init received
        sysVar.WiFiConfTime = 0;
        PwrUpTime = (uint32_t)ModemPwrUpTime * 1000.0;
        starttime = millis();
        String txt = sysVar.ssid+","+sysVar.pass+","+((Device_Profile=="WL")?((sysVar.WiFiConfTime>0)?"W1":"W0"):Device_Profile)+"," + sysVar.DevID+","+uint16_t2HEX(iNetTx.seqNr);
        iNETSerial.println ("2," + txt);
        printDEBUG (F("[N] New Config sent..."));
        ESPstate = 3;
      } else if (monResult == 12) {               // Wifi connected
        ESPstate = 11;                            // exit while {}
      } else if (monResult == 42) {               // Config portal time-out
        ESPstate = 21;
      } else if (monResult == 44) {               // Config portal error
        ESPstate = 21;
      }
    }
    timeCnt--;
  }
  
  if (ESPstate == 2) {
    printDEBUG (F("[N] ESP init error."));
    iNetTx.iNETWififail++;
  } else if ((ESPstate == 3) || (ESPstate > 20)) {
    printDEBUG (F("[N] Wifi connect error."));
    iNetTx.iNETWififail++;
  } else if ((ESPstate == 4) || (ESPstate == 21)) {
    printDEBUG (F("[N] Wifi Config time-out."));
  } else if (ESPstate == 11) {             // modem power-on successfully, pause for CMDdelayTime sec
    sysVar.WiFiConfTime = 0;
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
    printDEBUG ("[N] Time-out @ESP state: " + String (ESPstate));
  }
  iNetTx.iNETSentfail++;
  iNETPowerDown(ESPstate);
  return false;
}

void iNETPowerDown (uint8_t ESPstate) {
  if (ESPstate > 1) iNETSerial.println("4,shutdown"); // send ESP shutdown command
  if (monitoriNET (1000) == 14) printDEBUG (F("[N] iNET normal Power-down."));
  else printDEBUG (F("[N] iNET forced Power-down."));
  delay (200);
  digitalWrite(ESP_ENpin, LOW);
  digitalWrite(MODEM_ENpin, LOW);
  LEDoff();                                           // turn-off LED
  //printDEBUG ("[N] Attempt: "+String (iNetTx.iNETattempt)+", NoResp: "+String(iNetTx.iNETNoResp)+", failed: "+String(iNetTx.iNETSentfail)+" [Wifi: "+String(iNetTx.iNETWififail)+", Host: "+String (iNetTx.iNETHostfail)+"]");
}

String packLevel (uint16_t distanceCM) {  // 0xaadd dddd dddd:distancetype 0..3,  distance:03FF (0..1023)
  uint16_t result = distanceCM & 0x3FF;
  result |= (SENSOR_TYPE << 10); 
  String txt = "000" + String (result, HEX);
  return txt.substring(txt.length() - 3);
}
