#include "hal/i2c.h"
#include "board.h"          /* F_CPU (also passed via -DF_CPU by CMake) */

#include <stddef.h>         /* NULL */
#include <avr/io.h>
#include <util/twi.h>       /* TW_STATUS, TW_START, TW_MT_SLA_ACK, ... */

/* SCL = F_CPU / (16 + 2*TWBR*prescaler); prescaler = 1 here. */
#if (F_CPU / I2C_SCL_HZ) < 16UL
#  error "I2C_SCL_HZ too high for this F_CPU"
#endif
#if (((F_CPU / I2C_SCL_HZ) - 16UL) / 2UL) > 255UL
#  error "I2C_SCL_HZ too low - TWBR would overflow (a prescaler > 1 is needed)"
#endif
#define I2C_TWBR_VALUE  ((uint8_t)(((F_CPU / I2C_SCL_HZ) - 16UL) / 2UL))

/* Spin-loop iterations to wait for TWINT before giving up. One iteration is a
 * few cycles; the default leaves a ~100x margin over a 100 kHz byte transfer. */
#ifndef I2C_TIMEOUT_LOOPS
#define I2C_TIMEOUT_LOOPS 10000U
#endif

/* Wait for the TWI hardware to finish the current operation (TWINT set).
 * Returns false on timeout. */
static bool twi_wait(void)
{
    uint16_t t = I2C_TIMEOUT_LOOPS;
    while (!(TWCR & (1 << TWINT))) {
        if (--t == 0u) {
            return false;
        }
    }
    return true;
}

void i2c_init(void)
{
    TWSR = 0;                   /* prescaler = 1 (status bits are read-only) */
    TWBR = I2C_TWBR_VALUE;
    TWCR = (1 << TWEN);         /* enable TWI; polled, so no TWIE */
}

/* START/repeated START + SLA+R/W. dir: 0 = write, 1 = read. */
static bool twi_addr(uint8_t addr7, uint8_t dir)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!twi_wait()) {
        return false;
    }
    uint8_t st = TW_STATUS;
    if (st != TW_START && st != TW_REP_START) {
        return false;
    }

    TWDR = (uint8_t)((addr7 << 1) | (dir & 1u));
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait()) {
        return false;
    }
    st = TW_STATUS;
    return (st == TW_MT_SLA_ACK) || (st == TW_MR_SLA_ACK);
}

bool i2c_start(uint8_t addr7, bool read)
{
    return twi_addr(addr7, read ? 1u : 0u);
}

bool i2c_rep_start(uint8_t addr7, bool read)
{
    return twi_addr(addr7, read ? 1u : 0u);     /* TWSTA mid-transfer = repeated START */
}

void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    /* STOP does not set TWINT; TWSTO self-clears once the bus is released. */
    uint16_t t = I2C_TIMEOUT_LOOPS;
    while ((TWCR & (1 << TWSTO)) && (--t != 0u)) {
        /* spin */
    }
}

bool i2c_write(uint8_t byte)
{
    TWDR = byte;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait()) {
        return false;
    }
    return TW_STATUS == TW_MT_DATA_ACK;
}

uint8_t i2c_read_ack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    if (!twi_wait()) {
        return 0xFFu;
    }
    return TWDR;
}

uint8_t i2c_read_nack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait()) {
        return 0xFFu;
    }
    return TWDR;
}

bool i2c_probe(uint8_t addr7)
{
    bool acked = i2c_start(addr7, false);
    i2c_stop();
    return acked;
}

uint8_t i2c_scan(uint8_t *found, uint8_t max)
{
    uint8_t count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_probe(addr)) {
            if (found != NULL && count < max) {
                found[count] = addr;
            }
            count++;
        }
    }
    return count;
}

bool i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint8_t len)
{
    if (!i2c_start(addr7, false) || !i2c_write(reg)) {
        i2c_stop();
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_write(data[i])) {
            i2c_stop();
            return false;
        }
    }
    i2c_stop();
    return true;
}

bool i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (len == 0) {
        return true;
    }
    if (!i2c_start(addr7, false) || !i2c_write(reg) || !i2c_rep_start(addr7, true)) {
        i2c_stop();
        return false;
    }
    for (uint8_t i = 0; i + 1 < len; i++) {     /* every byte but the last gets ACK */
        buf[i] = i2c_read_ack();
    }
    buf[len - 1] = i2c_read_nack();
    i2c_stop();
    return true;
}
