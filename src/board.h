#ifndef BOARD_H
#define BOARD_H

/*
 * board.h - hardware pin map and clock for Heat Sentinel (ATmega328P @ 8 MHz).
 *
 * The user owns the physical wiring; adjust the assignments here to match the
 * board. Pins are written as a (PORT_LETTER, BIT) pair so they can be passed
 * straight to the gpio_*() macros in hal/gpio.h, e.g.  gpio_output(LED_R_PIN);
 *
 *   Pin    | Use
 *   -------+--------------------------------------------------------------
 *   PB0    | RGB LED - Red    \
 *   PB1    | RGB LED - Green   >  HW-479 module (common cathode; '-' pin to GND)
 *   PB2    | RGB LED - Blue   /
 *   PB3    | ISP MOSI    \
 *   PB4    | ISP MISO     >  reserved for the programmer ("loader") - do not reuse
 *   PB5    | ISP SCK     /
 *   PB6/7  | XTAL1/XTAL2 (only if an external crystal is used; else free GPIO)
 *   PC0    | DHT11 data (single-wire; needs an external pull-up on the line)
 *   PC4    | I2C SDA  \  hardware TWI - the SH1106 OLED and DS1307 RTC share this
 *   PC5    | I2C SCL  /  bus (wire SDAs together, SCLs together; one ~4.7k pull-up pair)
 *   PC6    | /RESET (ISP)
 *   PD0    | USART RXD  <-  ESP-01 TX   (the ESP-01's RX/TX cross over to PD0/PD1)
 *   PD1    | USART TXD  ->  ESP-01 RX
 *   PD3    | Buzzer signal (= OC2B, so Timer2 can tone-drive a passive buzzer)
 *   PD4    | ESP-01 RST (optional - see ESP_RST_PIN below)
 *
 * Only the ESP-01 uses the UART; the OLED + RTC are on I2C; the DHT11, RGB LED
 * channels and the buzzer are plain GPIO.
 */

#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 8000000UL          /* mirrors -DF_CPU from CMake; fallback for IDEs */
#endif

/* --- Alert LED: RGB module HW-479 (4 pins: '-', R, G, B; common cathode) ---
 * One GPIO per colour, on PB0..PB2 (kept clear of the ISP pins PB3/PB4/PB5). */
#define LED_R_PIN        B, 0    /* PB0 */
#define LED_G_PIN        B, 1    /* PB1 */
#define LED_B_PIN        B, 2    /* PB2 */
#define LED_RGB_ACTIVE_HIGH  1   /* HW-479 is common-cathode: 1 = drive pin HIGH to light */

/* --- DHT11 temperature/humidity sensor (single-wire GPIO) --- */
#define DHT11_PIN        C, 0    /* PC0 - needs an external pull-up on the data line */

/* --- Alert buzzer: module (e.g. MH-FMD) on PD3 = OC2B (so a passive buzzer can
 * be tone-driven by Timer2). MH-FMD-style boards are often active-LOW - if the
 * buzzer is on at rest, set this to 0. */
#define BUZZER_PIN       D, 3    /* PD3 / OC2B */
#define BUZZER_ACTIVE_HIGH   1

/* --- ESP-01 WiFi: USART0 RXD = PD0, TXD = PD1 (fixed) ---
 * Optional hardware reset line - uncomment and wire it if your board uses one
 * (otherwise tie the ESP-01's RST + CH_PD to 3.3 V): */
/* #define ESP_RST_PIN   D, 4 */ /* PD4, active low */

/* --- TMP35 Analog Temperature Sensor ---
 * Connected to PC1 (ADC1). Formula assumes 5V AVCC reference. */
#define TMP35_ADC_CH     1       /* PC1 = ADC Channel 1 */

/* --- MCP3201 12-bit SPI ADC ---
 * Uses Hardware SPI: PB4 (MISO) and PB5 (SCK). These pins are shared with the ISP.
 * The Chip Select (CS) pin is assigned to PC2. */
#define MCP3201_CS_PIN   C, 2    /* PC2 */

#endif /* BOARD_H */
