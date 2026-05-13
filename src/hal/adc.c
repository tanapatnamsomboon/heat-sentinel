#include "hal/adc.h"
#include <avr/io.h>

void adc_init(void)
{
    /* REFS0 = 1: Voltage Reference is AVCC with external capacitor at AREF pin */
    ADMUX = (1 << REFS0);
    
    /* ADEN = 1: Enable ADC
     * ADPS2:1 = 1: Prescaler = 64 (8 MHz / 64 = 125 kHz ADC clock, ideal is 50-200 kHz) */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

uint16_t adc_read(uint8_t channel)
{
    /* Clear bottom 4 bits (MUX3:0) and set the desired channel */
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    
    /* Start conversion */
    ADCSRA |= (1 << ADSC);
    
    /* Wait for conversion to complete (ADSC becomes 0) */
    while (ADCSRA & (1 << ADSC)) {
        /* spin */
    }
    
    /* Return the 10-bit result */
    return ADC;
}