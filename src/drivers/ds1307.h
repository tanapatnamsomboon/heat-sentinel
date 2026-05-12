#ifndef DRIVERS_DS1307_H
#define DRIVERS_DS1307_H

#include <stdint.h>
#include <stdbool.h>

/*
 * DS1307 real-time clock over the hal/i2c TWI master. Time is kept in 24-hour
 * mode; on-chip values are BCD and are converted to/from plain decimal here.
 *
 * The DS1307 powers up (or recovers from a dead backup battery) with the
 * clock-halt (CH) bit set, meaning the oscillator is stopped and the stored
 * time is meaningless. ds1307_init_or_seed() detects that at boot and, if so,
 * seeds the clock from the firmware's build timestamp and starts it.
 */

#ifndef DS1307_I2C_ADDR
#define DS1307_I2C_ADDR  0x68    /* 7-bit; the DS1307 address is fixed */
#endif

typedef struct {
    uint8_t second;   /* 0..59 */
    uint8_t minute;   /* 0..59 */
    uint8_t hour;     /* 0..23 (24-hour) */
    uint8_t day;      /* day of month, 1..31 */
    uint8_t month;    /* 1..12 */
    uint8_t year;     /* 0..99  -> 2000..2099 */
    uint8_t weekday;  /* 1..7 (meaning is user-defined; convention: 1 = Sunday) */
} ds1307_time_t;

/* True if the DS1307 ACKs its address on the bus. */
bool ds1307_present(void);

/* Read / write the time. Return false on I2C error / no device.
 * ds1307_set_time() clears the CH bit (starts the oscillator) and forces
 * 24-hour mode; day/month/weekday are clamped to valid ranges. */
bool ds1307_get_time(ds1307_time_t *t);
bool ds1307_set_time(const ds1307_time_t *t);

/* Boot helper. If the clock-halt bit is set (cold start / dead battery), seed
 * the clock from the build time (__DATE__/__TIME__) and start it.
 * Returns:  1 = was halted, seeded from build time
 *           0 = clock already running, left untouched
 *          -1 = I2C error / DS1307 not present                                */
int8_t ds1307_init_or_seed(void);

#endif /* DRIVERS_DS1307_H */
