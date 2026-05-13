#include "drivers/tmp35.h"
#include "hal/adc.h"
#include "board.h"

void tmp35_init(void)
{
    adc_init();
}

uint16_t tmp35_read_temp_x10(void)
{
    uint16_t val = adc_read(TMP35_ADC_CH);
    
    /* TMP35:
     * 10mV per °C
     * Vref = 5000mV
     * ADC = 1024 steps
     *
     * Temp(°C x10) = (ADC * 5000) / 1024
     */
    return (uint16_t)((val * 5000UL) / 1024UL);
}