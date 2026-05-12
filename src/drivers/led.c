#include "drivers/led.h"
#include "board.h"
#include "hal/gpio.h"

/* Pin level that drives a channel ON / OFF, per the module's polarity.
 * (We go through gpio_write(pin, level) rather than gpio_high/gpio_low so the
 * (LETTER, BIT) pin macro from board.h expands correctly through one wrapper.) */
#if LED_RGB_ACTIVE_HIGH
#  define LED_LEVEL_(on)  ((on) ? 1 : 0)
#else
#  define LED_LEVEL_(on)  ((on) ? 0 : 1)
#endif

static led_color_t s_color = LED_OFF;

void led_init(void)
{
    /* park the channels OFF before enabling their output drivers */
    gpio_write(LED_R_PIN, LED_LEVEL_(0));
    gpio_write(LED_G_PIN, LED_LEVEL_(0));
    gpio_write(LED_B_PIN, LED_LEVEL_(0));
    gpio_output(LED_R_PIN);
    gpio_output(LED_G_PIN);
    gpio_output(LED_B_PIN);
    s_color = LED_OFF;
}

void led_set(led_color_t color)
{
    gpio_write(LED_R_PIN, LED_LEVEL_(color & LED_RED));
    gpio_write(LED_G_PIN, LED_LEVEL_(color & LED_GREEN));
    gpio_write(LED_B_PIN, LED_LEVEL_(color & LED_BLUE));
    s_color = color;
}

led_color_t led_get(void)
{
    return s_color;
}

void led_off(void)
{
    led_set(LED_OFF);
}

void led_toggle(led_color_t color)
{
    led_set((s_color == color) ? LED_OFF : color);
}
