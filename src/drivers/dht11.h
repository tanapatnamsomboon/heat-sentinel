#ifndef DRIVERS_DHT11_H
#define DRIVERS_DHT11_H

#include <stdint.h>
#include <stdbool.h>

/*
 * DHT11 temperature / humidity sensor on the single GPIO defined as DHT11_PIN
 * in board.h. The data line needs an external pull-up (part of the user's
 * hardware - it's on the usual DHT11 breakout). The read is a bit-banged,
 * timing-critical exchange:
 *
 *   MCU pulls the line low >= 18 ms, releases -> DHT11 answers with an 80 us
 *   low + 80 us high, then 40 data bits (each: ~50 us low, then a high whose
 *   length encodes the bit: ~26 us = '0', ~70 us = '1'), 5 bytes total:
 *   [RH int][RH frac][T int][T frac][checksum]. DHT11 has no fractional part,
 *   so RH = byte0, T = byte2; checksum = (byte0+byte1+byte2+byte3) & 0xFF.
 *
 * Interrupts are masked during the ~5 ms of bit timing (Timer0's millis() loses
 * a few ms per read - immaterial; the DS1307 is the real clock). The DHT11
 * needs >= 1 s between reads; dht11_read() enforces DHT11_MIN_INTERVAL_MS and
 * returns DHT11_BUSY if polled sooner.
 */

typedef enum {
    DHT11_OK = 0,
    DHT11_BUSY,           /* called again before DHT11_MIN_INTERVAL_MS elapsed */
    DHT11_NO_RESPONSE,    /* the sensor never answered the start pulse */
    DHT11_TIMEOUT,        /* a bit-timing phase ran too long (line stuck?) */
    DHT11_BAD_CHECKSUM,   /* 40 bits read but the checksum didn't match */
} dht11_status_t;

typedef struct {
    uint8_t humidity;     /* % RH (integer; DHT11 has no fractional part) */
    uint8_t temperature;  /* deg C (integer) */
} dht11_reading_t;

#ifndef DHT11_MIN_INTERVAL_MS
#define DHT11_MIN_INTERVAL_MS  2000   /* datasheet: >= 1 s between reads; we use 2 s */
#endif

void           dht11_init(void);                  /* park the line idle (released) */
dht11_status_t dht11_read(dht11_reading_t *out);  /* one read; respects the min interval */

#endif /* DRIVERS_DHT11_H */
