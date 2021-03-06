// theremin.c
// for NerdKits with ATmega168
// hevans@nerdkits.com

// #define F_CPU 14745600
// #define __AVR_ATmega168__ 1

#include <stdio.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "pins.h"

#define MAX_STATE 65535
// um wieviel wird step modifiziert
#define MOD_DIFF  0 //200
#define COUNTER_MAX 10
#define BTN_TRSH 1000

uint16_t state;	// phase of triangle wave
uint8_t dir;    // 1 = up, 0=down
uint16_t step;  
uint8_t vol;    // volume, 0 to 255

// oscillation
uint16_t step_mod;  
uint16_t counter;
uint8_t dir_mod;

// button
uint8_t btn;
uint8_t btn_tmp;
uint8_t btn_tmp_prev;
uint16_t btn_counter;

uint8_t next_val(){
  // compute next phase, doing rollovers properly
  if(dir == 0 && state < step_mod) {
    // if we'd roll over in the negative direction...
    state = step_mod - state;
    dir = 1;
    counter++;
  } else if(dir == 1 && ((MAX_STATE - step_mod) < state)){
    // if we'd roll over in the positive direction...
    state = state + step_mod;
    state = MAX_STATE - state;
    dir = 0;
    counter++;
  } else {
    // no rollover event this cycle
    if(dir)
      state = state + step_mod;
    else
      state = state - step_mod;
  }

  // compute next volume-weighted sample
  uint8_t raw = (uint8_t)(state>>8); // unweighted
  uint16_t rawweighted = raw * vol;

  // modification of step_mod
  if (counter > COUNTER_MAX) { // counter overflow
      if (dir_mod == 0) {
          dir_mod = 1;
          step_mod = step + MOD_DIFF;
      } else {
          dir_mod = 0;
          step_mod = step - MOD_DIFF;
      }
      counter = 0;
  }

  // check button
  btn_tmp_prev = btn_tmp;
  btn_tmp = PINC & BTN_R2;
  if (btn_tmp == btn_tmp_prev) {
      btn_counter++;
  } else {
      btn_counter = 0;
  }
//  if (counter > BTN_TRSH) {
      btn = btn_tmp;
      if (btn == BTN_R2) {
        PORTD &= ~LED_G1;  //disable LED G1, !!ggf. Register anpassen
      } else {
        PORTD |= LED_G1;   //enable LED G1, !!ggf. Register anpassen
      }
//  }

  return (uint8_t)(rawweighted>>8); 
}

ISR(TIMER1_OVF_vect){ //set the new value of PZR
  PZ1R = PZ2R = next_val();
}

void pwm_timer_init(){
  //// begin math ///
  // song sampled at 11025Hz
  // clk = 14745600Hz (ticks per second)
  // 
  // with TOP value at =0xFF 
  // 14745600 / 256 = 57600Hz
  // end math ///

  //using TIMER1 16-bit
  
  //set PZR for clear on compare match, fast PWM mode, TOP value in ICR1, no prescaler
  //compare mach is done against value in PZR
  TCCR1A |= (1<<COM1A1) | (1<<COM1B1) | (1<<WGM11);
  TCCR1B |= (1<<WGM13) | (1<<WGM12) | (1<<CS10);
  PZ1R = 0;
  PZ2R = 0;
  ICR1 = 0xFF;

  //enable interrupt on overflow. to set the next value
  TIMSK1 |= (1<<TOIE1);

  // set PZ as PWM output
  DDRB |= PZ1;
  DDRB |= PZ2;
}

void adc_init() {
  // set analog to digital converter
  // for external reference (5v), single ended input ADC0
  ADMUX = 0;

  // set analog to digital converter
  // to be enabled, with a clock prescale of 1/128 (ADPS2,1,0 gesetzt)
  // so that the ADC clock runs at 125kHz.
  ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);

  // fire a conversion just to get the ADC warmed up
  ADCSRA |= (1<<ADSC);
}

void btn_init() {
    // set Buttons as input
    DDRC &= ~BTN_R1;
    DDRC &= ~BTN_R2;
    DDRC &= ~BTN_L;
    // enable internal pullup for Buttons 
    PORTC |= BTN_R1;
    PORTC |= BTN_R2;
    PORTC |= BTN_L;
}

void led_init() {
    //set LED ports, !!ggf. Register anpassen
    DDRD |= LED_G1;
    DDRD |= LED_G2;
    DDRD |= LED_G3;
    DDRD |= LED_Y;
    DDRD |= LED_R;
}

uint16_t adc_read(uint8_t mux) {
  // select desired channel
  ADMUX = mux;
  
  // set ADSC bit to get the *next* conversion started
  ADCSRA |= (1<<ADSC);

  // read from ADC, waiting for conversion to finish
  while(ADCSRA & (1<<ADSC)) {
    // do nothing... just hold your breath.
  }
  // bit is cleared, so we have a result.

  // read from the ADCL/ADCH registers, and combine the result
  // Note: ADCL must be read first (datasheet pp. 259)
  uint16_t result = ADCL;
  uint16_t temp = ADCH;
  result = result + (temp<<8);
  
  return result;
}

double adc_average(uint8_t mux, uint8_t count) {
  double foo = 0.0;
  uint8_t i;
  for(i=0; i<count; i++) {
    foo += adc_read(mux);
  }
  return foo / ((double) count);
}

double hand_position_left, hand_position_right;
double adc_left_on, adc_left_off, adc_right_on, adc_right_off;

int main() {
    wdt_disable();
    /* Clear WDRF in MCUSR */
    MCUSR &= ~(1<<WDRF);
    /* Write logical one to WDCE and WDE */
    /* Keep old prescaler setting to prevent unintentional time-out */
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    /* Turn off WDT */
    WDTCSR = 0x00;
/*  uart_init();
  FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
  stdin = stdout = &uart_stream;
*/ 
  state = 0;
  dir = 1;
  step = 100;
  step_mod = 100;
  vol = 255;

  btn_init();
  adc_init();
  pwm_timer_init();
  led_init();
  ir_init();
  sei();

  enable_ir1();
  enable_ir2();
  
  double step_double, vol_double;
  while(1) {

    // filter ambient light
    disable_ir1();
    disable_ir2();
    // grab adc values for background light
    for (uint16_t i = 0; i < 4000; i++) {} // give the LED and PT some time to react
    adc_left_off = adc_average(PT_L, 20);
    adc_right_off = adc_average(PT_R, 20);
    // turn IR_lEDs on again and grab another pair of adc values
    enable_ir1();
    enable_ir2();
    for (uint16_t i = 0; i < 4000; i++) {} // give the LED and PT some time to react
    adc_left_on = adc_average(PT_L, 20);
    adc_right_on = adc_average(PT_R, 20);

    // calculate the difference
    hand_position_left = adc_left_on - adc_left_off;
    hand_position_right = adc_right_on - adc_right_off;

    // PITCH
    //linear interpolate
    //step size should be between 100-12000
    //adc values between 200-950
    //y - (x-200)*( (12000-100)/(950-200) + 100
    step_double = ((hand_position_left - 50)* 10) + 100;
    if(step_double < 100) {
      step = 100;
    } else if(step_double > 12000) {
      step = 12000;
    } else {
      step = step_double;
    }
    
    // VOLUME
    vol_double = (hand_position_right - 10);
    if(vol_double < 0) {
      vol = 0;
    } else if(vol_double >= 255) {
      vol = 255;
    } else {
      vol = vol_double;
    }

  }

}
