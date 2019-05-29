#include <Arduino.h>
#include "Node_Config.h"
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>

uint8_t ESPstatus = 0;
uint8_t strCMDidx = 0;
uint8_t OTAallowCnt = 0;
String CMDSerial = "", strCMD = "", devID = "", sendTxt = "", SEQNr = "";
uint32_t prevtime = 0;

char ssid[32] = "test";
char pass[32] = "test";

ESP8266WebServer server(80);              // local web server for configuration portal
IPAddress Host_addr(255,255,255,255);

// ======================== Program Setup Part ===========================
void setup() {
  WiFi.mode (WIFI_OFF);
  WiFi.forceSleepBegin();
  delay (1);                                              // turn-off RF at Power-up
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);                        // turn-off LED
  
  Serial.begin (19200);                                   // Init COMM to communicate with Arduino
  delay (100);
  Serial.println ("\r\n\r\nC,ESP init");
  prevtime = millis();
  while (((millis() - prevtime) < 10000L) && (ESPstatus == 0)) {
    monitorSerial();                                        // wait for command from Arduino
  }
  if (ESPstatus == 0) {                                     // time-out
    ESP.restart();
  } else if (ESPstatus == 1) {                              // coonect with 3G profile
    Serial.println ("C,connecting 3G with " + String(ssid));
    Connect2WiFi (0);
  } else if (ESPstatus == 6) {                              // connect using WiFi profile
    Serial.println ("C,connecting wifi.." + String(ssid));
    Connect2WiFi (1);
  } else if (ESPstatus == 7) {                              // connect using WiFi Manager
    ReadWiFiConfig ();                                      // use local ssid/pass
    Serial.println ("C,connecting wifi with wifiConfig.." + String(ssid));
    Connect2WiFi (2);
    if (ESPstatus == 2) Serial.println ("C,Config ok," + String (ssid) + ","+String(pass));
  }
  
  if (ESPstatus == 2) {                                     // wifi connected
    Serial.println ("C,wifi ready,IP:" + WiFi.localIP().toString());
  } else if (ESPstatus == 7) {                              // wifi.begin failed and in wifi config mode
    Serial.println ("C,Enter wifi config mode");
    ESPstatus = 5;
    startConfigPortal (600);                                // startConfigPortal result in ESP rertart.
  } else {
    Serial.println ("C,Wifi connect error..TIME-OUT");
    Serial.println ("C,ESP ready to shut-down");
    WiFi.disconnect ();
    delay (80);
    ESP.deepSleep (10000000L, WAKE_RF_DISABLED);
    WiFi.forceSleepBegin();
    delay (30000);
    ESP.restart();
  }
  ReadHostAddr ();                                          // read Host address from EEPROM
}
//================= end of setup, ESPstate-2 ==============================

void loop() {
  uint8_t HostlookupFail = 0;
  if (ESPstatus == 2) {                               // wifi connected
    while (ESPstatus != 4) monitorSerial();           // wait for completed RCV Data
    ESPstatus = 10;                                   // Move Connecting to Hostname resolving State
    prevtime = millis() - 12000;
  } else if (ESPstatus == 10) {
    ResolveHostName ();
  } else if (ESPstatus == 11) {                       // Host Address presented
    sendData2iNet ();
    ESPstatus = 2;                                    // move to Wait for CMD
  }
  monitorSerial();
}
