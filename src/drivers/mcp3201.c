#include "drivers/mcp3201.h"
#include "hal/spi.h"
#include "hal/gpio.h"
#include "board.h"

void mcp3201_init(void)
{
    spi_init();
    gpio_output(MCP3201_CS_PIN);
    gpio_high(MCP3201_CS_PIN);  /* Deselect the ADC by default */
}

uint16_t mcp3201_read(void)
{
    gpio_low(MCP3201_CS_PIN);   /* Select the ADC */

    /* MCP3201 requires 2 bytes (16 clocks) to output its 12-bit reading.
     * Data format clocked out on MISO:
     * Byte 1: ? ? ? Null D11 D10 D9 D8 D7
     * Byte 2: D6 D5 D4 D3 D2 D1 D0 ?
     */
    uint8_t b1 = spi_transfer(0x00);
    uint8_t b2 = spi_transfer(0x00);

    gpio_high(MCP3201_CS_PIN);  /* Deselect */

    /* Extract the 12 bits */
    uint16_t value = ((uint16_t)(b1 & 0x1F) << 7) | (b2 >> 1);

    return value;
}