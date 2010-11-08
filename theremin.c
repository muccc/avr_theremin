// theremin.c
// for NerdKits with ATmega168
// hevans@nerdkits.com

#define F_CPU 14745600
#define __AVR_ATmega168__ 1

#include <stdio.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define MAX_STATE 65535
// um wieviel wird step modifiziert
#define MOD_DIFF  200
#define COUNTER_MAX 256
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
  btn_tmp = PORTC & (1<<PC2);
  if (btn_tmp == btn_tmp_prev) {
      btn_counter++;
  } else {
      btn_counter = 0;
  }
  if (counter > BTN_TRSH) {
      btn = btn_tmp;
      if (btn == PC2) {
          PORTC |= (1<<PC5); // enable LED
      } else {
          PORTC &= ~(1<<PC5); // disable LED
      }
  }

  return (uint8_t)(rawweighted>>8); 
}

ISR(TIMER1_OVF_vect){ //set the new value of OCR1A
  OCR1A = next_val();
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
  
  //set OC1A for clear on compare match, fast PWM mode, TOP value in ICR1, no prescaler
  //compare mach is done against value in OCR1A
  TCCR1A |= (1<<COM1A1) | (1<<WGM11);
  TCCR1B |= (1<<WGM13) | (1<<WGM12) | (1<<CS10);
  OCR1A = 0;
  ICR1 = 0xFF;

  //enable interrupt on overflow. to set the next value
  TIMSK1 |= (1<<TOIE1);

  // set PB1 as PWM output
  DDRB |= (1<<PB1);
}

void adc_init() {
  // set analog to digital converter
  // for external reference (5v), single ended input ADC0
  ADMUX = 0;

  // set analog to digital converter
  // to be enabled, with a clock prescale of 1/128
  // so that the ADC clock runs at 115.2kHz.
  ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);

  // fire a conversion just to get the ADC warmed up
  ADCSRA |= (1<<ADSC);
}

void btn_init() {
    // set PC2 as input
    DDRC &= ~(1<<PC2);
    // enable internal pullup on PC2
    PORTC |= (1<<PC2);
}

void led_init() {
    //set PC5 as output
    DDRC |= (1<<PC5);
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

double hand_position_0, hand_position_1;

int main() {
/*  uart_init();
  FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
  stdin = stdout = &uart_stream;
*/ 
  state = 0;
  dir = 1;
  step = 100;
  step_mod = 100;
  vol = 255;

  adc_init();
  pwm_timer_init();
  btn_init();
  led_init();
  sei();
  
  double step_double, vol_double;
  while(1) {

    //grab the hand positions
    hand_position_0 = adc_average(0, 25);
    hand_position_1 = adc_average(1, 25);

    // PITCH
    //linear interpolate
    //step size should be between 100-12000
    //adc values between 200-950
    //y - (x-200)*( (12000-100)/(950-200) + 100
    step_double = ((hand_position_0 - 50)* 10) + 100;
    if(step_double < 1) {
      step = 1;
    } else if(step_double > 12000) {
      step = 12000;
    } else {
      step = step_double;
    }
    
    // VOLUME
    vol_double = (hand_position_1 - 10);
    if(vol_double < 0) {
      vol = 0;
    } else if(vol_double >= 255) {
      vol = 255;
    } else {
      vol = vol_double;
    }

  }

}
