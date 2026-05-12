#ifndef DRIVERS_LED_H
#define DRIVERS_LED_H

#include <stdint.h>

/*
 * RGB alert LED - the HW-479 module wired to LED_R_PIN / LED_G_PIN / LED_B_PIN
 * (see board.h). Each channel is digital on/off, so there are 8 states: off plus
 * the 7 colour combinations. Polarity follows LED_RGB_ACTIVE_HIGH.
 *
 * (Brightness/colour mixing via PWM is a possible future extension; for status
 * indication plain on/off colours are plenty.)
 */

typedef enum {
    LED_OFF     = 0,
    LED_RED     = 1u << 0,
    LED_GREEN   = 1u << 1,
    LED_BLUE    = 1u << 2,
    LED_YELLOW  = LED_RED | LED_GREEN,
    LED_CYAN    = LED_GREEN | LED_BLUE,
    LED_MAGENTA = LED_RED | LED_BLUE,
    LED_WHITE   = LED_RED | LED_GREEN | LED_BLUE,
} led_color_t;

void        led_init(void);                 /* channels -> outputs, LED off */
void        led_set(led_color_t color);     /* show this colour (LED_OFF = dark) */
led_color_t led_get(void);                  /* the colour last passed to led_set() */
void        led_off(void);                  /* == led_set(LED_OFF) */
void        led_toggle(led_color_t color);  /* show `color`, or turn off if already showing it (handy for blinking one colour) */

#endif /* DRIVERS_LED_H */
