#include "drivers/led.h"
#include "board.h"
#include "hal/gpio.h"

#if LED_RGB_ACTIVE_HIGH
#  define LED_CH_ON_(pin)   gpio_high(pin)
#  define LED_CH_OFF_(pin)  gpio_low(pin)
#else
#  define LED_CH_ON_(pin)   gpio_low(pin)
#  define LED_CH_OFF_(pin)  gpio_high(pin)
#endif

static led_color_t s_color = LED_OFF;

void led_init(void)
{
    /* park the channels at the inactive level before enabling outputs */
    LED_CH_OFF_(LED_R_PIN);
    LED_CH_OFF_(LED_G_PIN);
    LED_CH_OFF_(LED_B_PIN);
    gpio_output(LED_R_PIN);
    gpio_output(LED_G_PIN);
    gpio_output(LED_B_PIN);
    s_color = LED_OFF;
}

void led_set(led_color_t color)
{
    if (color & LED_RED)   { LED_CH_ON_(LED_R_PIN); } else { LED_CH_OFF_(LED_R_PIN); }
    if (color & LED_GREEN) { LED_CH_ON_(LED_G_PIN); } else { LED_CH_OFF_(LED_G_PIN); }
    if (color & LED_BLUE)  { LED_CH_ON_(LED_B_PIN); } else { LED_CH_OFF_(LED_B_PIN); }
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
