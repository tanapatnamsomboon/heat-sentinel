#ifndef BOARD_H
#define BOARD_H

/*
 * board.h - hardware pin map and clock for Heat Sentinel (ATmega328P @ 8 MHz).
 *
 * The user owns the physical wiring; adjust the pin assignments here to match
 * the board. Pins are written as a (PORT_LETTER, BIT) pair so they can be passed
 * straight to the gpio_*() macros in hal/gpio.h, e.g.  gpio_output(LED_PIN);
 *
 * I2C (SDA = PC4, SCL = PC5) and USART (RXD = PD0, TXD = PD1) live on fixed
 * ATmega328P pins and are listed here only for reference.
 */

#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 8000000UL          /* mirrors -DF_CPU from CMake; fallback for IDEs */
#endif

/* --- Alert LED (GPIO output, active high) --- */
#define LED_PIN          B, 1    /* PB1 */

/* --- DHT11 temperature/humidity sensor (single-wire GPIO) --- */
#define DHT11_PIN        B, 0    /* PB0 */

/* --- ESP-01 WiFi: USART0 RXD = PD0, TXD = PD1 (fixed) ---
 * Optional hardware reset line - uncomment and wire it if your board uses one: */
/* #define ESP_RST_PIN   D, 4 */ /* PD4, active low */

#endif /* BOARD_H */
