#include "drivers/tmp35.h"
#include "drivers/mcp3201.h"
#include "board.h"
#include <util/delay.h>

void tmp35_init(void)
{
    mcp3201_init();
}

uint16_t tmp35_read_temp_x10(void)
{
    uint32_t sum = 0;
    const uint8_t num_samples = 16;

    /* Take 16 samples to average out the analog noise */
    for (uint8_t i = 0; i < num_samples; i++) {
        sum += mcp3201_read();
        _delay_ms(1); /* Small delay between reads */
    }

    uint16_t avg_val = (uint16_t)(sum / num_samples);

    /* * IMPORTANT: Change 5000UL to 3300UL if your USBasp is set to 3.3V!
     * Assuming 5V system here.
     */
    uint32_t mv = ((uint32_t)avg_val * 5000UL) / 1024UL;

    /* Assuming TMP36 (has 500mV offset).
     * If using TMP35, remove the "- 500" part. */
    return (uint16_t)mv;
}