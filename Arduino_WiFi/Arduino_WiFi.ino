#define stDEBUG             0               // Show Debug message
#define SENSOR_RAIN         1               // Rain Gauge Sensor: 0-not present, 1-present

#include "Node_Config.h"
#include <avr/wdt.h>          // library for default watchdog functions
#include <avr/interrupt.h>    // library for interrupts handling
#include <avr/sleep.h>        // library for sleep
#include <avr/power.h>        // library for power control
#include <NeoSWSerial.h>
#include <I2C.h>
#include <BME280_I2C.h>

#define inRange(x, a, b) (((a) <= (x)) && ((x) <= (b)))     // define macro if (x>=a) && (x<=b)
#define outRange(x, a, b) (((x) < (a)) || ((x) > (b)))      // define macro if (x<a) || (x>b)
#define BYTE2HEX(x) ((((x) > 0x0F)?"":"0") + String((x), HEX))
#define LEDon() digitalWrite(LED_BUILTIN, HIGH)
#define LEDoff() digitalWrite(LED_BUILTIN, LOW)
#define LEDflash() ({digitalWrite(LED_BUILTIN, HIGH); delay(10); digitalWrite(LED_BUILTIN, LOW);})

// Timers & Counters
volatile uint16_t cntRemaining;               // how many times remain before wakeup and tx value, Volatile variable, to be able to modify in interrupt function
volatile uint16_t RainCount = 0;              // rain sensor counter
volatile uint32_t INT0_DebounceTimer = 0;     // INT0 Debounce Timer

uint8_t OTAallowCnt   = OTA_ATTEMPT_ALLOW;    // #OTA update failure before stop OTA. reset only when reboot Arduino
uint8_t TxiNETcnt     = 0;                    // Counter to send data to internet.
uint8_t Rapidcnt      = 128;                  // Counter of rapid update
uint8_t nonRapidcnt   = 0;                    // Counter of consecutive non-rapid count to reset Rapidcnt counter
uint32_t wakeup_time  = 0;
String iNETSerialmsg  = "";

NeoSWSerial iNETSerial(ESP_RxPin, ESP_TxPin); // RX, TX
BME280 myBME280;

void(* resetFunc) (void) = 0;                 //declare reset function @address 0

// ============================================================================================
void setup() {
  wdt_disable();
  delay (500);                                // startup delay to avoid power surge
# if (stDEBUG > 0)
  Serial.begin(57600);
#endif

  pinMode (Profile_SEL_Pin,  INPUT_PULLUP);
  pinMode (ESP_ENpin, OUTPUT);
  digitalWrite(ESP_ENpin, LOW);               // disable ESP8266
  pinMode (MODEM_ENpin, OUTPUT);
  digitalWrite(MODEM_ENpin, LOW);             // disable 3G/4G modem
  pinMode (Sensor_ENpin, OUTPUT);
  digitalWrite(Sensor_ENpin, LOW);
  pinMode (LED_BUILTIN, OUTPUT);
  LEDoff();

  if (digitalRead (Profile_SEL_Pin) == LOW) { // change from 3G to WiFi
    Device_Profile  = "WL";     // use WiFi Profile
    ModemWaitTime   = 0;        // wait for modem to power-up
    ModemPwrUpTime  = 50;       // total modem power-up time 50s
    CMDdelayTime    = 0;        // delay time after wifi connected
    WakeUpInterval  = 120;      // device wake-up every 120s
    TxiNET_LowBatt  = 60;       // Send data to internet every 120min (WakeUpInterval x TxiNET_Normal)
    TxiNET_Normal   = 5;        // Send data to internet every 10min (WakeUpInterval x TxiNET_Normal)
    T30dayCnt       = 2592000L/(uint32_t)WakeUpInterval;     // 10d = 864000sec
    printDEBUG (F("[S] ====== SYSTEM INIT (WiFi Profile) ======"));
  } else {
    printDEBUG (F("[S] ====== SYSTEM INIT (3G Profile) ======"));
  }
  pinMode (Profile_SEL_Pin,  INPUT);          // change pinMode to remove Internal R pullup
  
  randomSeed(get_rand_byte());
  iNetTx.seqNr = random(65536);
  iNETSerial.begin(19200);
  delay (200);
  Init_Peripheral();
  Initialize_wdt();                           // configure with reset interval 1 sec and start the watchdog.
  resetTxData ();
  sysPara.ssid = sysPara.DevID + "-" + String (Device_Profile);
  sysPara.pass = sysPara.ssid;
}

void loop(void) {
  wakeup_time = millis();
  cntRemaining = WakeUpInterval * 1.2;
  
  uint16_t PrevdistanceCM = TxData.lastCM;                            // save current distance value before new distance measurement
  uint16_t LvlRapidCM = LvlCMChange * WakeUpInterval / 60;            // calc cm/min to cm threshold per measurement
  uint16_t lowerRapid =  (PrevdistanceCM > LvlRapidCM) ? PrevdistanceCM - LvlRapidCM: 0;
  
  iNetTx.BattLvl = getBatt();                                         // get bettery level
  if (iNetTx.BattLvl <= BATTPowerOff) {                               // if Batt_PowerOff, set TxiNETcnt = 254 (never update iNET)
    TxiNETcnt = 254;
  } else {                                                            // if Batt_level above Batt_Power_Off
    if (TxiNETcnt > 250) {
      if (iNetTx.BattLvl <= BATTPowerSave) {
        TxiNETcnt = TxiNET_LowBatt;
      } else {
        TxiNETcnt = TxiNET_Normal;
      }
    }
  }
  measureDistanceCM ();                                               // single measure distance, every wake-up (3 min)
  if (TxData.lastCM  > 0) {
    if (TxData.lastCM > (PrevdistanceCM + LvlRapidCM)) {              // distance above threshold
      Rapidcnt++; nonRapidcnt = 0;
    } else if (TxData.lastCM < lowerRapid) {                          // distance below threshold
      Rapidcnt--; nonRapidcnt = 0;
    } else {
      nonRapidcnt++;
      if (nonRapidcnt > 10) {
        Rapidcnt = 128; nonRapidcnt = 0;
      }
    }
    if (outRange (Rapidcnt, 125, 131)) {                              // if 3 consecutive rapid change, update inet/override BATTPowerOff
      TxiNETcnt = 0; Rapidcnt = 128;
      sysPara.NodeStatus |= 0x08;                                     // Set Rapid Update flag
    }
  }
  printDEBUG ("[D] rangeCM-"+String(TxData.distanceIdx)+":"+String(TxData.lastCM)+", Batt:"+String(iNetTx.BattLvl)+", RainCnt:"+String(RainCount)+", TxiNETcnt:"+String(TxiNETcnt)+", Rapidcnt:"+String(Rapidcnt)+", ACnt:"+String(sysPara.AvalueCnt));
  if (TxiNETcnt == 0) {                                 // Send data to internet
    updateData ();
    if (iNetTx.BattLvl <= BATTPowerSave) {
      TxiNETcnt = TxiNET_LowBatt;                       // if low batt, set TxInterval to TxiNET_LowBatt
    } else {
      TxiNETcnt = TxiNET_Normal;
    }
  }
  printDEBUG ("[S] Sleep...for " + String (calcSleepTime()) + " sec.");
  delay (100);                                          // ensure everything printed
  sleep();
  TxiNETcnt--;
}

void resetTxData (void) {
  iNetTx.LvErrType      = 1;
  iNetTx.distanceCM     = 0;
  iNetTx.TempC          = 127;
  iNetTx.RH             = 127;
  iNetTx.hPAx100        = 0xFFFF;
  iNetTx.LvSampleRead   = 0;
  iNetTx.iNETattempt    = 0;
  iNetTx.iNETDNSfail    = 0;
  iNetTx.iNETSentfail   = 0;
  iNetTx.iNETWififail   = 0;
  iNetTx.iNETHostfail   = 0;
  if ((sysPara.NodeStatus & 0x40) == 0) iNetTx.iNETNoResp = 0;
}

void getSensorData (void) {       // calculate average distance and get weather data
  getAvgDistance ();              // iNetTx.distanceCM and iNetTx.errType were updated
  float TempC = 127, RH = 127, Pa = 0;
  readBME280 (myBME280, TempC, Pa, RH);
  if (myBME280.ready < 1) {
    TempC = 127; RH = 127; Pa = 0;
  }
  if (isnan(TempC)) TempC = 127;
  if (isnan(RH)) RH = 127;
  if (isnan(Pa)) Pa = 0;
  iNetTx.TempC    = (uint8_t)((TempC == 127)?127:(TempC+40));
  iNetTx.RH       = (uint8_t)RH;
  iNetTx.hPAx100  = (uint16_t)(outRange(Pa, 50000, 115500)?0xFFFF:(Pa-50000.0));
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

uint16_t calcSleepTime (void) {                   // calculate sleep time (sec) to prevent wake-up time > WakeUpInterval, e.g OTA
  uint16_t runningTime = (uint16_t)((millis() - wakeup_time)/1000L);
  runningTime = runningTime % WakeUpInterval;     // calc running time of the last cycle to ensure that runningtime < WakeUpInterval
  return (WakeUpInterval - runningTime);
}

String uint16_t2HEX (uint16_t value) {
  String result = BYTE2HEX((uint8_t)((value & 0xFF00) >> 8));
  result += BYTE2HEX((uint8_t)(value & 0x00FF));
  return result;
}

// ========================= Generate true random number, use for RandomSeed() ============================
int GetTemp (void) {
  unsigned int wADC;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC
  delay(20);            // wait for voltages to become stable.
  ADCSRA |= _BV(ADSC);  // Start the ADC
  while (bit_is_set(ADCSRA,ADSC));
  wADC = ADCW;
  return (wADC);
}

int coin_flip(void) {
  int little_bit;
  return little_bit = (GetTemp() & 1);
}

int getFairBit(void) {
  while(1) {
    int a = coin_flip();
    if (a != coin_flip()) return a;
  }
}

int get_rand_byte(void){
  int n=0;
  int bits = 7;
  while (bits--) {
    n<<=1; 
    n |= getFairBit();
  }
  return n;
}
