/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 4: RGB LED (HW-479) + buzzer drivers are online. With no display yet, the
 * boot-time I2C scan shows up as a colour on the alert LED:
 *   - green, slow blink -> at least one device ACKed (bus OK; once both are
 *                          wired, expect the SH1106 OLED @ 0x3C and the
 *                          DS1307 RTC @ 0x68)
 *   - red,   fast blink -> nothing answered (check SDA/SCL wiring & pull-ups)
 * A short chirp at boot confirms the buzzer works. Step 5 adds the OLED.
 */

#include "hal/i2c.h"
#include "hal/timer.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
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

    s_blink_color = (n > 0) ? LED_GREEN : LED_RED;
    buzzer_tone(2300, 120);         /* boot chirp (works for a passive buzzer) */
    sched_add((n > 0) ? 500 : 150, status_blink_task);

    for (;;) {
        sched_tick();
        buzzer_tick();              /* end the beep/tone once its duration elapses */
        /* further scheduled jobs (sensor, display, RTC, WiFi) join here later */
    }
}
