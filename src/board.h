#ifndef BOARD_H
#define BOARD_H

/*
 * board.h - hardware pin map and clock for Heat Sentinel (ATmega328P @ 8 MHz).
 *
 * The user owns the physical wiring; adjust the pin assignments here to match
 * the board. Pins are written as a (PORT_LETTER, BIT) pair so they can be passed
 * straight to the gpio_*() macros in hal/gpio.h, e.g.  gpio_output(LED_R_PIN);
 *
 * Fixed-function pins (not configurable, listed for reference):
 *   I2C  - SDA = PC4, SCL = PC5      (SH1106 OLED + DS1307 RTC)
 *   USART- RXD = PD0, TXD = PD1      (ESP-01 WiFi)
 */

#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 8000000UL          /* mirrors -DF_CPU from CMake; fallback for IDEs */
#endif

/* --- DHT11 temperature/humidity sensor (single-wire GPIO) --- */
#define DHT11_PIN        B, 0    /* PB0 */

/* --- Alert LED: RGB module HW-479 (4 pins: '-', R, G, B; common cathode) ---
 * One GPIO per colour. Note PB3 is also MOSI on the ISP header - harmless, the
 * blue channel may just flicker while flashing. */
#define LED_R_PIN        B, 1    /* PB1 */
#define LED_G_PIN        B, 2    /* PB2 */
#define LED_B_PIN        B, 3    /* PB3 (also ISP MOSI) */
#define LED_RGB_ACTIVE_HIGH  1   /* HW-479 is common-cathode: 1 = drive pin HIGH to light */

/* --- Alert buzzer: module on PD3 = OC2B (so a passive buzzer can be tone-driven
 * by Timer2). MH-FMD-style boards are often active-LOW - if the buzzer is on at
 * rest, set this to 0. */
#define BUZZER_PIN       D, 3    /* PD3 / OC2B */
#define BUZZER_ACTIVE_HIGH   1

/* --- ESP-01 WiFi: USART0 RXD = PD0, TXD = PD1 (fixed) ---
 * Optional hardware reset line - uncomment and wire it if your board uses one: */
/* #define ESP_RST_PIN   D, 4 */ /* PD4, active low */

#endif /* BOARD_H */
