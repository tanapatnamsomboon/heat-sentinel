/*
 * Heat Sentinel - bare-metal firmware for the ATmega328P @ 8 MHz.
 *
 * Step 1 (toolchain bring-up): this is a placeholder "blinky" that toggles the
 * pin earmarked for the alert LED (PB1). Its only job is to prove that the
 * CMake + avr-gcc build and the `flash` target work on both Windows and macOS.
 * It will be replaced by the real application in later steps.
 */

#ifndef F_CPU
/* Mirrors the -DF_CPU value injected by CMake; keeps IDEs/linters happy. */
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>

/* Provisional pin map - hardware wiring is owned by the user; see board.h later. */
#define LED_DDR  DDRB
#define LED_PORT PORTB
#define LED_BIT  PB1

int main(void)
{
    LED_DDR |= (1 << LED_BIT); /* alert LED pin -> output */

    for (;;) {
        LED_PORT ^= (1 << LED_BIT);
        _delay_ms(500);
    }

    return 0; /* not reached */
}
