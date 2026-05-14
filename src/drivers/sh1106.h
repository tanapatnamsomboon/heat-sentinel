#ifndef DRIVERS_SH1106_H
#define DRIVERS_SH1106_H

#include <stdint.h>
#include <stdbool.h>

/*
 * SH1106 128x64 monochrome OLED, I2C, page-addressed and framebuffer-less
 * (no 1 KB shadow buffer - text is written straight to the panel). Built on the
 * hal/i2c TWI master. The cursor is tracked as (x: 0..127, page: 0..7); the
 * controller is 132 columns wide, so a 2-px column offset is applied internally.
 *
 * A 5x8 font (printable ASCII 0x20..0x7E) lives in flash. Each glyph is a 6-px
 * cell (5 columns + 1 spacer), so 21 characters fit per line and 8 lines fit on
 * the panel; '\n' and right-edge wrapping move to the start of the next page.
 */

#ifndef SH1106_I2C_ADDR
#define SH1106_I2C_ADDR    0x3C    /* 7-bit; a few modules are 0x3D */
#endif
#ifndef SH1106_COL_OFFSET
#define SH1106_COL_OFFSET  2       /* 132-col controller, 128-px panel centred */
#endif
#ifndef SH1106_FLIP_180
#define SH1106_FLIP_180    0       /* set 1 if the image comes out upside-down/mirrored */
#endif

#define SH1106_WIDTH   128
#define SH1106_HEIGHT  64
#define SH1106_PAGES   (SH1106_HEIGHT / 8)    /* 8 */
#define SH1106_COLS    21                     /* characters per line (128 / 6)  */

/* Probes SH1106_I2C_ADDR, runs the init sequence, clears the screen.
 * Returns false if the panel doesn't ACK on the bus. */
bool sh1106_init(void);

void sh1106_clear(void);                       /* blank the whole panel, cursor -> (0,0) */
void sh1106_clear_line(uint8_t page);          /* blank one 8-px page row, cursor -> (0,page) */
void sh1106_set_cursor(uint8_t x, uint8_t page);   /* x: 0..127, page: 0..7 (both clamped) */

void sh1106_putc(char c);                       /* one char at the cursor; handles '\n' + wrap */
void sh1106_puts(const char *s);                /* a NUL-terminated string */
void sh1106_put_uint(uint16_t v);               /* unsigned value, decimal */
void sh1106_put_hex8(uint8_t v);                /* two hex digits, e.g. "3C" */

void sh1106_print_line(uint8_t page, const char *s);   /* clear_line(page) then puts(s) */

void sh1106_set_inverted(bool inv);             /* swap black/white */
void sh1106_set_on(bool on);                    /* panel on / off (low-power sleep) */
void sh1106_set_contrast(uint8_t contrast);     /* 0 to 255 (0x00 is dimmest, 0xFF is brightest) */

#endif /* DRIVERS_SH1106_H */
