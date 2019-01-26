void Init_Peripheral() {
  pinMode(SR04_TRIGpin, OUTPUT);
  digitalWrite(SR04_TRIGpin, LOW);
  pinMode(SR04_ECHOpin, INPUT);
  
  pinMode(Sensor_Dpin, INPUT);
  pinMode(BattMeasurepin, INPUT);
  
  BME280_Init(myBME280);
  if (myBME280.ready > 0) {
    sysVar.NodeStatus |= 0x20;
    printDEBUG ("[S] Temp/RH: BME280..Init OK");
  } else {
    printDEBUG ("[S] Temp/RH: BME280 not found");
  }

  RainCount = 0;
#if (SENSOR_RAIN == 1)
  printDEBUG ("[S] Rain Gauge..Init OK.");
  sysVar.NodeStatus |= 0x40;
  pinMode(RainGauge_pin, INPUT_PULLUP);                 // Set INT0 back to input-pullup
  attachInterrupt(digitalPinToInterrupt(RainGauge_pin), RainGaugeCount, FALLING);
#else
  printDEBUG ("[S] Rain Gauge..not found.");
#endif
  printDEBUG ("[S] WakeUpInterval: " + String(WakeUpInterval) + "\r\n");
}

uint8_t getBatt(void) {
  uint16_t Avalue   = 0;                                                  // ADC 0..1023, read 64 loop => ADC value = 0..65472
  uint16_t AmVolt   = 0;
  uint16_t A11Value = readADC11();                                        // read ADC @1.1v reference  
  uint16_t mVcc = map(1023, 0, A11Value, 0, 1100);                        // calculate Vcc

  if (sysVar.maxAmVolt < 4100) sysVar.maxAmVolt = 4100;                   // init maxAmVolt to 4.10v as 100%
  for (uint8_t i = 0; i < 64; i++) Avalue += analogRead(BattMeasurepin);
  AmVolt = map (Avalue, 0, 65472, 0, mVcc) * 2;                           // Calculate A2 volt. x2 due to R-divider

  if (BattreCalcnt >= Batt_Interval) {                                    // re-calibration counter expire,
    BattreCalcnt = 0;
    sysVar.maxAmVolt -= 50;                                               // reduce maxAmVolt 50mV
  }
  printDEBUG ("[B] Batt_cnt: "+ String(BattreCalcnt)+" ("+ String(Batt_Interval)+"), Vcc: "+ String(mVcc)+", A-mV: "+ String(AmVolt)+", Amax-mV: "+ String(sysVar.maxAmVolt));
  if (AmVolt > sysVar.maxAmVolt) sysVar.maxAmVolt = AmVolt;
  if ((AmVolt <= BATT_V000) || (mVcc < 3050)) {
    return 0;  
  } else {
    return (uint8_t)map(AmVolt, BATT_V000, sysVar.maxAmVolt, 0, 100);
  }
}

uint16_t readADC11() {          // set ADC reference to Vcc and read ADC of internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2);                         // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);              // Start conversion
  while (bit_is_set(ADCSRA,ADSC));  // measuring

  uint8_t low  = ADCL;              // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH;              // unlocks both
  uint16_t result = (high<<8) | low;
  return result;
}

void getAvgDistance (void) {
  uint8_t i, countDistance1 = 0;
  uint16_t sumDistance = 0, minDistance, maxDistance;
  
  iNetTx.LvSampleRead = TxData.distanceIdx;
  for (i = 0; i < TxData.distanceIdx; i++) {
    if (TxData.distanceCM[i] > 0) {
      sumDistance += TxData.distanceCM[i];
      countDistance1++;
    }
  }
  iNetTx.distanceCM = 0;
  if (countDistance1 == 0) {
    iNetTx.LvErrType = 1;                       // cannot read distance
    return;
  }
  iNetTx.distanceCM = (uint16_t)((float)(sumDistance)/(float)(countDistance1));  // get averge distance in cm
  if (countDistance1 <= 4) {
    iNetTx.LvErrType = 2;                     // too low #sample
    return;
  }
  minDistance = (uint16_t)((float)iNetTx.distanceCM * 0.95);
  maxDistance = (uint16_t)((float)iNetTx.distanceCM * 1.05);
  uint8_t countDistance2 = 0;
  sumDistance = 0;
  for (i = 0; i < TxData.distanceIdx; i++) {
    if (inRange(TxData.distanceCM[i], minDistance, maxDistance)) {
      sumDistance += TxData.distanceCM[i];
      countDistance2++;
    }
  }
  iNetTx.distanceCM = (uint16_t)((float)(sumDistance)/(float)(countDistance2));  // get averge distance in mm
  float pctValid = (float)(countDistance2)/(float)(countDistance1);                 // get % valid data
  if (pctValid < 0.6) {                                                           // too high standard deviation
    iNetTx.LvErrType = 3;
  } else {
    iNetTx.LvErrType = 0;
  }
}

void measureDistanceCM(void) {                                // main function to get distance from short range and long range sensor
  TxData.lastCM = getDistanceSR04 ();                         // get distance from HC-SR04
  TxData.distanceCM[TxData.distanceIdx] = TxData.lastCM; 
  TxData.distanceIdx++;
}

uint16_t getDistanceSR04() {                          // Get distance (mm), single ping from HC-SR04+
  uint16_t result = 0;
  uint32_t pulse_width;
  digitalWrite(SR04_TRIGpin, LOW);
  delayMicroseconds(2);
  digitalWrite(SR04_TRIGpin, HIGH);                   // Hold the trigger pin high for at least 10 us
  delayMicroseconds(10);
  digitalWrite(SR04_TRIGpin, LOW);
  delayMicroseconds(120);                             // delay at least 2cm (120 us pulse)
  pulse_width = pulseIn(SR04_ECHOpin, HIGH, 23280);   // Returns the pulse width in uS or 0 if over 400 cm (23200 us pulse)
  result = (uint16_t)(((float)pulse_width)/58.2);     // return distance, cm
  if (inRange(result, 2, 400)) return result;         // valid distance is 3-400cm 
  return 0;
}

// ======================================== BME80 Sensor ================================
void BME280_Init(BME280 &BME280Sensor) {        // Init_BME280 in Force Mode
  BME280Sensor.I2CAddress = 0x76;               // I2C Addr 0x77(default) or 0x76 and ignore SPI
  BME280Sensor.runMode = 1;                     // 0=Sleep, 1,2=Force mode, 3=Normal Mode
  BME280Sensor.tStandby = 0;                    // tStandby[0-7] (Normal Mode Only): 0.5ms, 62.5ms, 125ms, 500ms 1000ms, 10ms, 20ms
  BME280Sensor.filter = 0;                      // Filter coefficients[0-4]: off, 2, 4, 8, 16
  BME280Sensor.tempOverSample = 1;              // tempOverSample[0-5]: skipped, *1, *2, *4, *8, *16
  BME280Sensor.pressOverSample = 1;             // pressOverSample[0-5]: skipped, *1, *2, *4, *8, *16
  BME280Sensor.humidOverSample = 1;             // humidOverSample[0-5]: skipped, *1, *2, *4, *8, *16
  BME280Sensor.begin();
}

void readBME280(BME280 &BME280Sensor, float &TempC, float &Pa, float &RH) {   //Asking sensor to measure data
  if (BME280Sensor.ready > 0) return BME280Sensor.getData (TempC, Pa, RH);
}
