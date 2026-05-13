#ifndef DRIVERS_ESP01_H
#define DRIVERS_ESP01_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Non-blocking ESP-01 AT command state machine.
 * * Drives the ESP-01 through reset -> CWMODE -> CWJAP (join AP) -> IDLE.
 * When esp01_post() is called, it transitions through CIPSTART -> CIPSEND ->
 * sending the HTTP GET -> CIPCLOSE, and back to IDLE.
 */

void esp01_init(void);

/* Call this frequently from the superloop to process RX and step the state machine */
void esp01_tick(void);

/* Queue telemetry data for upload. Ignored if not in IDLE state. */
void esp01_post(uint8_t temp, uint8_t humidity);

/* Returns a short string representing the current state (useful for the OLED) */
const char* esp01_get_state_str(void);

#endif /* DRIVERS_ESP01_H */