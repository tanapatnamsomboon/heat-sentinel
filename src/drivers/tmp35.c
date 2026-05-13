#include "drivers/tmp35.h"
#include "hal/adc.h"
#include "board.h"
#include <util/delay.h>

void tmp35_init(void)
{
    adc_init();
}

uint16_t tmp35_read_temp_x10(void)
{
    uint32_t sum = 0;
    const uint8_t num_samples = 16;

    /* Take 16 samples to average out the analog noise */
    for (uint8_t i = 0; i < num_samples; i++) {
        sum += adc_read(TMP35_ADC_CH);
        _delay_ms(1); /* Small delay between reads */
    }

    uint16_t avg_val = (uint16_t)(sum / num_samples);

    /* * IMPORTANT: Change 5000UL to 3300UL if your USBasp is set to 3.3V!
     * Assuming 5V system here.
     */
    uint16_t mv = (uint16_t)(avg_val * 3300UL / 1024UL);

    /* Assuming TMP36 (has 500mV offset).
     * If using TMP35, remove the "- 500" part. */
    if (mv > 500) {
        return (mv - 500);
    }
    return 0;
}