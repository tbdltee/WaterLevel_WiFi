#define stDEBUG             1               // Show Debug message. 0-no debug, 1-show debug, 2-debug using wifi profile
#define SENSOR_TYPE         0               // Distance Sensor:    0-Top-Down distanceCM (HC-R04), 1-Buttom-up 4-20mA, 2-reserved, 3-reserved
#define SENSOR_RAIN         1               // Rain Gauge Sensor:  0-not present, 1-present

// ================================= Node Info. ===========================================
// SW version 0040D 
const char* Device_GroupID  = "IOT-0001";   // Device_GroupID
const char* Device_ID       = "D001";       // Device ID

// =================================== Library Declaration ================================
#include "Node_Config.h"
#include <avr/wdt.h>          // library for default watchdog functions
#include <avr/interrupt.h>    // library for interrupts handling
#include <avr/sleep.h>        // library for sleep
#include <avr/power.h>        // library for power control
#include <NeoSWSerial.h>
#include <EEPROM.h>
#include <I2C.h>
#include <BME280_I2C.h>

#define inRange(x, a, b) (((a) <= (x)) && ((x) <= (b)))     // define macro if (x>=a) && (x<=b)
#define outRange(x, a, b) (((x) < (a)) || ((x) > (b)))      // define macro if (x<a) || (x>b)
#define BYTE2HEX(x) ((((x) > 0x0F)?"":"0") + String((x), HEX))
#define LEDon() digitalWrite(LED_BUILTIN, HIGH)
#define LEDoff() digitalWrite(LED_BUILTIN, LOW)
#define LEDflash() ({digitalWrite(LED_BUILTIN, HIGH); delay(10); digitalWrite(LED_BUILTIN, LOW);})
#define LEDtoggle() (digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)))

// Timers & Counters
volatile uint8_t  cnt1secWDT  = 0;            // Counter for 1 sec
volatile uint8_t  INT0_btnCnt = 0;            // INT0 Debounce Counter 10x15 = 150ms, (millis() not working in sleep mode)
volatile uint16_t cntRemaining;               // how many times remain before wakeup and tx value, Volatile variable, to be able to modify in interrupt function
volatile uint16_t RainCount   = 0;            // rain sensor counter

uint8_t TxiNETcnt     = 0;                    // Counter to send data to internet. 0-Send data, 1..250-Normal/Low Batt, 251..254-Pwr-Off recovery, 255-never send, 
uint8_t Rapidcnt      = 128;                  // Counter of rapid update
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
  pinMode (LED_BUILTIN, OUTPUT);
  LEDoff();

  if ((digitalRead (Profile_SEL_Pin) == LOW) || (stDEBUG == 2)) {               // change from 3G to WiFi
    Device_Profile  = "WL";                   // use WiFi Profile
    ModemWaitTime   = 0;                      // wait for modem to power-up
    ModemPwrUpTime  = 50;                     // total modem power-up time 50s
    CMDdelayTime    = 0;                      // delay time after wifi connected
    WakeUpInterval  = 120;                    // device wake-up every 120s
    Batt_Interval   = 5040;                   // Batt re-calibration interval: 5 040 x 120 = 604 800 sec = 7 days
    iNET_ERR_RST    = 720;                    // Consecutive wifi error before node restart: 720 x 120 = 86 400 sec = 1 days
    TxiNET_LowBatt  = 60;                     // Send data to internet every 120min (WakeUpInterval x TxiNET_Normal)
    TxiNET_Normal   = 5;                      // Send data to internet every 10min (WakeUpInterval x TxiNET_Normal)
    printDEBUG (F("[S] == SYSTEM INIT (WiFi Profile) =="));
  } else {
    printDEBUG (F("[S] == SYSTEM INIT (3G Profile) =="));
  }
  printDEBUG ("[S] DeviceID: " + sysVar.DevID);
  pinMode (Profile_SEL_Pin,  INPUT);          // change pinMode to remove Internal R pullup
  
  randomSeed(get_rand_byte());
  iNetTx.seqNr = random(65536);
  iNETSerial.begin(19200);
  delay (200);
  Init_Peripheral();
  Initialize_wdt();                           // configure with reset interval 1 sec and start the watchdog.
  resetTxData ();
  sysVar.ssid = sysVar.DevID + "-" + String (Device_Profile);
  sysVar.pass = sysVar.ssid;
#if (SENSOR_TYPE == 1)
  EEPROM.get (0x00, ADC4mA);
#endif
}

void loop(void) {
  wakeup_time = millis();
  cntRemaining = WakeUpInterval * 1.2;
  updateBattState ();
  uint16_t PrevdistanceCM = TxData.lastCM;                            // save current distance value before new distance measurement
  measureDistanceCM ();                                               // single measure distance, every wake-up (3 min)
  checkLevelRapidChange (PrevdistanceCM);
  //printDEBUG ("[D] rangeCM-"+String(TxData.distanceIdx)+":"+String(TxData.lastCM) + ", Batt:"+String(iNetTx.BattLvl)+", RainCnt:"+String(RainCount)+", Rapidcnt:"+String(Rapidcnt));
  
  if (TxiNETcnt == 0) {                                 // Send data to internet
    updateData ();
    if (iNetTx.BattLvl <= BATTPowerSave) {
      TxiNETcnt = TxiNET_LowBatt;                       // if low batt, set TxInterval to TxiNET_LowBatt
    } else {
      TxiNETcnt = TxiNET_Normal;
    }
  }
  if (SYScnt.iNetRst > iNET_ERR_RST) {
    //printDEBUG ("[S] No iNET 24hr...Restart");
    delay (500);
    resetFunc();
  }
  sleep();
  TxiNETcnt--;
}

void checkLevelRapidChange (uint16_t PrevdistanceCM) {
  uint16_t LvlRapidCM = LvlCMChange * WakeUpInterval / 60;            // calc cm/min to cm threshold per measurement
  uint16_t lowerRapid =  (PrevdistanceCM > LvlRapidCM) ? PrevdistanceCM - LvlRapidCM: 0;
  if (TxData.lastCM  > 0) {
    if (TxData.lastCM > (PrevdistanceCM + LvlRapidCM)) {              // distance above threshold -> Rapid change increase counter
      Rapidcnt++; SYScnt.nonRapid = 0;
    } else if (TxData.lastCM < lowerRapid) {                          // distance below threshold -> Rapid change decrease counter
      Rapidcnt--; SYScnt.nonRapid = 0;
    } else {                                                          // distance within threshold range -> increase non-rapid counter
      SYScnt.nonRapid++;
      if (SYScnt.nonRapid > 10) {                                     // if non-rapid counter > 10, reset rapid change counter
        Rapidcnt = 128; SYScnt.nonRapid = 0;
      }
    }
    if (outRange (Rapidcnt, 125, 131)) {                              // if 3 consecutive rapid change, update inet/override BATTPowerOff
      TxiNETcnt = 0; Rapidcnt = 128;
      sysVar.NodeStatus |= 0x08;                                      // Set Rapid Update flag
    }
  }
}

void updateBattState (void) {
  SYScnt.BattreCal++;
  iNetTx.BattLvl = getBatt();                                       // get %battery
  //printDEBUG ("[Batt] max-mV:" + String(sysVar.maxAmVolt) + ", BattCnt:"+ String(SYScnt.BattreCal) + " ,TxiNETcnt:"+ String(TxiNETcnt));
  LEDflash ();
  if (iNetTx.BattLvl <= BATTPowerOff) {                             // if Batt_PowerOff, set TxiNETcnt = 254 (never update iNET)
    TxiNETcnt = 255;
    //printDEBUG ("[D] Batt Power-Off..");
   return;
  }
  if (TxiNETcnt > 250) {                                            // if Batt in Power_off state, check wakeup condition?
    if (iNetTx.BattLvl > BATTPowerSave) {                           // if Batt Level above Power_Save, decrease counter
      //printDEBUG ("[D] Batt Recovery.." + String (TxiNETcnt - 251));
    } else {                                                        // Tx in Power-Save with below Power_Save threshold -> hold in receovery state
      TxiNETcnt = 254;
      //printDEBUG ("[D] Batt Reovery-hold..");
    }
    if (TxiNETcnt == 251) {                                         // Batt above Power_Save for 4 consecutive WakeupInterval, send data to iNET
      if (iNetTx.BattLvl <= BATTPowerSave) sysVar.NodeStatus |= 0x10;
      else sysVar.NodeStatus &= 0xEF;
      TxiNETcnt = 0;
    }
    return;
  }
  // not in recovery state
  if (iNetTx.BattLvl <= BATTPowerSave) {                            // if Batt Level above Power_Save, decrease counter
    if ((sysVar.NodeStatus & 0x10) == 0) {                          // Normal ==> Power Save
      sysVar.NodeStatus |= 0x10;                                    // Low Batt
      if (TxiNETcnt > 0) TxiNETcnt += (TxiNET_LowBatt - TxiNET_Normal);
      //printDEBUG ("[D] Batt Norm -> PwrSave...new TxiNETcnt:" + String (TxiNETcnt));
    }
  } else {                                                          // Normal state
    if (sysVar.NodeStatus & 0x10) {                                 // Power Save ==> Normal
      sysVar.NodeStatus &= 0xEF;                                    // Normal Batt
      TxiNETcnt = TxiNET_LowBatt - TxiNETcnt;
      if (TxiNETcnt >= TxiNET_Normal) TxiNETcnt = 0;
      else TxiNETcnt = TxiNET_Normal - TxiNETcnt;
      //printDEBUG ("[D] Batt PwrSave -> Norm...new TxiNETcnt:" + String (TxiNETcnt));
    }
  }
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
  if ((sysVar.NodeStatus & 0x04) == 0) iNetTx.iNETNoResp = 0;
}

void getSensorData (void) {       // calculate average distance and get weather data
  
  return;
  
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
  if ((TempC == 127)||(RH == 127)||(Pa == 0)) SYScnt.BME280err++;
  else SYScnt.BME280err = 0;
  if (SYScnt.BME280err > BME280err_ALLOW) {
    myBME280.reset();
    BME280_Init (myBME280);
    SYScnt.BME280err = BME280err_ALLOW - 1;
  }
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
