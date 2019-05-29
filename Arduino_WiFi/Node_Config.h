// =================================== Paramteres ================================
String Device_Profile   = "3G";         // 3G profile parameter
uint8_t ModemWaitTime   = 10;           // wait for modem to power-up
uint8_t ModemPwrUpTime  = 75;           // total modem power-up time 75s
uint8_t CMDdelayTime    = 5;            // delay time after wifi connected
uint16_t WakeUpInterval = 300;          // device wake-up every 5min
uint16_t Batt_Interval  = 2016;         // Batt re-calibration interval: 2016 x 300 = 604 800 sec = 7 days
uint16_t iNET_ERR_RST   = 288;          // Consecutive wifi error before node restart: 288 x 300 = 86 400 sec = 1 days
uint8_t TxiNET_LowBatt  = 72;           // Max:250, Send data to internet every 6hr (WakeUpInterval x TxiNET_Normal)
uint8_t TxiNET_Normal   = 6;            // Max:250, Send data to internet every 30min (WakeUpInterval x TxiNET_Normal

// Sensor pin: |GND|Rain| |SR04_TRIG|SR04_ECHO|GND|A1|5V|3V3|
// pin assignment
const uint8_t RainGauge_pin   = 2;
const uint8_t SR04_TRIGpin    = 3;
const uint8_t SR04_ECHOpin    = 4;
const uint8_t BattMeasurepin  = A2;
const uint8_t Profile_SEL_Pin = 6;            // Profile Select: 0-WiFi, 1-3G

const uint8_t ESP_RxPin       = 7;            // to ESP Tx
const uint8_t ESP_TxPin       = 8;            // to ESP Rx
const uint8_t ESP_ENpin       = 9;            // ESP enabled (CHPD pin)
const uint8_t MODEM_ENpin     = 10;           // Pocket WiFi enable pin

// ============================= Threshold =============================
const uint8_t  OTA_ATTEMPT_ALLOW  = 5;        // ESP8266 http OTA: 0-OTA not allow, 1..7-#of OTA failure allow
const uint8_t  BATTPowerSave      = 30;       // %Batt to operate in power save mode
const uint8_t  BATTPowerOff       = 10;       // %Batt to operate in power off
const uint16_t BATT_V000          = 3600;     // Batt mVolt as 0%Batt, 3.40v = Vcc 3.15v + 0.25v drop voltage
const uint16_t LvlCMChange        = 10;       // Update data asap if levelCM change > 10 cm/min

// ============================= Variable =============================
struct sysParaType {                          // system parameters
  String   DevID          = String(Device_GroupID) + String(Device_ID);
  String   ssid           = "";
  String   pass           = "";
  uint8_t  NodeStatus     = 0x80;             // Node Status: 7654 3210, 7:node reboot, 6:Rain Sensor-ready, 5:Weather sensor, 4:Low Batt, 3:Rapid Update, 2:Server-No response, 1/0: Send retry
  uint16_t WiFiConfTime   = 600;              // Wifi Config mode time-out, 5 min. Allow only after Arduino reset
  uint16_t maxAmVolt      = 4100;             // max mV of ADC value
} sysVar;

struct NodeDataType {
  uint16_t lastCM         = 0;                // lastast distanceCM
  uint8_t  distanceIdx    = 0;                // index of distanceMM[]
  uint16_t accRainCount   = 0;                // accumulated rain counter
  uint16_t distanceCM[5]  = { 0,0,0,0,0 };
} TxData;

struct iNetType {                   // data to send to Host
  uint8_t  BattLvl       = 0;       // battery level 0..100
  uint8_t  LvErrType     = 1;       // Error type 0-no error, 1-no data, 2-too few sample, 3-too high stdDV
  uint16_t distanceCM    = 0;
  uint8_t  TempC         = 127;     // TempC+40 (-40..85) -> 0..125
  uint8_t  RH            = 127;     // 0..100
  uint16_t hPAx100       = 0xFFFF;  // PA-50000 (500.00..1155.00 hPA) -> 0..65500
  uint16_t accRainCount  = 0;       // accumulated rain counter 0..0x7FFF + overflow flag (0x8000)
  uint16_t seqNr         = 0;       // iNET sequence number
  
  uint8_t LvSampleRead   = 0;       // #Read of Distance Sensor
  uint32_t iNETattempt   = 0;
  uint32_t iNETDNSfail   = 0;       // DNS resolve error
  uint32_t iNETNoResp    = 0;       // #No reponse from TCP/UDP server
  uint32_t iNETSentfail  = 0;       // #ESP power-up failed or UDP error response
  uint32_t iNETWififail  = 0;       // #Wifi connenect error
  uint32_t iNETHostfail  = 0;       // #TCP conect to Host error
} iNetTx;

#if (stDEBUG > 0)
  #define printDEBUG(x) Serial.println (x)
#else
  #define printDEBUG(x)
#endif
