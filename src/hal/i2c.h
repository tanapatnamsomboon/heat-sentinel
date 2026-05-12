#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Hardware-TWI (I2C) master - blocking, with a timeout on every wait so a stuck
 * bus can never hang the firmware. Shared by the SH1106 OLED and the DS1307 RTC.
 *
 * Addresses are 7-bit; the R/W direction bit is added internally. On any failure
 * (NACK, arbitration loss, timeout) the low-level calls return false/0xFF and
 * the *caller* is responsible for issuing i2c_stop() to release the bus - the
 * convenience helpers (i2c_probe / i2c_write_reg / i2c_read_reg) do that already.
 *
 * Bus clock = I2C_SCL_HZ (default 100 kHz - the DS1307's maximum). External
 * pull-ups on SDA (PC4) / SCL (PC5) are part of the user's hardware; this driver
 * does not enable the AVR's internal pull-ups.
 */

#ifndef I2C_SCL_HZ
#define I2C_SCL_HZ 100000UL
#endif

void    i2c_init(void);

/* --- low-level primitives -------------------------------------------------- */
/* START (or repeated START) followed by the address byte. Returns true if the
 * addressed device ACKed. On false, call i2c_stop() before retrying. */
bool    i2c_start(uint8_t addr7, bool read);
bool    i2c_rep_start(uint8_t addr7, bool read);
void    i2c_stop(void);

bool    i2c_write(uint8_t byte);    /* true if the slave ACKed the byte        */
uint8_t i2c_read_ack(void);         /* read a byte, reply ACK (more to follow) */
uint8_t i2c_read_nack(void);        /* read a byte, reply NACK (last byte)     */

/* --- convenience helpers --------------------------------------------------- */
/* True if a device ACKs its address (issues START + addr + STOP). */
bool    i2c_probe(uint8_t addr7);

/* Probe every valid 7-bit address (0x08..0x77); store up to `max` of them in
 * `found` and return how many devices answered. */
uint8_t i2c_scan(uint8_t *found, uint8_t max);

/* "register pointer, then payload" transfers (the DS1307 access pattern). */
bool    i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint8_t len);
bool    i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len);

#endif /* HAL_I2C_H */
