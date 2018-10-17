const char ConfigPage1[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/><title>WiFi Config</title></head>
<body style='text-align: center;'><h2>WiFi Config</h2><table align='center' width=250><tbody>)=====";

const char ConfigPage2[] PROGMEM=R"=====(<tr><td colspan=3 align='center'>
<form action='/wifisave' method='get'><p>ssid: <input type='text' id='s' name='s' required/></p>
<p>pass: <input type='text' id='p' name='p'/></p>
<p><button type='submit'> SAVE </button></p></form></td></tr>
<tr><td colspan=3>Press F5 to rescan.</td></tr></tbody></table></body></html>)=====";

const char ConfigOKPage[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
<title>WiFi Config</title></head><body><h2 style='text-align: center;'>WiFi Config</h2>
<p>WiFi Config save sucessfully. System Restart.</p></body></html>)=====";

// ======================================= WiFi Confog Code start =====================================
void startConfigPortal (uint32_t portalTimeout) {     // portalTimeout (sec)
  portalTimeout = portalTimeout * 1000L;
  ESPstatus = 5;                                      // enter config mode
  
  WiFi.mode (WIFI_AP_STA);
  WiFi.softAP ("iotConfAP", "thaisolarway");          // start softAP with default IP:192.168.4.1
  server.on("/", sendConfigPage);
  server.on("/wifisave", sendSavePage);
  server.onNotFound([]() {server.send(200, "text/plain", F("Page not found."));});
  server.begin();

  uint32_t prevtime = millis();
  String oldssid = String(ssid), oldpass = String(pass);
  while (((millis() - prevtime) < portalTimeout) && (ESPstatus == 5)) {
    server.handleClient();
  }
  if (ESPstatus == 6) {
    SaveWiFiConfig ();
    Serial.println ("C,Config ok," + String (ssid) + ","+String(pass));
  } else {
    Serial.println ("C,Config time-out.");
  }
  delay (500);
  ESP.restart();
}

void sendConfigPage (void) {
  uint16_t c_len = strlen_P(ConfigPage1) + strlen_P(ConfigPage2);
  String ssidList = "";
  WiFi.scanDelete();
  int nWiFi = WiFi.scanNetworks();            // scan wifi network
  int32_t rssi = 0;
  
  if (nWiFi > 0) {
    for (uint8_t i=0; i<nWiFi;i++) {
      rssi = WiFi.RSSI(i);
      if (rssi > -85) {
        ssidList += "<tr align='left'><td width='80%'><a href='#' onclick=""document.getElementById('s').value='" + String (WiFi.SSID(i)) + "';"">" + String (WiFi.SSID(i)) + "</a></td>";
        ssidList += "<td width='5%'>" + String(rssi) + "</td><td>" + rssiText (rssi) + "</td></tr>";
      }
    }
  }
  if (ssidList == "") ssidList = "<tr><td colspan=3>No Wifi Network available.</td></tr>";
  
  c_len += ssidList.length();
  server.setContentLength(c_len);
  server.send(200, "text/html", "");
  server.sendContent_P(ConfigPage1);         // sent content directly from PROGMEM
  server.sendContent(ssidList);
  server.sendContent_P(ConfigPage2);         // sent content directly from PROGMEM  
}

String rssiText (int32_t rssi) {
  if (rssi > -55) return "V good";
  if (rssi > -65) return "Good";
  if (rssi > -75) return "Average";
  if (rssi > -85) return "Poor";
  return "V poor";
}

void sendSavePage (void) {
  server.setContentLength(strlen_P(ConfigOKPage));
  server.send(200, "text/html", "");
  server.sendContent_P(ConfigOKPage);
  for (uint8_t i = 0; i <= server.args(); i++) {        // loop through each argument
    if (server.argName(i) == "s") strcpy (ssid, server.arg(i).c_str());
    if (server.argName(i) == "p") strcpy (pass, server.arg(i).c_str());
  }
  ESPstatus = 6;
  delay (500);
}

void SaveWiFiConfig (void) {          // save wifi info on byte 16..79
  EEPROM.begin(80);
  for (uint8_t i = 0; i<32; i++) {
    EEPROM.write (i+16, ssid[i]);
    EEPROM.write (i+48, pass[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void ReadWiFiConfig (void) {          // read wifi info on byte 16..79
  EEPROM.begin(80);
  for (uint8_t i = 0; i<32; i++) {
    ssid[i] = EEPROM.read (i+16);
    pass[i] = EEPROM.read (i+48);
  }
  EEPROM.end();
}
