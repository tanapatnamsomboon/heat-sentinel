#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>

/*
 * 10-bit ADC driver.
 * Uses AVCC (5V) as the voltage reference.
 */

void     adc_init(void);
uint16_t adc_read(uint8_t channel);

#endif /* HAL_ADC_H */