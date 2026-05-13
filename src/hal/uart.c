#include "hal/uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#if (UART_RX_BUF_SIZE & (UART_RX_BUF_SIZE - 1)) != 0
#  error "UART_RX_BUF_SIZE must be a power of two"
#endif
#define UART_RX_MASK ((uint8_t)(UART_RX_BUF_SIZE - 1u))

/* Single-producer (RX ISR writes head) / single-consumer (caller writes tail)
 * ring buffer - safe without locks given byte-sized volatile head/tail. */
static volatile uint8_t s_rx_buf[UART_RX_BUF_SIZE];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;

void uart_init(uint32_t baud)
{
    /* UBRR = round(F_CPU / (16*baud)) - 1, normal-speed mode. */
    uint16_t ubrr = (uint16_t)((F_CPU + 8UL * baud) / (16UL * baud) - 1UL);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFFu);

    UCSR0A = 0;                                       /* U2X0 = 0 (normal speed)        */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);           /* 8 data bits, no parity, 1 stop */

    s_rx_head = 0;
    s_rx_tail = 0;

    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);   /* RX, TX, RX-complete IRQ */
}

void uart_putc(uint8_t c)
{
    while (!(UCSR0A & (1 << UDRE0))) {
        /* wait until the transmit data register is empty */
    }
    UDR0 = c;
}

void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc((uint8_t)*s++);
    }
}

void uart_puts_p(const char *progmem_s)
{
    uint8_t c;
    while ((c = pgm_read_byte(progmem_s++)) != 0) {
        uart_putc(c);
    }
}

void uart_write(const uint8_t *buf, uint16_t len)
{
    while (len-- != 0u) {
        uart_putc(*buf++);
    }
}

uint8_t uart_available(void)
{
    return (uint8_t)((s_rx_head - s_rx_tail) & UART_RX_MASK);
}

int uart_getc(void)
{
    if (s_rx_head == s_rx_tail) {
        return -1;
    }
    uint8_t c = s_rx_buf[s_rx_tail];
    s_rx_tail = (uint8_t)((s_rx_tail + 1u) & UART_RX_MASK);
    return (int)c;
}

void uart_flush_rx(void)
{
    s_rx_tail = s_rx_head;
}

ISR(USART_RX_vect)
{
    uint8_t c = UDR0;                                 /* reading UDR0 clears RXC0 */
    uint8_t next = (uint8_t)((s_rx_head + 1u) & UART_RX_MASK);
    if (next != s_rx_tail) {
        s_rx_buf[s_rx_head] = c;
        s_rx_head = next;
    }
    /* else: buffer full -> drop the byte */
}
