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
      if (iNETmsg.startsWith ("C,ESP ready to shut-down"))  return 14;
      
      if (iNETmsg.startsWith ("A,CMD RCV"))                 return 15;
      
      if (iNETmsg.startsWith ("A,Sent OK-done")) {
        if (iNETmsg.startsWith ("A,Sent OK-done,OTA "))     return 18;  // send OK but OTA error
        else if (iNETmsg.startsWith ("A,Sent OK-done,resetADC4mA,")) {  // send OK and restart node
#if (SENSOR_TYPE == 1)
          ADC4mA = ADC4mAINIT;
#endif
        } else if (iNETmsg.startsWith ("A,Sent OK-done,restart,")) return 19;  // send OK and reset ADC4mA
        else if (iNETmsg.startsWith ("A,Sent OK-done,15")) {
          iNetTx.iNETNoResp++;
          sysVar.NodeStatus |= 0x04;                                    // set No response bit (bit was cleared when enter updateData();)
        }
        return 16;
      }
      if (iNETmsg.startsWith ("C,resolve ok"))              return 17;
      if (iNETmsg.startsWith ("C,resolve error")) {                     // resolve error, use default Host IP
        iNetTx.iNETDNSfail++; return 17;
      }
      
      if (iNETmsg.startsWith ("B,")) {
        if (iNETmsg.startsWith ("B,5,")) {                              // DNS lookup error, no host IP
          iNetTx.iNETDNSfail++;                             return 22;
        }
        if (iNETmsg.startsWith ("B,6,")) iNetTx.iNETHostfail++;         // connect to TCP host error
        if (iNETmsg.startsWith ("B,7,")) {                              // TCP/UDP data sent error
          iNetTx.iNETSentfail++;                            return 21;
        }
      }
      
      if (iNETmsg.startsWith ("A,Sent OK-OTA,"))            return 31;
      if (iNETmsg.startsWith ("C,OTA OK"))                  return 32;
      if (iNETmsg.startsWith ("C,OTA err"))                 return 33;

      if (iNETmsg.startsWith ("C,Enter wifi config mode"))  return 41;
      if (iNETmsg.startsWith ("C,Config time-out"))         return 42;
      if (iNETmsg.startsWith ("C,Config ok")) {
        sysVar.ssid = getValue (iNETmsg, ',', 2);
        sysVar.pass = getValue (iNETmsg, ',', 3);
        return 43;
      }
      if (iNETmsg.startsWith ("C,Config error"))            return 44;
      
      if (iNETmsg.startsWith ("A,status,")) {
        uint8_t result = (uint8_t)getValue (iNETmsg, ',', 2).toInt();
        if (result == 0) return 11;                         // ESP inited
        if (result == 2) return 12;                         // wifi connected
      }
    }
  }
  if ((iNETSerialmsg.indexOf("\n") == 0) && iNETSerialmsg.length() > 40) iNETSerialmsg = "";
  return 99;  // timeout
}
