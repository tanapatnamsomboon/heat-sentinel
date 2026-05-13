#ifndef DRIVERS_TMP35_H
#define DRIVERS_TMP35_H

#include <stdint.h>

/*
 * TMP35 Analog Temperature Sensor driver.
 * Returns temperature multiplied by 10 to preserve 1 decimal place
 * using integer math.
 */

void     tmp35_init(void);
uint16_t tmp35_read_temp_x10(void); /* e.g., 254 means 25.4 °C */

#endif /* DRIVERS_TMP35_H */