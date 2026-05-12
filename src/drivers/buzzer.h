#ifndef DRIVERS_BUZZER_H
#define DRIVERS_BUZZER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Buzzer on BUZZER_PIN (see board.h). Two ways to drive it, so both module
 * types are covered:
 *
 *   - buzzer_on/off, buzzer_beep(ms)   : an *active* buzzer (built-in oscillator
 *                                        - just needs its pin held active).
 *   - buzzer_tone(freq_hz, ms)         : a *passive* buzzer - generates a square
 *                                        wave with Timer2 on OC2B. Requires
 *                                        BUZZER_PIN to be PD3 (OC2B); Timer2 is
 *                                        used only while a tone is sounding.
 *
 * All the timed calls are non-blocking. Call buzzer_tick() frequently from the
 * superloop so the beep/tone is switched off once its duration elapses.
 *
 * Polarity follows BUZZER_ACTIVE_HIGH - many MH-FMD-style modules are active-LOW.
 */

void buzzer_init(void);
void buzzer_on(void);                              /* hold active level (active buzzer)        */
void buzzer_off(void);                             /* hard off: stop any tone, inactive level  */
void buzzer_beep(uint16_t ms);                     /* active-buzzer beep for `ms`, non-blocking*/
void buzzer_tone(uint16_t freq_hz, uint16_t ms);   /* passive-buzzer tone for `ms`, non-block. */
void buzzer_tick(void);                            /* call from the loop: ends a due beep/tone */
bool buzzer_is_sounding(void);

#endif /* DRIVERS_BUZZER_H */
