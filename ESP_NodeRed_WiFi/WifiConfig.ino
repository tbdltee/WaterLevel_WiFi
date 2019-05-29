#define SOFT_AP_SSID    "iotConfAP"
#define SOFT_AP_PASS    "thaisolarway"        // Config SSID: 0xSSID:PASSx0

const char ConfigPage1[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/><title>WiFi Config</title></head>
<body style='text-align: center;'><h2>WiFi Config</h2><table align='center' width=250>)=====";

const char ConfigPage2[] PROGMEM=R"=====(<tr><td colspan=2 align='center'>
<form action='/wifisave' method='get'><p>ssid: <input type='text' id='s' name='s' required/></p>
<p>pass: <input type='text' id='p' name='p'/></p>
<p><button type='submit'> SAVE </button></p></form></td></tr>
<tr><td colspan=2>Press F5 to rescan.</td></tr></table></body></html>)=====";

const char ConfigOKPage[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
<title>WiFi Config</title></head><body><h2 style='text-align: center;'>WiFi Config</h2>
<p>WiFi Config save sucessfully. System Restart.</p></body></html>)=====";

const char WiFiConfigRedirectPage[] PROGMEM=R"=====(<!DOCTYPE html><html lang=en><head><meta http-equiv='refresh' content='1;url=/'><title>WiFi Config</title></head>
<body>Page not found. Redirecting...</body></html>)=====";

// ======================== Fixed IP Address =============================
IPAddress ip (192,168,8,9);
IPAddress subnet (255,255,255,0);
IPAddress gateway (192,168,8,1);
IPAddress dns (8,8,8,8);

// ======================================= WiFi Confog Code start =====================================
void Connect2WiFi (uint8_t ConntectionOption) {         // ConntectionOption 0-fixed IP, 1-DHCP assign, 2-myWiFiConfig
  if (String(ssid).length() == 0) return;
  WiFi.forceSleepWake(); delay (1);
  WiFi.persistent (false);                              // Disable WiFi persistence. ESP8266 will not save WiFi settings in flash memory.
  WiFi.mode (WIFI_STA);
  
  if (ConntectionOption == 0) WiFi.config(ip, dns, gateway, subnet);
  WiFi.begin (ssid, pass);
  
  prevtime = millis();
  while ((millis() - prevtime) < 15000L) {              // 1st connnection attempt 15 sec
    if (WiFi.status() == WL_CONNECTED) {
      ESPstatus = 2; return;
    } else if ((WiFi.status() == WL_NO_SSID_AVAIL) || (WiFi.status() == WL_CONNECT_FAILED)) {
      break;
    }
    monitorSerial();
  }
  WiFi.disconnect();                                    // 1st attempt failed
  Serial.println ("C,Wifi re-connecting..");
  delay(500);
  WiFi.begin (ssid, pass);
  prevtime = millis();
  while ((millis() - prevtime) < 25000L) {              // 2nd connnection attempt 25 sec
    if (WiFi.status() == WL_CONNECTED) {
      ESPstatus = 2; return;
    } else if ((WiFi.status() == WL_NO_SSID_AVAIL) || (WiFi.status() == WL_CONNECT_FAILED)) {
      break;
    }
    monitorSerial();
  }
}

bool tryConfigSSID (void) {
  String ssidList = "";
  Serial.print ("C,Searching WiFi Config..");
  WiFi.scanDelete();
  int nWiFi = WiFi.scanNetworks();

  if (nWiFi > 0) {
    for (uint8_t i = 0; i < nWiFi; i++) {
      ssidList = WiFi.SSID(i);
      if (ssidList.startsWith("x0") && ssidList.endsWith("x0") && (countChar(ssidList, ':') == 1)) {
        Serial.println ("found.");
        ssidList = getValue(WiFi.SSID(i), ':', 0);
        ssidList.remove (0, 2);
        strcpy (ssid, ssidList.c_str());
        ssidList = getValue(WiFi.SSID(i), ':', 1);
        ssidList.remove (ssidList.length() - 2, 2);
        strcpy (pass, ssidList.c_str());
        return true;
      }
    }
  }
  Serial.println ("not found.");
  return false;
}

void startConfigPortal (uint32_t portalTimeout) {       // portalTimeout (sec)
  if (tryConfigSSID ()) {                               // load wifi config success, restart
    SaveWiFiConfig ();
    Serial.println ("C,Config ok," + String (ssid) + ","+String(pass));
    WiFi.disconnect ();
    delay (500);
    ESP.restart();
  }
  
  IPAddress APip (192,168,4,1);
  IPAddress APgw (192,168,4,1);
  IPAddress APsubnet (255,255,255,0);
  uint32_t prevtime = millis();
  uint8_t currClient = 0;
  
  if (portalTimeout < 15) portalTimeout = 15;
  portalTimeout = portalTimeout * 1000L;
  ESPstatus = 5;                                          // enter config mode
  
  WiFi.mode (WIFI_AP);
  while ((millis() - prevtime) < 15000) {                 // start AP within 15 sec
    if (currClient == 0) {
      if (WiFi.softAPConfig(APip, APgw, APsubnet)) currClient = 1;
    } else if (currClient == 1) {
      if (WiFi.softAP (SOFT_AP_SSID, SOFT_AP_PASS)) {     // start softAP with default IP:192.168.4.1
        currClient = 2;
        break;
      }
    }
    delay (500);
  }
  if (currClient < 2) {
    Serial.println ("C,Config error, AP start error.");
    WiFi.disconnect ();
    delay (500);
    ESP.restart();
  }
  Serial.println ("C,Config mode, softAP ip: " + WiFi.softAPIP().toString());
  server.on("/", sendConfigPage);
  server.on("/wifisave", sendSavePage);
  server.onNotFound(sendErrorPage);                   // send redirect page to 192.168.4.1
  server.begin();
  Serial.println ("C,Config mode, portal ready");
  
  currClient = 0;
  while (((millis() - prevtime) < portalTimeout) && (ESPstatus == 5)) {
    server.handleClient();
    if (currClient != WiFi.softAPgetStationNum()) {
      currClient = WiFi.softAPgetStationNum();
      Serial.println ("C,Config mode, client connected: " + String(currClient));
    }
  }
  if (ESPstatus == 6) {
    SaveWiFiConfig ();
    Serial.println ("C,Config ok," + String (ssid) + ","+String(pass));
  } else {
    Serial.println ("C,Config time-out -> Restart");
  }
  server.close();
  delay (500);                                // ensure all page was sent
  ESP.restart();
}

void sendConfigPage (void) {
  uint16_t c_len = strlen_P(ConfigPage1) + strlen_P(ConfigPage2);
  String ssidList = "";
  WiFi.scanDelete();
  int nWiFi = WiFi.scanNetworks();            // scan wifi network
  int32_t rssi = 0;

  if (nWiFi > 0) {
    int lastIdx = -1, i, j;
    uint8_t usedIdx[nWiFi];
    
    for (i = 0; i < nWiFi; i++) {
      rssi = WiFi.RSSI(i);
      if (rssi > -85) {                                       // signal strangth in range
        for (j = 0; j <= lastIdx; j++) {                      // check exisitng ssid
          if (WiFi.SSID(usedIdx[j]) == WiFi.SSID(i)) break;   // existing ssid found
        }
        if (j > lastIdx) {                                    // no duplicated ssid
          lastIdx++;
          usedIdx[lastIdx] = i;
        } else {
          if (rssi > WiFi.RSSI(usedIdx[j])) usedIdx[j] = i;
        }
      }
    }
    for (i = 0; i <= (lastIdx - 1); i++) {                    // sort ssid based on rssi
      for (j = i+1; j <= lastIdx; j++) {
        if (WiFi.RSSI(usedIdx[j]) > WiFi.RSSI(usedIdx[i])) {
          nWiFi = usedIdx[i];
          usedIdx[i] = usedIdx[j];
          usedIdx[j] = nWiFi;
        }
      }
    }
    for (i = 0; i <= lastIdx; i++) {
      ssidList += "<tr align='left'><td width='80%'><a href='#' onclick=""document.getElementById('s').value='" + String (WiFi.SSID(usedIdx[i])) + "';"">" + String (WiFi.SSID(usedIdx[i])) + "</a></td>";
      ssidList += "<td style='color:"+ rssiColor (WiFi.RSSI(usedIdx[i])) + ";'>" + rssiText (WiFi.RSSI(usedIdx[i])) + "</td></tr>";
    }
  }
  if (ssidList == "") ssidList = "<tr><td colspan=2>No Wifi Network available.</td></tr>";
  
  c_len += ssidList.length();
  server.setContentLength(c_len);
  server.send(200, "text/html", "");
  server.sendContent_P(ConfigPage1);         // sent content directly from PROGMEM
  server.sendContent(ssidList);
  server.sendContent_P(ConfigPage2);         // sent content directly from PROGMEM  
}

String rssiText (int32_t rssi) {
  String t = "&#8718;";
  if (rssi <= -85) return t;
  if (rssi <= -75) return (t+t);
  if (rssi <= -65) return (t+t+t);
  if (rssi <= -55) return (t+t+t+t);
  return (t+t+t+t+t);
}

String rssiColor (int32_t rssi) {
  if (rssi > -65) return "green";
  if (rssi > -75) return "orange";
  return "red";
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
}

void sendErrorPage (void) {
  server.setContentLength(strlen_P(WiFiConfigRedirectPage));
  server.send(200, "text/html", "");
  server.sendContent_P(WiFiConfigRedirectPage);
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
