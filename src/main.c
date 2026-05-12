/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 5: the SH1106 OLED driver is online. The boot-time I2C scan result is now
 * shown on the screen; the RGB LED also mirrors it (green = bus + OLED OK,
 * yellow = devices found but OLED init failed, red = nothing on the bus) and the
 * buzzer chirps once. Sensor / RTC / WiFi screens arrive in later steps.
 */

#include "hal/i2c.h"
#include "hal/timer.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
#include "drivers/sh1106.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>

static led_color_t s_blink_color = LED_GREEN;

static void status_blink_task(void)
{
    led_toggle(s_blink_color);
}

int main(void)
{
    led_init();
    buzzer_init();
    timer_init();
    i2c_init();
    sei();                          /* millis() needs interrupts running */

    uint8_t found[8];
    uint8_t n = i2c_scan(found, (uint8_t)sizeof found);
    bool oled_ok = sh1106_init();   /* probes 0x3C, runs the init sequence, clears */

    s_blink_color = oled_ok ? LED_GREEN : (n > 0 ? LED_YELLOW : LED_RED);
    buzzer_tone(2300, 120);         /* boot chirp (works for a passive buzzer) */

    if (oled_ok) {
        sh1106_print_line(0, "Heat Sentinel");
        sh1106_print_line(1, "----------------");
        sh1106_set_cursor(0, 3);
        sh1106_puts("I2C devices: ");
        sh1106_put_uint(n);
        for (uint8_t i = 0; i < n && i < (uint8_t)sizeof found; i++) {
            sh1106_set_cursor((uint8_t)((i % 4u) * 32u), (uint8_t)(4u + i / 4u));
            sh1106_putc('0');
            sh1106_putc('x');
            sh1106_put_hex8(found[i]);
        }
    }

    sched_add((n > 0) ? 500 : 150, status_blink_task);

    for (;;) {
        sched_tick();
        buzzer_tick();
        /* further scheduled jobs (sensor, display, RTC, WiFi) join here later */
    }
}
