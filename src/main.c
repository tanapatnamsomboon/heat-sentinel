/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 2: core infrastructure online. main() brings up the 1 ms millis() tick,
 * the LED driver and the cooperative scheduler, then registers one job that
 * blinks the alert LED at ~1 Hz as a liveness heartbeat. Sensor reads, the OLED,
 * the RTC, WiFi telemetry and the real threshold-driven alert are layered on in
 * the steps that follow.
 */

#include "hal/timer.h"
#include "drivers/led.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>

static void heartbeat_task(void)
{
    led_toggle();
}

int main(void)
{
    led_init();
    timer_init();
    sei();                          /* millis() needs interrupts running */

    sched_add(500, heartbeat_task); /* toggle every 500 ms -> ~1 Hz blink */

    for (;;) {
        sched_tick();
        /* further scheduled jobs (sensor, display, RTC, WiFi) join here later */
    }
}
