#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <avr/io.h>

/*
 * Zero-cost GPIO helpers. A pin is described in board.h as a (LETTER, BIT) pair:
 *
 *     #define LED_PIN  B, 1            // PB1
 *     ...
 *     gpio_output(LED_PIN);            // DDRB  |=  (1 << 1)
 *     gpio_high(LED_PIN);              // PORTB |=  (1 << 1)
 *     if (gpio_read(BTN_PIN)) { ... }  // (PINx >> bit) & 1
 *
 * Set / clear / toggle compile to a single instruction. The two-level macro
 * indirection lets the (LETTER, BIT) pair from board.h expand into two macro
 * arguments before the register names are pasted together.
 */

#define GPIO_CAT_(a, b)    a ## b
#define GPIO_DDR_(letter)  GPIO_CAT_(DDR,  letter)
#define GPIO_PORT_(letter) GPIO_CAT_(PORT, letter)
#define GPIO_PIN_(letter)  GPIO_CAT_(PIN,  letter)

#define gpio_output(pin)         gpio_output_(pin)
#define gpio_input(pin)          gpio_input_(pin)
#define gpio_input_pullup(pin)   gpio_input_pullup_(pin)
#define gpio_high(pin)           gpio_high_(pin)
#define gpio_low(pin)            gpio_low_(pin)
#define gpio_toggle(pin)         gpio_toggle_(pin)
#define gpio_write(pin, val)     gpio_write_(pin, val)
#define gpio_read(pin)           gpio_read_(pin)

#define gpio_output_(L, B)       do { GPIO_DDR_(L)  |=  (1u << (B)); } while (0)
#define gpio_input_(L, B)        do { GPIO_DDR_(L)  &= ~(1u << (B)); GPIO_PORT_(L) &= ~(1u << (B)); } while (0)
#define gpio_input_pullup_(L, B) do { GPIO_DDR_(L)  &= ~(1u << (B)); GPIO_PORT_(L) |=  (1u << (B)); } while (0)
#define gpio_high_(L, B)         do { GPIO_PORT_(L) |=  (1u << (B)); } while (0)
#define gpio_low_(L, B)          do { GPIO_PORT_(L) &= ~(1u << (B)); } while (0)
#define gpio_toggle_(L, B)       do { GPIO_PORT_(L) ^=  (1u << (B)); } while (0)
#define gpio_write_(L, B, v)     do { if (v) { GPIO_PORT_(L) |= (1u << (B)); } else { GPIO_PORT_(L) &= ~(1u << (B)); } } while (0)
#define gpio_read_(L, B)         ((GPIO_PIN_(L) >> (B)) & 1u)

#endif /* HAL_GPIO_H */
