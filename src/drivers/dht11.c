#include "board.h"              /* DHT11_PIN, F_CPU (needed before <util/delay.h>) */
#include "drivers/dht11.h"
#include "hal/gpio.h"
#include "hal/timer.h"          /* millis() */

#include <util/delay.h>
#include <util/atomic.h>

/* Spin-loop iterations to wait for a level transition before giving up. One
 * iteration is a handful of cycles (~1-1.5 us @ 8 MHz), so this is ~200-300 us
 * - well over the longest DHT11 phase (~90 us), short enough to bail on a stuck
 * line. */
#define DHT11_PHASE_LOOPS   200u

/* When to sample the data-bit high pulse: between a '0' (~26-28 us) and a
 * '1' (~70 us). */
#define DHT11_BIT_SAMPLE_US 40

static uint32_t s_last_ms;      /* millis() at the last dht11_read() call */

static bool dht11_wait_level(uint8_t want)
{
    uint8_t t = DHT11_PHASE_LOOPS;
    while (((uint8_t)gpio_read(DHT11_PIN)) != want) {
        if (--t == 0u) {
            return false;
        }
    }
    return true;
}

void dht11_init(void)
{
    gpio_input(DHT11_PIN);                       /* release; pull-up holds it idle-HIGH */
    s_last_ms = millis() - DHT11_MIN_INTERVAL_MS;   /* allow the first read straight away */
}

dht11_status_t dht11_read(dht11_reading_t *out)
{
    uint32_t now = millis();
    if ((uint32_t)(now - s_last_ms) < (uint32_t)DHT11_MIN_INTERVAL_MS) {
        return DHT11_BUSY;
    }
    s_last_ms = now;

    uint8_t data[5] = { 0, 0, 0, 0, 0 };
    dht11_status_t st = DHT11_OK;

    /* --- start pulse: hold the line low for >= 18 ms (interrupts on) --- */
    gpio_low(DHT11_PIN);
    gpio_output(DHT11_PIN);
    _delay_ms(20);

    /* --- response + 40 data bits: timing-critical, interrupts off --- */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        gpio_input(DHT11_PIN);                   /* release; the pull-up takes it HIGH */

        if (!dht11_wait_level(1)) {              /* line risen after release */
            st = DHT11_NO_RESPONSE;
        } else if (!dht11_wait_level(0)) {       /* DHT11 pulls low (~80 us response) */
            st = DHT11_NO_RESPONSE;
        } else if (!dht11_wait_level(1)) {       /* DHT11 releases (~80 us high) */
            st = DHT11_NO_RESPONSE;
        } else if (!dht11_wait_level(0)) {       /* first data bit's low preamble begins */
            st = DHT11_NO_RESPONSE;
        } else {
            for (uint8_t i = 0; i < 5 && st == DHT11_OK; i++) {
                for (uint8_t bit = 0; bit < 8; bit++) {
                    if (!dht11_wait_level(1)) {  /* end of the ~50 us low preamble */
                        st = DHT11_TIMEOUT;
                        break;
                    }
                    _delay_us(DHT11_BIT_SAMPLE_US);
                    if ((uint8_t)gpio_read(DHT11_PIN)) {
                        data[i] = (uint8_t)(data[i] | (uint8_t)(1u << (7 - bit)));
                        if (!dht11_wait_level(0)) {   /* wait out the rest of the high pulse */
                            st = DHT11_TIMEOUT;
                            break;
                        }
                    }
                }
            }
        }

        gpio_input(DHT11_PIN);                   /* leave the line released (idle) */
    }

    if (st != DHT11_OK) {
        return st;
    }

    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4]) {
        return DHT11_BAD_CHECKSUM;
    }

    out->humidity    = data[0];
    out->temperature = data[2];
    return DHT11_OK;
}
