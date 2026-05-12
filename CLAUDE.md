# CLAUDE.md

Notes for AI tooling working in this repo. Keep this current as the project evolves.

## What this is

**Heat Sentinel** — bare-metal firmware for an ATmega328P @ 8 MHz that monitors
temperature/humidity (DHT11), timestamps via DS1307 RTC, displays on an SH1106
OLED, raises an RGB-LED (HW-479 module) + buzzer alert above a threshold, and
uploads telemetry over an ESP-01 WiFi module (UART AT commands). The user owns
all hardware design; this repo is firmware + project management only.

Work proceeds in **10 numbered steps** — see `PROJECT_LOG.md` for the roadmap,
status, and the rationale behind key decisions. Each step ends with a git commit
(message provided to the user, who commits manually). Update `PROJECT_LOG.md`
**and** `README.md` as part of every step.

## Toolchain & build

- **Compiler:** `avr-gcc` / `avr-libc`. **Build system:** CMake ≥ 3.20 + Ninja.
  Must build identically on Windows and macOS.
- Toolchain file `cmake/avr-gcc-toolchain.cmake` is auto-selected by
  `CMakeLists.txt` (override with `-DCMAKE_TOOLCHAIN_FILE=...`). It uses a
  `STATIC_LIBRARY` try-compile so CMake never tries to *run* an AVR binary.
- Target: `MCU=atmega328p`, `F_CPU=8000000UL` (both CMake cache vars; `F_CPU` is
  also passed as `-DF_CPU=...`). `main.c` re-defines `F_CPU` under `#ifndef` only
  as an IDE/linter fallback.
- Flags: `-std=gnu11 -Os -g -Wall -Wextra -ffunction-sections -fdata-sections`
  + AVR struct/enum packing flags; link with `-Wl,--gc-sections` and a `.map`.

### Commands

```bash
cmake --preset default            # configure (Ninja + AVR toolchain)
cmake --build --preset default    # build -> build/heat_sentinel.{elf,hex,map} + size report
cmake --build build --target size # re-print flash/RAM usage
cmake --build build --target flash # avrdude flash (configure-time cache vars below)
```

avrdude is configured via cache vars: `AVRDUDE_PROGRAMMER` (default `usbasp`),
`AVRDUDE_PORT` (default `usb`), `AVRDUDE_BAUD` (default empty). Fuse setup is
deliberately *not* part of the `flash` target — documented in `README.md`.

## Architecture (target design)

- **Bare-metal superloop**, no RTOS. **Timer0** provides a `millis()` tick; a
  small **cooperative scheduler** (table of `{period, last_run, fn}`) runs each
  job at its own cadence without blocking the others.
- Layering: `hal/` (gpio, timer, i2c, uart) → `drivers/` (led, buzzer, sh1106,
  ds1307, dht11, esp01) → `app/` (scheduler, state machine, thresholds, `main`).
- **I²C (hardware TWI):** shared by SH1106 + DS1307; blocking-with-timeout
  transactions (short, called from scheduled slots).
- **USART (hardware):** dedicated to the ESP-01 — interrupt-driven RX ring
  buffer + a **non-blocking AT state machine** (reset → join AP → TCP → HTTP GET
  → close) so a slow upload never freezes the OLED. **9600 baud** (clean timing
  at 8 MHz; module reconfigured via `AT+UART_DEF=9600,8,1,0,0`).
- **DHT11:** single-wire bit-bang, checksum-validated, retried; interrupts masked
  during the timing-critical read.
- **Alert (visual):** RGB LED module **HW-479** — three GPIOs, one per colour
  (assumed common-cathode → active-high; `board.h` toggle to flip). Digital
  on/off ⇒ 7 colours; e.g. green = normal, red blink ~2 Hz while above
  `TEMP_ALERT_C` (default 30 °C). Hardware-PWM colour mixing is a possible later
  extension.
- **Alert (audible):** buzzer module on its own GPIO — short beep on entering the
  alert state (optional periodic chirp while in alert). Active vs passive buzzer
  handling per the hardware.
- **RTC:** on boot, if the DS1307 clock-halt (CH) bit is set, seed from build
  time (`__DATE__`/`__TIME__`); `rtc_set_time()` available.
- **Watchdog:** ~2 s, kicked from the loop (added in Step 10).

## Conventions

- C only, `gnu11`. Lower_snake_case for functions/vars; `UPPER_SNAKE` macros;
  module-prefixed names (`gpio_`, `timer_`, `i2c_`, `led_`, `buzzer_`, `sh1106_`,
  `ds1307_`, `dht11_`, `esp01_`, `sched_`). One module = `.c` + `.h`.
- **No monolithic `config.h`.** Pin map and clock-derived constants in `board.h`;
  per-driver tunables in the driver's header; secrets (WiFi SSID/pass, telemetry
  host/path) in an **untracked `app_config.h`** generated from a committed
  `app_config.h.example`. `app_config.h` is git-ignored.
- New `.c` files must be added to `add_executable(...)` in `CMakeLists.txt`
  (no globbing).
- Keep `README.md` (user-facing: setup/build/flash, current state) and
  `PROJECT_LOG.md` (step status + decisions) in sync with reality every step.
- Each numbered step ends with a **Conventional Commits**-style message
  (`feat:` / `fix:` / `docs:` / `refactor:` / `chore:` …) handed to the user,
  plus a short **validation method** (build check + the on-hardware behaviour to
  look for) recorded under that step in `PROJECT_LOG.md`.

## Current state

Step 7 complete. In place:
- `board.h` — pin map as `(LETTER, BIT)` pairs + `F_CPU` fallback. DHT11 `PB0`; RGB LED `PB1/PB2/PB3` (`LED_RGB_ACTIVE_HIGH=1`, HW-479 common-cathode); buzzer `PD3`/OC2B (`BUZZER_ACTIVE_HIGH=1`); optional `ESP_RST_PIN` commented.
- `hal/gpio.h` — zero-cost GPIO macros consuming a board.h pin pair.
- `hal/timer.{c,h}` — Timer0 CTC → 1 ms `millis()` (atomic 32-bit read); needs `sei()` after `timer_init()`.
- `hal/i2c.{c,h}` — TWI master, blocking with timeout; 7-bit addresses; primitives (`i2c_start/rep_start/stop/write/read_ack/read_nack`) + helpers (`i2c_probe`, `i2c_scan`, `i2c_write_reg`, `i2c_read_reg`). 100 kHz default (DS1307 limit); external SDA/SCL pull-ups are the user's hardware.
- `drivers/led.{c,h}` — RGB LED (HW-479): `led_color_t` enum, `led_init/set/get/off/toggle(color)`; channel polarity from `LED_RGB_ACTIVE_HIGH`. Digital on/off per channel (8 states).
- `drivers/buzzer.{c,h}` — `buzzer_init/on/off`, non-blocking `buzzer_beep(ms)` (active buzzer) and `buzzer_tone(freq_hz, ms)` (passive buzzer; Timer2 CTC toggling OC2B/PD3, used only while sounding), `buzzer_tick()` (call from loop), `buzzer_is_sounding()`. Polarity from `BUZZER_ACTIVE_HIGH`.
- `drivers/sh1106.{c,h}` — SH1106 128×64 OLED over `hal/i2c`, **framebuffer-less** (text written straight to the panel). `sh1106_init()` (probes `SH1106_I2C_ADDR`=`0x3C`, runs init seq, clears — returns `false` if absent), `sh1106_clear/clear_line/set_cursor` (cursor `(x:0..127, page:0..7)`, 2-px `SH1106_COL_OFFSET` applied internally), `sh1106_putc/puts/put_uint/put_hex8/print_line` (5×8 PROGMEM font, 6-px cell ⇒ 21 chars/line, `'\n'` + wrap), `sh1106_set_inverted/set_on`. `SH1106_FLIP_180` toggles orientation.
- `drivers/ds1307.{c,h}` — DS1307 RTC over `hal/i2c` helpers. `ds1307_time_t` (sec/min/hour 24h, day/month/year-as-yy, weekday). `ds1307_present`, `ds1307_get_time`/`ds1307_set_time` (set clears CH ⇒ starts oscillator, 24h mode, clamps day/month/weekday), `ds1307_init_or_seed()` (boot: if CH set, seed from `__DATE__`/`__TIME__`; returns 1/0/-1). BCD↔dec internal.
- `drivers/dht11.{c,h}` — DHT11 on `DHT11_PIN` (PB0). `dht11_status_t`, `dht11_reading_t { humidity, temperature }` (8-bit). `dht11_init()`, `dht11_read(out)` — enforces `DHT11_MIN_INTERVAL_MS` (2 s) returning `DHT11_BUSY`; 20 ms start pulse (IRQs on) then `ATOMIC_BLOCK` (IRQs off, ~5 ms) for the response + 40 bits; checksum-validated; phase waits bounded by `DHT11_PHASE_LOOPS`. Single attempt per call — the scheduler re-reads every 2 s.
- `app/scheduler.{c,h}` — fixed-table cooperative scheduler: `sched_add(period_ms, fn)` + `sched_tick()`, wraparound-safe.
- `main.c` — superloop: inits, boot-time `i2c_scan()`, `sh1106_init()`, `ds1307_init_or_seed()`, `dht11_init()`, boot chirp; prints the scan result to the OLED; a 1 Hz `rtc_task` shows the date/time and a 2 s `dht_task` shows temp/humidity. RGB colour mirrors OLED status (green/yellow/red); calls `sched_tick()` + `buzzer_tick()`. Still a stand-in for the real app logic.

Timer usage so far: **Timer0** = `millis()`; **Timer2** = buzzer tone (only while sounding); **Timer1** = free.

Sources live under `src/` with `src/` on the include path; every new `.c` must be
added to `add_executable(...)` in `CMakeLists.txt` (no globbing).

Next: Step 8 — `hal/uart.{c,h}` (interrupt-driven RX ring buffer @ 9600 baud)
+ `drivers/esp01.{c,h}` (non-blocking AT state machine: reset → join AP → TCP →
HTTP GET → close), ticked from the superloop. Untracked `app_config.h`
(SSID/pass, telemetry host/path) from a committed `.example`. `main.c` shows
WiFi state on the OLED + uploads readings. May split 8a (UART) / 8b (ESP-01).

## Reference: old code from another project

The user shared SH1106 / DHT11 / I²C / DS1307 / config / Makefile snippets from a
previous project for context only. They explicitly said **not** to follow them —
treat as background, not a template. (E.g. the old monolithic `config.h` is
intentionally being replaced by the `board.h` + per-driver + `app_config.h` split.)
