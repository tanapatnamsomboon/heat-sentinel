/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 7: the DHT11 driver is online. A 2 s scheduled task now reads temperature
 * and humidity and shows them on the OLED, alongside the 1 Hz RTC clock and the
 * boot-time I2C scan. The threshold-driven RGB/buzzer alert and the WiFi upload
 * arrive in Steps 8-9. RGB LED still mirrors OLED status (green/yellow/red).
 */

#include "hal/i2c.h"
#include "hal/timer.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
#include "drivers/sh1106.h"
#include "drivers/ds1307.h"
#include "drivers/dht11.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>

static bool        s_oled_ok;
static bool        s_rtc_ok;
static bool        s_dht_valid;     /* at least one good DHT11 read so far */
static uint8_t     s_dht_fails;     /* consecutive failed DHT11 reads */
static led_color_t s_blink_color = LED_GREEN;

static void put2(uint8_t v)         /* two zero-padded decimal digits */
{
    sh1106_putc((char)('0' + (v / 10u) % 10u));
    sh1106_putc((char)('0' + (v % 10u)));
}

static void status_blink_task(void)
{
    led_toggle(s_blink_color);
}

static void rtc_task(void)
{
    if (!s_rtc_ok) {
        return;
    }
    ds1307_time_t t;
    if (!ds1307_get_time(&t)) {
        s_rtc_ok = false;
        if (s_oled_ok) {
            sh1106_print_line(2, "RTC: read error");
            sh1106_clear_line(3);
        }
        return;
    }
    if (!s_oled_ok) {
        return;
    }
    sh1106_set_cursor(0, 2);
    sh1106_puts("20");
    put2(t.year);  sh1106_putc('-');
    put2(t.month); sh1106_putc('-');
    put2(t.day);
    sh1106_set_cursor(0, 3);
    put2(t.hour);   sh1106_putc(':');
    put2(t.minute); sh1106_putc(':');
    put2(t.second);
}

static void dht_task(void)
{
    dht11_reading_t r = { 0, 0 };
    dht11_status_t st = dht11_read(&r);

    if (st == DHT11_OK) {
        s_dht_valid = true;
        s_dht_fails = 0;
        if (s_oled_ok) {
            sh1106_clear_line(4);
            sh1106_puts("Temp: ");
            sh1106_put_uint(r.temperature);
            sh1106_puts(" C");
            sh1106_clear_line(5);
            sh1106_puts("Humi: ");
            sh1106_put_uint(r.humidity);
            sh1106_puts(" %");
        }
    } else if (st != DHT11_BUSY) {
        if (s_dht_fails < 255) {
            s_dht_fails++;
        }
        if (!s_dht_valid && s_dht_fails >= 3 && s_oled_ok) {
            sh1106_print_line(4, "Temp: no sensor?");
            sh1106_clear_line(5);
        }
    }
}

int main(void)
{
    led_init();
    buzzer_init();
    timer_init();
    i2c_init();
    dht11_init();
    sei();                          /* millis() needs interrupts running */

    uint8_t found[8];
    uint8_t n = i2c_scan(found, (uint8_t)sizeof found);
    s_oled_ok = sh1106_init();
    int8_t rtc_rc = ds1307_init_or_seed();
    s_rtc_ok = (rtc_rc >= 0);

    s_blink_color = s_oled_ok ? LED_GREEN : (n > 0 ? LED_YELLOW : LED_RED);
    buzzer_tone(2300, 120);         /* boot chirp (works for a passive buzzer) */

    if (s_oled_ok) {
        sh1106_print_line(0, "Heat Sentinel");
        sh1106_print_line(1, "----------------");
        if (!s_rtc_ok) {
            sh1106_print_line(2, "RTC: not found");
        }
        sh1106_print_line(4, "Temp: -- C");
        sh1106_print_line(5, "Humi: -- %");
        sh1106_set_cursor(0, 6);
        sh1106_puts("I2C dev: ");
        sh1106_put_uint(n);
        for (uint8_t i = 0; i < n && i < 3; i++) {
            sh1106_set_cursor((uint8_t)(i * 40u), 7);
            sh1106_putc('0');
            sh1106_putc('x');
            sh1106_put_hex8(found[i]);
        }
    }

    if (s_rtc_ok) {
        rtc_task();                 /* show the time straight away ... */
        sched_add(1000, rtc_task);  /* ... then refresh once a second  */
    }
    sched_add(2000, dht_task);      /* DHT11: every 2 s (>= its 1 s minimum) */
    sched_add((n > 0) ? 500 : 150, status_blink_task);

    for (;;) {
        sched_tick();
        buzzer_tick();
        /* further scheduled jobs (WiFi upload) join here later */
    }
}
