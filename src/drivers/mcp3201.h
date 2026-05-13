#ifndef DRIVERS_MCP3201_H
#define DRIVERS_MCP3201_H

#include <stdint.h>

/*
 * MCP3201 12-bit ADC over SPI.
 * Returns a value from 0 to 4095.
 */

void mcp3201_init(void);
uint16_t mcp3201_read(void);

#endif /* DRIVERS_MCP3201_H */