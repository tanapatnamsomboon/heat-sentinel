#ifndef DRIVERS_LDR_H
#define DRIVERS_LDR_H

#include <stdint.h>

/*
 * Light Dependent Resistor (LDR) driver using the MCP3201 12-bit SPI ADC.
 * Assumes the LDR is part of a voltage divider connected to the MCP3201 input.
 */

void     ldr_init(void);
uint16_t ldr_read_raw(void);       /* Returns raw 12-bit ADC value (0-4095) */
uint8_t  ldr_read_percent(void);   /* Returns light level as 0-100% */

#endif /* DRIVERS_LDR_H */