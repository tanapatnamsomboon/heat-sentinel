/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 3: the I2C (TWI) HAL is online. With no display yet, the alert LED
 * reports the boot-time bus scan:
 *   - slow ~1 Hz blink  -> at least one device ACKed (bus OK; once both are
 *                          wired, expect the SH1106 OLED @ 0x3C and the
 *                          DS1307 RTC @ 0x68)
 *   - fast ~3 Hz blink  -> nothing answered (check SDA/SCL wiring & pull-ups)
 * Step 4 replaces this with a proper on-screen device list.
 */

#include "hal/i2c.h"
#include "hal/timer.h"
#include "drivers/led.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>

static void status_blink_task(void)
{
    led_toggle();
}

int main(void)
{
    led_init();
    timer_init();
    i2c_init();
    sei();                          /* millis() needs interrupts running */

    uint8_t found[8];
    uint8_t n = i2c_scan(found, (uint8_t)sizeof found);

    /* Blink rate doubles as a "did we see anything on the I2C bus?" indicator. */
    uint16_t blink_ms = (n > 0) ? 500 : 150;
    sched_add(blink_ms, status_blink_task);

    for (;;) {
        sched_tick();
        /* further scheduled jobs (sensor, display, RTC, WiFi) join here later */
    }
}
