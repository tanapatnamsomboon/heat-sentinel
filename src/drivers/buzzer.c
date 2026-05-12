#include "drivers/buzzer.h"
#include "board.h"
#include "hal/gpio.h"
#include "hal/timer.h"      /* millis() */

#include <avr/io.h>

#if BUZZER_ACTIVE_HIGH
#  define BUZZER_DRIVE_ON_()   gpio_high(BUZZER_PIN)
#  define BUZZER_DRIVE_OFF_()  gpio_low(BUZZER_PIN)
#else
#  define BUZZER_DRIVE_ON_()   gpio_low(BUZZER_PIN)
#  define BUZZER_DRIVE_OFF_()  gpio_high(BUZZER_PIN)
#endif

static bool     s_sounding;     /* a beep/tone is currently active                  */
static bool     s_timed;        /* ...and it has an off-deadline (vs. buzzer_on())  */
static bool     s_tone_mode;    /* Timer2 is driving OC2B (vs. plain GPIO level)    */
static uint32_t s_off_at_ms;    /* millis() value at which to stop, when s_timed    */

/* Disconnect OC2B from Timer2 and stop the timer, returning PD3 to GPIO control. */
static void tone_stop(void)
{
    TCCR2A = 0;
    TCCR2B = 0;
    s_tone_mode = false;
}

void buzzer_init(void)
{
    tone_stop();
    BUZZER_DRIVE_OFF_();
    gpio_output(BUZZER_PIN);
    s_sounding  = false;
    s_timed     = false;
    s_off_at_ms = 0;
}

void buzzer_on(void)
{
    if (s_tone_mode) {
        tone_stop();
    }
    BUZZER_DRIVE_ON_();
    s_sounding = true;
    s_timed    = false;
}

void buzzer_off(void)
{
    if (s_tone_mode) {
        tone_stop();
    }
    BUZZER_DRIVE_OFF_();
    s_sounding = false;
    s_timed    = false;
}

void buzzer_beep(uint16_t ms)
{
    buzzer_on();
    s_timed     = true;
    s_off_at_ms = millis() + ms;
}

void buzzer_tone(uint16_t freq_hz, uint16_t ms)
{
    if (freq_hz == 0u) {
        buzzer_off();
        return;
    }

    /* Square wave on OC2B (PD3): Timer2 in CTC mode, TOP = OCR2A, OC2B toggled
     * on compare-match B (with OCR2B == OCR2A => 50% duty).
     *   f_out = F_CPU / (2 * prescaler * (OCR2A + 1))
     * Pick the smallest Timer2 prescaler that keeps OCR2A within 0..255. */
    static const struct { uint8_t cs; uint16_t div; } presc[] = {
        { 1u, 1u }, { 2u, 8u }, { 3u, 32u }, { 4u, 64u },
        { 5u, 128u }, { 6u, 256u }, { 7u, 1024u },
    };
    uint8_t  cs  = 7u;
    uint16_t ocr = 255u;
    for (uint8_t i = 0; i < (uint8_t)(sizeof presc / sizeof presc[0]); i++) {
        uint32_t div = (uint32_t)F_CPU / (2UL * (uint32_t)presc[i].div * (uint32_t)freq_hz);
        if (div >= 1UL && div <= 256UL) {
            cs  = presc[i].cs;
            ocr = (uint16_t)(div - 1UL);
            break;
        }
    }

    if (!s_tone_mode) {
        gpio_output(BUZZER_PIN);    /* OC2B needs the pin's output driver enabled */
    }
    TCCR2A = (1 << COM2B0) | (1 << WGM21);  /* toggle OC2B on match, CTC (TOP=OCR2A) */
    OCR2A  = (uint8_t)ocr;
    OCR2B  = (uint8_t)ocr;
    TCNT2  = 0;
    TCCR2B = cs;                            /* set prescaler => timer starts */

    s_tone_mode = true;
    s_sounding  = true;
    s_timed     = true;
    s_off_at_ms = millis() + ms;
}

void buzzer_tick(void)
{
    if (s_timed && (int32_t)(millis() - s_off_at_ms) >= 0) {
        buzzer_off();
    }
}

bool buzzer_is_sounding(void)
{
    return s_sounding;
}
