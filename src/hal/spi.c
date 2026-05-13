#include "hal/spi.h"
#include <avr/io.h>

void spi_init(void)
{
    /* PB3 (MOSI), PB5 (SCK), and PB2 (SS) must be outputs for Master mode.
     * Note: Even if we use a different pin for Chip Select, PB2 (SS) MUST
     * be an output, otherwise if it's pulled low, the SPI hardware will
     * switch to Slave mode automatically. */
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << PB2);

    /* Enable SPI, Master, set clock rate fck/16 */
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

uint8_t spi_transfer(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & (1 << SPIF))) {
        /* wait for transmission complete */
    }
    return SPDR;
}