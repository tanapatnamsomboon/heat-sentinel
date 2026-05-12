#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

/*
 * Timer0-based millisecond clock.
 *
 * timer_init() configures Timer0 for a 1 ms compare-match interrupt; call it
 * once at startup, then enable global interrupts with sei(). Timer0's PWM
 * outputs (OC0A / OC0B) are consumed by this and must not be used elsewhere.
 */
void timer_init(void);

/* Milliseconds elapsed since timer_init(). Wraps roughly every 49.7 days. */
uint32_t millis(void);

#endif /* HAL_TIMER_H */
