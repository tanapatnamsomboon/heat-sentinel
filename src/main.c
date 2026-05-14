#include "hal/i2c.h"
#include "hal/timer.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
#include "drivers/sh1106.h"
#include "drivers/ds1307.h"
#include "drivers/dht11.h"
#include "drivers/ldr.h"
#include "app/scheduler.h"

#include <avr/interrupt.h>
#include <util/delay.h>

static bool        s_oled_ok;
static bool        s_rtc_ok;
static bool        s_dht_valid;
static uint8_t     s_dht_fails;
static led_color_t s_blink_color = LED_GREEN;

/* NIGHT MODE & SENSOR CACHE */
static bool        s_night_mode = false;
static dht11_reading_t s_last_dht;
static uint8_t     s_last_ldr_pct = 0;
static ds1307_time_t s_current_time;

/* HEAT INDEX & GRAPH GLOBALS */
#define GRAPH_WIDTH 21
static uint8_t     s_current_hi = 0;
static uint8_t     s_hi_history[GRAPH_WIDTH] = {0}; 

static void put2(uint8_t v)
{
    sh1106_putc((char)('0' + (v / 10u) % 10u));
    sh1106_putc((char)('0' + (v % 10u)));
}

/* Calculates Heat Index in Celsius */
static uint8_t calc_heat_index(uint8_t t_c, uint8_t rh) {
    if (t_c < 27) return t_c; 
    
    float T = (t_c * 9.0f / 5.0f) + 32.0f; 
    float HI = -42.379f + 2.04901523f*T + 10.14333127f*rh 
               - 0.22475541f*T*rh - 0.00683783f*T*T - 0.05481717f*rh*rh 
               + 0.00122874f*T*T*rh + 0.00085282f*T*rh*rh - 0.00000199f*T*T*rh*rh;
               
    float HI_C = (HI - 32.0f) * 5.0f / 9.0f;
    return (uint8_t)HI_C;
}

static void status_blink_task(void)
{
    if (s_night_mode) {
        led_off();
    } else {
        led_toggle(s_blink_color);
    }
}

static void rtc_task(void)
{
    if (!s_rtc_ok) return;
    if (!ds1307_get_time(&s_current_time)) {
        s_rtc_ok = false;
    }
}

static void sensor_task(void)
{
    /* 1. Read DHT11 */
    dht11_status_t st = dht11_read(&s_last_dht);
    if (st == DHT11_OK) {
        s_dht_valid = true;
        s_dht_fails = 0;
        
        /* Calculate Heat Index */
        s_current_hi = calc_heat_index(s_last_dht.temperature, s_last_dht.humidity);
        
        /* Shift graph history array and append new value (42s window) */
        for(uint8_t i = 0; i < GRAPH_WIDTH - 1; i++) {
            s_hi_history[i] = s_hi_history[i+1];
        }
        s_hi_history[GRAPH_WIDTH - 1] = s_current_hi;
        
    } else if (st != DHT11_BUSY && s_dht_fails < 255) {
        s_dht_fails++;
    }

    /* 2. Read LDR & Process Night Mode */
    s_last_ldr_pct = ldr_read_percent();
    
    bool is_dark = (s_last_ldr_pct < 5); 
    
    if (is_dark != s_night_mode) {
        s_night_mode = is_dark;
        if (s_night_mode) {
            sh1106_set_contrast(0x01); /* Dim screen */
            led_off();                 /* LED off */
            buzzer_off();              /* Mute buzzer */
        } else {
            sh1106_set_contrast(0xCF); /* Normal brightness */
        }
    }
}

static void ui_task(void)
{
    if (!s_oled_ok) return;

    /* Line 0: Date and Time */
    sh1106_set_cursor(0, 0);
    if (s_rtc_ok) {
        sh1106_puts("20"); put2(s_current_time.year); sh1106_putc('-');
        put2(s_current_time.month); sh1106_putc('-'); put2(s_current_time.day);
        sh1106_puts("  ");
        put2(s_current_time.hour); sh1106_putc(':');
        put2(s_current_time.minute); sh1106_putc(':'); put2(s_current_time.second);
    } else {
        sh1106_print_line(0, "RTC: read error     ");
    }

    /* Line 1: Divider */
    sh1106_print_line(1, "---------------------");

    /* Line 2: Raw Sensor Data */
    sh1106_clear_line(2);
    if (s_dht_valid) {
        sh1106_puts("T:"); sh1106_put_uint(s_last_dht.temperature); sh1106_puts("C  ");
        sh1106_puts("H:"); sh1106_put_uint(s_last_dht.humidity); sh1106_puts("%  ");
    } else {
        sh1106_puts("T:--C  H:--%  ");
    }
    sh1106_puts("L:"); sh1106_put_uint(s_last_ldr_pct); sh1106_puts("%");

    /* Line 3: Combined Heat Index & Status */
    sh1106_clear_line(3);
    sh1106_puts("HI:");
    if (!s_dht_valid) {
        sh1106_puts("--C NO SENSOR");
    } else {
        sh1106_put_uint(s_current_hi);
        sh1106_puts("C ");
        
        /* Updated 4-Tier Statuses */
        if (s_current_hi < 27) sh1106_puts("SAFE");
        else if (s_current_hi < 32) sh1106_puts("CAUTION");
        else if (s_current_hi < 41) sh1106_puts("WARNING");
        else sh1106_puts("DANGER");
    }

    /* Line 4: Graph Label */
    sh1106_print_line(4, "----- HI Trend -----");

    /* --- 3-LINE DOT GRAPH --- */
    sh1106_set_cursor(0, 5); /* Line 5: DANGER zone (>= 32C) */
    for (uint8_t i = 0; i < GRAPH_WIDTH; i++) {
        if (s_hi_history[i] >= 32) sh1106_putc('.'); else sh1106_putc(' ');
    }

    sh1106_set_cursor(0, 6); /* Line 6: CAUTION zone (27C - 31C) */
    for (uint8_t i = 0; i < GRAPH_WIDTH; i++) {
        if (s_hi_history[i] >= 27 && s_hi_history[i] < 32) sh1106_putc('.'); else sh1106_putc(' ');
    }

    sh1106_set_cursor(0, 7); /* Line 7: SAFE zone (< 27C) */
    for (uint8_t i = 0; i < GRAPH_WIDTH; i++) {
        if (s_hi_history[i] > 0 && s_hi_history[i] < 27) sh1106_putc('.'); else sh1106_putc(' ');
    }
}

static void alert_check_task(void)
{
    if (!s_dht_valid) return;

    if (s_current_hi < 27) {
        /* SAFE (< 27C) */
        s_blink_color = LED_GREEN;
        
    } else if (s_current_hi < 32) {
        /* CAUTION (27C - 31C) */
        s_blink_color = LED_YELLOW;
        
    } else if (s_current_hi < 41) {
        /* WARNING (32C - 40C) 
         * Using MAGENTA (Purple) because digital RGB cannot mix Orange! */
        s_blink_color = LED_MAGENTA; 
        
        if (!s_night_mode) {
            /* FIXED: Changed from buzzer_beep() to buzzer_tone() */
            // buzzer_tone(2300, 500); 
        }
        
    } else {
        /* DANGER (>= 41C) */
        /* NOTE: To test this indoors, temporarily change the '41' above to something like '30' */
        s_blink_color = LED_RED;
        
        if (!s_night_mode) {
            /* FIXED: Changed from buzzer_beep() to buzzer_tone() */
            buzzer_tone(2300, 500); 
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
    ldr_init();                     
    sei();  
    
    ds1307_set_time(&(ds1307_time_t){
        .second = 0,
        .minute = 45,
        .hour = 11,
        .day = 14,
        .month = 5,
        .year = 26,  
        .weekday = 4
    });
    
    s_oled_ok = sh1106_init();
    int8_t rtc_rc = ds1307_init_or_seed();
    s_rtc_ok = (rtc_rc >= 0);

    if (s_oled_ok) {
        sh1106_print_line(0, "Heat Sentinel");
        sh1106_print_line(1, "---------------------");
        sh1106_print_line(3, "Initializing...");
    }

    buzzer_tone(2300, 120);         

    /* Register Tasks */
    sched_add(1000, rtc_task);           
    sched_add(2000, sensor_task);        
    sched_add(1000, ui_task);            
    sched_add(2000, alert_check_task);   
    sched_add(s_oled_ok ? 500 : 150, status_blink_task);

    for (;;) {
        sched_tick();
        buzzer_tick();
    }
}