#include "drivers/ds1307.h"
#include "hal/i2c.h"

/* DS1307 register map (timekeeping block). */
#define DS1307_REG_SECONDS  0x00   /* bit7 = CH (Clock Halt) */
#define DS1307_REG_HOURS    0x02   /* bit6 = 12/24 mode select (we use 24h) */
#define DS1307_CH_BIT       0x80
#define DS1307_12H_BIT      0x40
#define DS1307_PM_BIT       0x20   /* (only meaningful in 12-hour mode) */

static uint8_t bcd2dec(uint8_t b)
{
    return (uint8_t)((b >> 4) * 10u + (b & 0x0Fu));
}

static uint8_t dec2bcd(uint8_t d)
{
    return (uint8_t)(((d / 10u) << 4) | (d % 10u));
}

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/* Fill `t` from the firmware build time: __DATE__ = "Mmm dd yyyy" (dd is
 * space-padded), __TIME__ = "hh:mm:ss". Weekday is not derivable here -> 1. */
static void build_time(ds1307_time_t *t)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d  = __DATE__;
    const char *tm = __TIME__;

    uint8_t mon = 1;
    for (uint8_t m = 0; m < 12; m++) {
        if (d[0] == months[m * 3] && d[1] == months[m * 3 + 1] && d[2] == months[m * 3 + 2]) {
            mon = (uint8_t)(m + 1);
            break;
        }
    }

    uint8_t day  = (uint8_t)((d[4] == ' ' ? 0 : (d[4] - '0') * 10) + (d[5] - '0'));
    uint8_t year = (uint8_t)((d[9] - '0') * 10 + (d[10] - '0'));   /* last two digits */

    t->hour    = (uint8_t)((tm[0] - '0') * 10 + (tm[1] - '0'));
    t->minute  = (uint8_t)((tm[3] - '0') * 10 + (tm[4] - '0'));
    t->second  = (uint8_t)((tm[6] - '0') * 10 + (tm[7] - '0'));
    t->day     = day;
    t->month   = mon;
    t->year    = year;
    t->weekday = 1;
}

bool ds1307_present(void)
{
    return i2c_probe(DS1307_I2C_ADDR);
}

bool ds1307_get_time(ds1307_time_t *t)
{
    uint8_t r[7];
    if (!i2c_read_reg(DS1307_I2C_ADDR, DS1307_REG_SECONDS, r, 7)) {
        return false;
    }

    t->second = bcd2dec((uint8_t)(r[0] & 0x7Fu));
    t->minute = bcd2dec((uint8_t)(r[1] & 0x7Fu));

    if (r[2] & DS1307_12H_BIT) {                 /* 12-hour mode (we never set it, but be safe) */
        uint8_t h = bcd2dec((uint8_t)(r[2] & 0x1Fu));   /* 1..12 */
        if (r[2] & DS1307_PM_BIT) {
            if (h != 12u) { h = (uint8_t)(h + 12u); }
        } else if (h == 12u) {
            h = 0;
        }
        t->hour = h;
    } else {
        t->hour = bcd2dec((uint8_t)(r[2] & 0x3Fu));     /* 0..23 */
    }

    t->weekday = (uint8_t)(r[3] & 0x07u);
    t->day     = bcd2dec((uint8_t)(r[4] & 0x3Fu));
    t->month   = bcd2dec((uint8_t)(r[5] & 0x1Fu));
    t->year    = bcd2dec(r[6]);
    return true;
}

bool ds1307_set_time(const ds1307_time_t *t)
{
    uint8_t r[7];
    r[0] = dec2bcd((uint8_t)(t->second % 60u));   /* bit7 (CH) = 0 -> oscillator runs */
    r[1] = dec2bcd((uint8_t)(t->minute % 60u));
    r[2] = dec2bcd((uint8_t)(t->hour % 24u));     /* bit6 = 0 -> 24-hour mode */
    r[3] = clamp_u8(t->weekday, 1, 7);
    r[4] = dec2bcd(clamp_u8(t->day, 1, 31));
    r[5] = dec2bcd(clamp_u8(t->month, 1, 12));
    r[6] = dec2bcd((uint8_t)(t->year % 100u));
    return i2c_write_reg(DS1307_I2C_ADDR, DS1307_REG_SECONDS, r, 7);
}

int8_t ds1307_init_or_seed(void)
{
    uint8_t sec_reg;
    if (!i2c_read_reg(DS1307_I2C_ADDR, DS1307_REG_SECONDS, &sec_reg, 1)) {
        return -1;                          /* not present / I2C error */
    }
    if (!(sec_reg & DS1307_CH_BIT)) {
        return 0;                           /* oscillator running -> leave the clock alone */
    }

    ds1307_time_t t;
    build_time(&t);
    if (!ds1307_set_time(&t)) {
        return -1;
    }
    return 1;
}
