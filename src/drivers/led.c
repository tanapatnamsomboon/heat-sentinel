#include "drivers/led.h"
#include "board.h"
#include "hal/gpio.h"

void led_init(void)
{
    gpio_low(LED_PIN);      /* drive low first so it stays off when DDR flips */
    gpio_output(LED_PIN);
}

void led_on(void)     { gpio_high(LED_PIN); }
void led_off(void)    { gpio_low(LED_PIN); }
void led_toggle(void) { gpio_toggle(LED_PIN); }
void led_set(bool on) { gpio_write(LED_PIN, on); }
