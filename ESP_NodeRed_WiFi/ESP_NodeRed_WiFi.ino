#include <Arduino.h>
#include "Node_Config.h"
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>

uint8_t ESPstatus = 0;
uint8_t strCMDidx = 0;
uint8_t OTAFailcnt = 0;
String CMDSerial = "", strCMD = "", devID = "", sendTxt = "", SEQNr = "";
uint32_t prevtime = 0;

char ssid[32] = "test";
char pass[32] = "test";
IPAddress ip (192,168,8,9);
IPAddress subnet (255,255,255,0);
IPAddress gateway (192,168,8,1);
IPAddress dns (8,8,8,8);

ESP8266WebServer server(80);              // local web server for configuration portal
IPAddress Host_addr(255,255,255,255);

// ======================== Program Setup Part ===========================
void setup() {
  WiFi.mode (WIFI_OFF);
  WiFi.forceSleepBegin();
  delay (1);                                              // turn-off RF
  Serial.begin (19200);                                   // Init COMM to communicate with Arduino
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);                             // turn-off LED
  
  ReadHostAddr ();                                        // read Host address from EEPROM
  Serial.println ();
  Serial.println ();
  Serial.println ("C,ESP init");
  delay (100);
  prevtime = millis();
  while ((millis() - prevtime) < 10000L) {
    if (ESPstatus > 0) break;
    monitorSerial();                                        // wait for command from Arduino
  }
  if (ESPstatus == 0) {                                     // time-out
    ESP.restart();
  } else if (ESPstatus == 1) {                              // coonect with 3G profile
    WiFi.forceSleepWake(); delay (1);
    WiFi.persistent (false);                                // Disable WiFi persistence. ESP8266 will not save WiFi settings in flash memory.
    WiFi.mode (WIFI_STA);
    Serial.println ("C,connecting 3G with " + String(ssid));
    WiFi.config(ip, dns, gateway, subnet);
    WiFi.begin(ssid, pass);
  } else if (ESPstatus == 6) {                              // connect using WiFi profile
    WiFi.forceSleepWake(); delay (1);
    WiFi.persistent (false);
    WiFi.mode (WIFI_STA);
    Serial.println ("C,connecting wifi.." + String(ssid));
    WiFi.begin (ssid, pass);
  } else if (ESPstatus == 7) {                              // connect using WiFi Manager
    ReadWiFiConfig ();
    WiFi.forceSleepWake(); delay (1);
    WiFi.persistent (false);
    WiFi.mode (WIFI_STA);
    Serial.println ("C,connecting wifi with wifi-config.." + String(ssid));
    if (String(ssid).length() > 0) WiFi.begin (ssid, pass);
  }
  prevtime = millis();
  while ((millis() - prevtime) < 30000) {
    if (WiFi.status() == WL_CONNECTED) {
      if (ESPstatus == 7) Serial.println ("C,Config ok," + String (ssid) + ","+String(pass));
      ESPstatus = 2;
      break;
    }
    monitorSerial();
  }
  if (ESPstatus == 2) {                                       // wifi connected
    Serial.println ("C,wifi ready,"+String (WiFi.SSID())+",IP:" + WiFi.localIP().toString());
  } else if (ESPstatus == 7) {                                // wifi.begin failed and in wifi config mode
    Serial.println ("C,Enter wifi config mode");
    ESPstatus = 5;
    startConfigPortal (600);                                  // startConfigPortal result in ESP.restart();
  } else {
    WiFi.mode (WIFI_OFF);
    Serial.println ("C,Stand-by for wifi reconnect.");
    delay (5000);
    ESP.restart();
  }
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
  } else if (ESPstatus==11) {                         // Host Address presented
    sendData2iNet ();
    ESPstatus = 2;                                    // move to Wait for CMD
  }
  monitorSerial();
}
