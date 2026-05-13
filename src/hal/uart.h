#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

/*
 * USART0 driver: 8-N-1, interrupt-driven RX into a ring buffer, blocking TX.
 * Dedicated to the ESP-01 (RXD = PD0, TXD = PD1). Call uart_init() once, then
 * enable global interrupts (sei()). TX waits for the data register between
 * bytes; AT commands are short, so the blocking is brief.
 *
 * The RX ring buffer is UART_RX_BUF_SIZE bytes (power of two); if it overflows
 * the new byte is dropped (the ESP-01 AT layer scans for "OK"/"ERROR" tokens,
 * so an occasional dropped byte is tolerable).
 */

#ifndef UART_RX_BUF_SIZE
#define UART_RX_BUF_SIZE 64        /* must be a power of two */
#endif

void    uart_init(uint32_t baud);
void    uart_putc(uint8_t c);                       /* blocking */
void    uart_puts(const char *s);                   /* NUL-terminated string from RAM   */
void    uart_puts_p(const char *progmem_s);         /* ... from flash (use with PSTR()) */
void    uart_write(const uint8_t *buf, uint16_t len);

uint8_t uart_available(void);                        /* bytes waiting in the RX buffer */
int     uart_getc(void);                             /* next RX byte (0..255), or -1 if none */
void    uart_flush_rx(void);                         /* discard any pending RX data */

#endif /* HAL_UART_H */
