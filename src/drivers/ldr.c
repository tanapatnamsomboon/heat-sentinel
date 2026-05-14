#include "drivers/ldr.h"
#include "drivers/mcp3201.h"

void ldr_init(void)
{
    mcp3201_init();
}

uint16_t ldr_read_raw(void)
{
    return mcp3201_read();
}

uint8_t ldr_read_percent(void)
{
    uint16_t raw = mcp3201_read();
    
    /* Convert 12-bit value (0-4095) to a percentage (0-100) 
     * Note: Depending on whether your LDR is tied to VCC or GND in the 
     * voltage divider, you may need to invert this by doing:
     * return 100 - (uint8_t)(((uint32_t)raw * 100UL) / 4095UL);
     */
    return (uint8_t)(((uint32_t)raw * 100UL) / 4095UL);
}