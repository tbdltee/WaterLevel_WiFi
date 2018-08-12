#define OTA_ATTEMPT_ALLOW   5               // ESP8266 http OTA: 0-disable, >0-#of OTA failure allow
uint16_t AnalogValue      = 632;            // AnalogRead from ref. voltage
uint16_t ActualVolt       = 408;            // Actual voltage at Battery from multi-meter x 100, e.g. 3.96 -> 396

String Device_Profile   = "3G";         // 3G profile parameter
uint8_t ModemWaitTime   = 10;           // wait for modem to power-up
uint8_t ModemPwrUpTime  = 75;           // total modem power-up time 75s
uint8_t CMDdelayTime    = 5;            // delay time after wifi connected
uint16_t WakeUpInterval = 300;          // device wake-up every 5min
uint8_t TxiNET_LowBatt  = 120;          // Send data to internet every 10hr (WakeUpInterval x TxiNET_Normal)
uint8_t TxiNET_Normal   = 6;            // Send data to internet every 30min (WakeUpInterval x TxiNET_Normal
uint32_t MAX10dACnt     = 864000L/(uint32_t)WakeUpInterval;     // 10d = 864000sec

// 3G-WiFi Modem info:
// ssid/pass: IOT-0001D001-3G, DHCP-Off
// url:192.168.8.1, user: admin, pass:admin

// Sensor pin: |GND|Rain| |SR04_TRIG|SR04_ECHO|GND|A1|5V|3V3|
// pin assignment
const uint8_t RainGauge_pin   = 2;
const uint8_t SR04_TRIGpin    = 3;
const uint8_t SR04_ECHOpin    = 4;
const uint8_t Sensor_Dpin     = A1;
const uint8_t Sensor_ENpin    = 11;
const uint8_t BattMeasurepin  = A2;
const uint8_t Profile_SEL_Pin = 6;            // Profile Select: 0-3G, 1-WiFi

const uint8_t ESP_RxPin       = 7;            // to ESP Tx
const uint8_t ESP_TxPin       = 8;            // to ESP Rx
const uint8_t ESP_ENpin       = 9;            // ESP enabled (CHPD pin)
const uint8_t MODEM_ENpin     = 10;           // 3G/4G enable pin

const float RainMMperTip      = (14.0/55.0);  // rain volume per tip, ml. x 10/sq.cm. of Rain measurement bucket

// ============================= Threshold =============================
const uint8_t BATTPowerSave   = 15;           // %Batt to operate in power save: TxInterval = 4 min
const uint8_t BATTPowerOff    = 5;            // %Batt to operate in power off
const uint16_t MIN_VOLT       = 360;          // Minimum Volt shown as 0%
const uint16_t LvlCMChange    = 10;           // Update data asap if levelCM change > 10 cm/min
const uint32_t INT0_DEBOUNCE  = 50;           // INT0 debounce time, ms

struct sysParaType {                          // system parameters
  String DevID            = String(Device_GroupID) + String(Device_ID);
  String  ssid            = "";
  String  pass            = "";
  uint8_t NodeStatus      = 0x80;             // Node Status: 7654 3210, 7:node reboot, 6:Rain Sensor, 5:Weather sensor, 4:reserve, 3:Rapid Update, 2:Server-No response, 1/0: Send retry
  uint32_t AvalueCnt      = 0;                // Avalue counter, 1 cnt every 1665ms
  uint16_t maxA10d        = 0;                // max Avalue during 10days period
  uint16_t WiFiConfTime   = 600;              // Wifi Config mode time-out, 5 min. Allow only after Arduino reset
} sysPara;

struct NodeDataType {
  uint16_t lastCM         = 0;                // lastast distanceCM
  uint8_t distanceIdx     = 0;                // index of distanceMM[]
  uint16_t RainCount      = 0;                // Rain Count
  uint16_t distanceCM[10] = { 0,0,0,0,0,0,0,0,0,0 };
} TxData;

struct iNetType {                 // data to send to Host
  uint8_t BattLvl       = 0;      // battery level 0..100
  uint8_t LvErrType     = 1;      // Error type 0-no error, 1-no data, 2-too few sample, 3-toohigh stdDV
  uint16_t distanceCM   = 0;
  uint8_t TempC         = 127;
  uint8_t RH            = 127;
  uint16_t RainCount    = 0;
  uint16_t seqNr        = 0;      // iNET sequence number
  
  uint8_t LvSampleRead   = 0;     // #Read of Distance Sensor
  uint32_t iNETattempt   = 0;
  uint32_t iNETDNSfail   = 0;
  uint32_t iNETNoResp    = 0;
  uint32_t iNETSentfail  = 0;
  uint32_t iNETWififail  = 0;
  uint32_t iNETHostfail  = 0;
} iNetTx;

#if (stDEBUG > 0)
  #define printDEBUG(x) Serial.println (x)
#else
  #define printDEBUG(x)
#endif

