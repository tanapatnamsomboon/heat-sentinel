/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 8b: The ESP-01 AT state machine is now integrated. The superloop
 * continuously ticks the ESP-01 driver to process UART RX and advance its
 * state (Reset -> CWMODE -> CWJAP -> IDLE). A scheduled task posts the latest
 * temperature and humidity telemetry to the cloud every 30 seconds.
 */

#include "hal/i2c.h"
#include "hal/timer.h"
#include "hal/uart.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
#include "drivers/sh1106.h"
#include "drivers/ds1307.h"
#include "drivers/dht11.h"
#include "drivers/tmp35.h"
#include "drivers/esp01.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#define ESP_UART_BAUD  9600UL    /* the ESP-01 must be set to this (AT+UART_DEF=9600,8,1,0,0) */

static bool        s_oled_ok;
static bool        s_rtc_ok;
static bool        s_dht_valid;     /* at least one good DHT11 read so far */
static uint8_t     s_dht_fails;     /* consecutive failed DHT11 reads */
static led_color_t s_blink_color = LED_GREEN;

static dht11_reading_t s_last_dht;  /* Cached reading for telemetry uploads */

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
    dht11_status_t st = dht11_read(&s_last_dht);

    if (st == DHT11_OK) {
        s_dht_valid = true;
        s_dht_fails = 0;
        if (s_oled_ok) {
            sh1106_clear_line(4);
            sh1106_puts("DHT: ");
            sh1106_put_uint(s_last_dht.temperature);
            sh1106_puts("C, ");
            sh1106_put_uint(s_last_dht.humidity);
            sh1106_puts("%");
        }
    } else if (st != DHT11_BUSY) {
        if (s_dht_fails < 255) s_dht_fails++;
        if (!s_dht_valid && s_dht_fails >= 3 && s_oled_ok) {
            sh1106_print_line(4, "DHT: no sensor?");
        }
    }
}

static void tmp35_task(void)
{
    if (s_oled_ok) {
        uint16_t t_x10 = tmp35_read_temp_x10();
        sh1106_clear_line(5);
        sh1106_puts("TMP: ");
        sh1106_put_uint(t_x10 / 10u);
        sh1106_putc('.');
        sh1106_put_uint(t_x10 % 10u);
        sh1106_puts(" C");
    }
}

static void ui_esp_task(void)
{
    if (s_oled_ok) {
        sh1106_print_line(7, esp01_get_state_str());
    }
}

static void telemetry_post_task(void)
{
    if (s_dht_valid) {
        esp01_post(s_last_dht.temperature, s_last_dht.humidity);
    }
}

int main(void)
{
    led_init();
    buzzer_init();
    timer_init();
    i2c_init();
    dht11_init();
    tmp35_init();
    uart_init(ESP_UART_BAUD);
    sei();                          /* millis() + UART RX need interrupts running */

    uint8_t found[8];
    uint8_t n = i2c_scan(found, (uint8_t)sizeof found);
    s_oled_ok = sh1106_init();
    int8_t rtc_rc = ds1307_init_or_seed();
    s_rtc_ok = (rtc_rc >= 0);

    s_blink_color = s_oled_ok ? LED_GREEN : (n > 0 ? LED_YELLOW : LED_RED);

    if (s_oled_ok) {
        sh1106_print_line(0, "Heat Sentinel");
        sh1106_print_line(1, "----------------");
        if (!s_rtc_ok) {
            sh1106_print_line(2, "RTC: not found");
        }
        sh1106_print_line(4, "DHT: --C, --%");
        sh1106_print_line(5, "TMP: --.- C");
        sh1106_set_cursor(0, 6);
        sh1106_puts("I2C:");
        sh1106_put_uint(n);
        for (uint8_t i = 0; i < n && i < 4; i++) {
            sh1106_putc(' ');
            sh1106_put_hex8(found[i]);
        }
    }

    esp01_init();

    buzzer_tone(2300, 120);         /* boot chirp */

    if (s_rtc_ok) {
        rtc_task();                 /* show the time straight away ... */
        sched_add(1000, rtc_task);  /* ... then refresh once a second  */
    }
    sched_add(2000, dht_task);      /* DHT11: every 2 s (>= its 1 s minimum) */
    sched_add(1000, tmp35_task);    /* TMP35: every 1 s */
    sched_add((n > 0) ? 500 : 150, status_blink_task);

    /* Update ESP-01 status on the OLED every 500 ms */
    sched_add(500, ui_esp_task);

    /* Queue a telemetry upload to the cloud every 30 seconds */
    sched_add(30000, telemetry_post_task);

    for (;;) {
        sched_tick();
        buzzer_tick();
        esp01_tick();               /* Process UART RX and step the AT state machine */
    }
}