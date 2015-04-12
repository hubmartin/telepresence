

#include <avr/sleep.h>
#include <avr/wdt.h>


void wdtlib_pwr_down(char value)
{
  
    //WDTCSR |= (1<<WDCE) | (1<<WDE);
    
        Serial.end();
  
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  
   digitalWrite(0, LOW);
    digitalWrite(1, LOW);

  /* set new watchdog timeout prescaler value */
  //WDTCSR = WDTO_2S;
  wdt_enable(value);
  
  /* Enable the WD interrupt (note no reset). */
  WDTCSR |= _BV(WDIE);
  
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  
  PRR |= _BV(PRUSART0) | _BV(PRADC) |_BV(PRTIM0);
   
   
  sleep_enable();
//attachInterrupt(0, pin2_isr, LOW);
/* 0, 1, or many lines of code here */
set_sleep_mode(SLEEP_MODE_PWR_DOWN);
cli();
//sleep_bod_disable();
sei();
sleep_cpu();
/* wake up here */
sleep_disable();

PRR = 0;
  
}


ISR(WDT_vect)
{
  // disable next isr
  wdt_disable();
  WDTCSR &= ~_BV(WDIE);
}
