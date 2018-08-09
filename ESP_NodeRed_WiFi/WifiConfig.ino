const char ConfigPage1[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
<title>WiFi Config</title></head>
<body style='text-align: center;'><h2>WiFi Config</h2>)=====";

const char ConfigPage2[] PROGMEM=R"=====(<form action='/wifisave' method='get'>
<p>ssid: <input type='text' id='s' name='s' required/></p>
<p>pass: <input type='text' id='p' name='p'/></p>
<p><button type='submit'> SAVE </button></p>
</form>Press F5 to rescan.</body></html>)=====";

const char ConfigOKPage[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
<title>WiFi Config</title></head>
<body><h2 style='text-align: center;'>WiFi Config</h2>
<p>WiFi Config save sucessfully.</p>
</body></html>)=====";

void startConfigPortal (uint32_t portalTimeout) {     // portalTimeout (sec)
  portalTimeout = portalTimeout * 1000L;
  ESPstatus = 5;                                      // enter config mode
  
  WiFi.mode (WIFI_AP_STA);
  WiFi.softAP ("iotConfAP", "iot12345");              // start softAP with default IP:192.168.4.1
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

void sendConfigPage (void) {
  uint16_t c_len = strlen_P(ConfigPage1) + strlen_P(ConfigPage2);
  String ssidList = "";
  int nWiFi = WiFi.scanNetworks();            // scan wifi network
  if (nWiFi > 10) nWiFi = 10;
  for (uint8_t i=0; i<nWiFi;i++) {
    ssidList += "<a href='#' onclick=""document.getElementById('s').value='" + String (WiFi.SSID(i)) + "';"">" + String (WiFi.SSID(i)) + "</a><br>";
  }
  c_len += ssidList.length();
  server.setContentLength(c_len);
  server.send(200, "text/html", "");
  server.sendContent_P(ConfigPage1);         // sent content directly from PROGMEM
  server.sendContent(ssidList);
  server.sendContent_P(ConfigPage2);         // sent content directly from PROGMEM  
}

void sendSavePage (void) {
  for (uint8_t i = 0; i <= server.args(); i++) {        // loop through each argument
    if (server.argName(i) == "s") strcpy (ssid, server.arg(i).c_str());
    if (server.argName(i) == "p") strcpy (pass, server.arg(i).c_str());
  }
  server.setContentLength(strlen_P(ConfigOKPage));
  server.send(200, "text/html", "");
  server.sendContent_P(ConfigOKPage);
  server.stop();
  ESPstatus = 6;
}
