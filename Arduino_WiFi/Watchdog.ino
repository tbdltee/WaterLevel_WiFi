void RainGaugeCount (void) {            // pin 2 ISR
  if (INT0_btnCnt > 60) {               // debounce = 60*15 = 900ms = 1 count/sec
    RainCount++;
    INT0_btnCnt = 0;
  }
}

ISR(WDT_vect) {                         // Interrupt raised by the watchdog, inidicated by WDT_vect.
  wdt_reset();                          // Reset watchdog timer
  ++cnt1secWDT;                         // counter increase every 15ms
  if (cnt1secWDT > 57) {                // 1 sec reach (1000/15ms = 67.6667) - delay(100ms)
    cnt1secWDT = 0;
    if (cntRemaining > 0) {             // Check if we are in sleep/waiting mode or it is a genuine WDR.
      --cntRemaining;                   // not hang, decrease the number of remaining avail loops
    } else {                            // Watchdog Interrupt happended. 
      resetFunc();                      // restart program
    }
  }
  ++INT0_btnCnt;
  if (INT0_btnCnt > 250) INT0_btnCnt = 250;
}

void Initialize_wdt(void) {             // Configure the watchdog: sleep cycle 1 seconds to trigger the IRQ without reset the MCU
  wdt_reset();
  cli();                                // disable interrupts for changing the registers
  MCUSR = 0;                            // reset status register flags
  WDTCSR |= 0b00011000;                 // Enter config mode: set WDCE and WDE to '1', leaves other bits unchanged.
  WDTCSR =  0b01000000 | WDTO_15MS;     // set watchdog to interrupt only mode for every 15ms
  sei();                                // re-enable interrupts
}

void sleep(void) {                      // Put the system to deep sleep. Only an interrupt can wake it up.
  digitalWrite(Sensor_ENpin, LOW);
  digitalWrite(MODEM_ENpin, LOW);
  delay (100);
  cntRemaining = calcSleepTime();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Set sleep to full power down.  Only external interrupts or the watchdog timer can wake the CPU!
  sleep_enable();
  power_adc_disable();                  // Turn off the ADC while asleep.
  while (cntRemaining > 0) {            // while in sleep stage
    sleep_mode();                       // MCU is now sleep and execution completely halt! execution will resume at this point
  }
  sleep_disable();                      // When awake, disable sleep mode
  power_all_enable();                   // put everything on again
  delay (100);                          // ensure everything is power up
}
