#ifndef _PINS_H
#define _PINS_H 1

/* Zuordnung der Bauteile zu den Pins am ATmega */

/* farbige LEDs */
#define LED_G1 (1<<PD0)  /* LED3 */
#define LED_G2 (1<<PD1)  /* LED4 */
#define LED_G3 (1<<PD2)  /* LED5 */
#define LED_Y  (1<<PD3)  /* LED6 */
#define LED_R  (1<<PD4)  /* LED7 */

/* Buttons */
#define BTN_L  (1<<PC1) /* links */
#define BTN_R1 (1<<PC4) /* rechts oben */
#define BTN_R2 (1<<PC3) /* rechts unten */

/* Piezo */
#define PZ1R OCR1A       /* zugehoeriges Register */
#define PZ1 (1<<PB1)     /* Pin */

/* Speaker ("Piezo2") */
#define PZ2R OCR1B       /* zugehoeriges Register */
#define PZ2 (1<<PB2)     /* Pin */

/* IR Phototransistoren */
#define PT_L 0   /*PT1, links */
#define PT_R 2   /*PT2, rechts */

/* IR LEDs */
#define ir_init() DDRB |= (1<<PB0); DDRD |= (1<<PD7)
#define disable_ir1() PORTB |= (1<<PB0)
#define disable_ir2() PORTD |= (1<<PD7)
#define enable_ir1() PORTB &= ~(1<<PB0)
#define enable_ir2() PORTD &= ~(1<<PD7)

#endif /* _PINS_H */
