/*
 * Heat Sentinel - ATmega328P @ 8 MHz.
 *
 * Step 8a: the USART (UART) driver is online. At boot, after bringing up the
 * OLED / RTC / DHT11, the firmware pings the ESP-01 with "AT" over the UART
 * @ 9600 baud and shows the result on the OLED. The 1 Hz clock and the 2 s
 * temp/humidity readout keep running. The ESP-01 AT state machine + WiFi
 * upload arrive in Step 8b.
 */

#include "hal/i2c.h"
#include "hal/timer.h"
#include "hal/uart.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
#include "drivers/sh1106.h"
#include "drivers/ds1307.h"
#include "drivers/dht11.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>
#include <util/delay.h>

#define ESP_UART_BAUD  9600UL    /* the ESP-01 must be set to this (AT+UART_DEF=9600,8,1,0,0) */

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

/* Boot-time UART/ESP-01 probe: send "AT", look for an "OK" reply, and show the
 * outcome on line 7. (Bring-up only - replaced by the ESP-01 state machine in
 * Step 8b.) */
static void uart_probe_and_show(void)
{
    _delay_ms(500);                 /* let the ESP-01 finish booting */

    bool    esp_ok = false;
    char    head[14];               /* first few printable RX bytes, for the OLED */
    uint8_t hn = 0;

    for (uint8_t attempt = 0; attempt < 3 && !esp_ok; attempt++) {
        uart_flush_rx();
        uart_puts("AT\r\n");
        _delay_ms(150);
        uint8_t prev = 0;
        int ch;
        while ((ch = uart_getc()) >= 0) {
            uint8_t b = (uint8_t)ch;
            if (b == (uint8_t)'K' && prev == (uint8_t)'O') {
                esp_ok = true;
            }
            prev = b;
            if (hn < 13 && b >= 0x20 && b < 0x7F) {
                head[hn++] = (char)b;
            }
        }
    }
    head[hn] = '\0';

    if (s_oled_ok) {
        sh1106_clear_line(7);
        if (esp_ok) {
            sh1106_puts("ESP-01: ready");
        } else if (hn > 0) {
            sh1106_puts("UART rx: ");
            sh1106_puts(head);
        } else {
            sh1106_puts("UART: no reply");
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
        sh1106_print_line(4, "Temp: -- C");
        sh1106_print_line(5, "Humi: -- %");
        sh1106_set_cursor(0, 6);
        sh1106_puts("I2C:");
        sh1106_put_uint(n);
        for (uint8_t i = 0; i < n && i < 4; i++) {
            sh1106_putc(' ');
            sh1106_put_hex8(found[i]);
        }
    }

    uart_probe_and_show();

    buzzer_tone(2300, 120);         /* boot chirp (works for a passive buzzer) */

    if (s_rtc_ok) {
        rtc_task();                 /* show the time straight away ... */
        sched_add(1000, rtc_task);  /* ... then refresh once a second  */
    }
    sched_add(2000, dht_task);      /* DHT11: every 2 s (>= its 1 s minimum) */
    sched_add((n > 0) ? 500 : 150, status_blink_task);

    for (;;) {
        sched_tick();
        buzzer_tick();
        /* the ESP-01 AT state machine joins here in Step 8b */
    }
}
