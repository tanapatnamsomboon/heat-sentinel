#ifndef DRIVERS_LED_H
#define DRIVERS_LED_H

#include <stdbool.h>

/* Alert LED on the pin defined as LED_PIN in board.h (active high). */

void led_init(void);    /* configure the pin as an output, LED off */
void led_on(void);
void led_off(void);
void led_toggle(void);
void led_set(bool on);

#endif /* DRIVERS_LED_H */
