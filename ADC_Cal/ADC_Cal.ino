#define Level_pin       A1
#define Level_EN_pin    11
#define NET_EN_pin      10

uint8_t mode = 0;           // 0: stop, 1: calibrate, 2: measurement, 10: switch to stop, 11: swtich to calibrate, 12: switch to measurement
uint32_t ADC4mA = 1, ADC4mAmax = 1, ADC4mAmin= 99999999;
uint32_t prevtime = 0;
void setup() {
  Serial.begin (57600);
  pinMode (Level_EN_pin, OUTPUT);
  pinMode (NET_EN_pin, OUTPUT);
  digitalWrite (Level_EN_pin, LOW);
  digitalWrite (NET_EN_pin, LOW);
  Serial.println ("ADC Calibration Module.");
  Serial.println ("  Move sensor out of water to get 4mA before start calibration.");
  Serial.println ("  Press 'C' to calibrate mode, 'M' to measurement mode.\r\n\r\n");
  prevtime = millis();
}

void loop() {
  while (Serial.available()) {
    String a = Serial.readStringUntil("\r\n");
    if (a.startsWith("c") && (mode!=1)) mode = 11;
    else if (a.startsWith("m") && (mode!=2)) mode = 12;
    else if (mode>0) mode = 10;
  }
  if (mode == 10) {
    Serial.println ("\r\n==========================");
    Serial.println ("  ADC@4mA => avg: "+ String (ADC4mA) + ", min: "+ String (ADC4mAmin) + ", max: "+ String (ADC4mAmax));
    Serial.println ();
    mode = 0;
    digitalWrite (Level_EN_pin, LOW);
  } else if (mode == 11) {
    Serial.println ("\r\n=== Calibration mode ===");
    mode = 1;
    digitalWrite (Level_EN_pin, HIGH);
    delay (500);
  } else if (mode == 12) {
    if (ADC4mA == 1) {
      Serial.println ("\r\n=== Measurement mode ===");
      Serial.println ("  => Please calibrate sensor before measure.");
      mode = 0;
      digitalWrite (Level_EN_pin, LOW);
    } else {
      Serial.println ("\r\n=== Measurement mode ===");
      mode = 2;
      digitalWrite (Level_EN_pin, HIGH);
      delay (500);
    }
  }
  if ((millis() - prevtime) > 1000L) {
    if (mode == 1) {
      getADC4mAfactor();
    } else if (mode == 2) {
      getDistanceSesnor ();
    }
    prevtime = millis();
  }
}

uint16_t getADC4mAfactor() {
  uint16_t A11Value = readADC11();                                // read ADC @1.1v reference
  uint16_t Avalue = 0;
  uint16_t mVcc = (uint16_t)(1125300UL/(uint32_t)A11Value);                // calculate Vcc

  Serial.print ("A11Value: " + String(A11Value) + ", mVcc: "+ String(mVcc));
  for (uint8_t i = 0; i < 8; i++) {
    delay (10);
    Avalue += analogRead(Level_pin);
  }
  Avalue = Avalue/8;
  Serial.print (", Avalue: "+ String(Avalue));
  Avalue = map (Avalue, 0, A11Value, 0, 1024/3);          // normalize Avalue@3.30v
  Serial.print (", ADC4mA@3.3v: "+ String(Avalue));
  
  if (ADC4mA == 1) ADC4mA = Avalue;
  else ADC4mA = (ADC4mA + Avalue)/2;
  if (Avalue < ADC4mAmin) ADC4mAmin = Avalue;
  if (Avalue > ADC4mAmax) ADC4mAmax = Avalue;
  Serial.println (", avg: "+ String (ADC4mA) + ", min: "+ String (ADC4mAmin) + ", max: "+ String (ADC4mAmax));
  return;
}


uint16_t getDistanceSesnor() {                                      // Get ADC value for 4-20mA
  uint16_t A11Value = readADC11();                                  // read ADC @1.1v reference
  uint16_t Avalue = 0;
  uint16_t mVcc = (uint16_t)(1125300UL/(uint32_t)A11Value);                // calculate Vcc
  Serial.print ("A11Value: " + String(A11Value) + ", mVcc: "+ String(mVcc));
  for (uint8_t i = 0; i < 8; i++) {
    delay (10);
    Avalue += analogRead(Level_pin);
  }
  Avalue = Avalue/8;
  Serial.print (", Avalue: "+ String(Avalue));
  Avalue = map (Avalue, 0, A11Value, 0, 1024/3);          // normalize Avalue@3.30v
  Serial.print (", Avalue@3.3v: "+ String(Avalue) + ", ADC4mA@3.3v: "+ String(ADC4mA) + ", ADC20mA@3.3v: "+ String(ADC4mA*5));
  
  Avalue  = map (Avalue, 0, ADC4mA * 5, 0, 1020);     // map ADC 0..20mA -> 0..1020
  Serial.print (", Sensor Value: " + String (Avalue));
  
  Serial.println (", Level: " + String (map (Avalue, 204, 1020, 0, 500)));
  return (Avalue);
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
