#include "hal/timer.h"
#include "board.h"          /* F_CPU (also passed via -DF_CPU by CMake) */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/*
 * 1 ms tick on Timer0 in CTC mode.
 *
 *   tick = (OCR0A + 1) * prescaler / F_CPU
 *
 * With prescaler = 64 and F_CPU = 8 MHz, OCR0A = 124 gives exactly 1.000 ms.
 * The value is recomputed from F_CPU below so other clocks still work; it is
 * only exact when F_CPU is a multiple of 64000.
 */
#define MILLIS_PRESCALER  64UL
#define MILLIS_OCR0A      ((F_CPU / MILLIS_PRESCALER / 1000UL) - 1UL)

#if MILLIS_OCR0A > 255UL
#  error "Timer0 1 ms tick: F_CPU too high for the /64 prescaler - use a larger one"
#endif

static volatile uint32_t s_millis;

void timer_init(void)
{
    TCCR0A = (1 << WGM01);                  /* CTC mode, TOP = OCR0A           */
    TCCR0B = 0;                             /* keep the timer stopped for now  */
    TCNT0  = 0;
    OCR0A  = (uint8_t)MILLIS_OCR0A;
    TIMSK0 = (1 << OCIE0A);                 /* enable compare-match A interrupt */
    TCCR0B = (1 << CS01) | (1 << CS00);     /* prescaler = 64 -> start counting */
}

uint32_t millis(void)
{
    uint32_t ms;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ms = s_millis;                      /* 4-byte read must not be torn */
    }
    return ms;
}

ISR(TIMER0_COMPA_vect)
{
    s_millis++;
}
