# CLAUDE.md

Notes for AI tooling working in this repo. Keep this current as the project evolves.

## What this is

**Heat Sentinel** — bare-metal firmware for an ATmega328P @ 8 MHz that monitors
temperature/humidity (DHT11), timestamps via DS1307 RTC, displays on an SH1106
OLED, drives a GPIO LED alert above a threshold, and uploads telemetry over an
ESP-01 WiFi module (UART AT commands). The user owns all hardware design; this
repo is firmware + project management only.

Work proceeds in **9 numbered steps** — see `PROJECT_LOG.md` for the roadmap,
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
- Layering: `hal/` (gpio, timer, i2c, uart) → `drivers/` (sh1106, ds1307, dht11,
  esp01) → `app/` (scheduler, state machine, thresholds, `main`).
- **I²C (hardware TWI):** shared by SH1106 + DS1307; blocking-with-timeout
  transactions (short, called from scheduled slots).
- **USART (hardware):** dedicated to the ESP-01 — interrupt-driven RX ring
  buffer + a **non-blocking AT state machine** (reset → join AP → TCP → HTTP GET
  → close) so a slow upload never freezes the OLED. **9600 baud** (clean timing
  at 8 MHz; module reconfigured via `AT+UART_DEF=9600,8,1,0,0`).
- **DHT11:** single-wire bit-bang, checksum-validated, retried; interrupts masked
  during the timing-critical read.
- **LED alert:** GPIO, blinks ~2 Hz while above `TEMP_ALERT_C` (default 30 °C).
- **RTC:** on boot, if the DS1307 clock-halt (CH) bit is set, seed from build
  time (`__DATE__`/`__TIME__`); `rtc_set_time()` available.
- **Watchdog:** ~2 s, kicked from the loop (added in Step 9).

## Conventions

- C only, `gnu11`. Lower_snake_case for functions/vars; `UPPER_SNAKE` macros;
  module-prefixed names (`i2c_`, `sh1106_`, `ds1307_`, `dht11_`, `esp01_`,
  `sched_`). One module = `.c` + `.h`.
- **No monolithic `config.h`.** Pin map and clock-derived constants in `board.h`;
  per-driver tunables in the driver's header; secrets (WiFi SSID/pass, telemetry
  host/path) in an **untracked `app_config.h`** generated from a committed
  `app_config.h.example`. `app_config.h` is git-ignored.
- New `.c` files must be added to `add_executable(...)` in `CMakeLists.txt`
  (no globbing).
- Keep `README.md` (user-facing: setup/build/flash, current state) and
  `PROJECT_LOG.md` (step status + decisions) in sync with reality every step.

## Current state

Step 1 complete (build skeleton + blinky placeholder on `PB1`). Next: Step 2 —
`board.h`, Timer0 `millis()`, GPIO helpers, LED driver, cooperative scheduler.
The blinky `main.c` is throwaway and will be replaced.

## Reference: old code from another project

The user shared SH1106 / DHT11 / I²C / DS1307 / config / Makefile snippets from a
previous project for context only. They explicitly said **not** to follow them —
treat as background, not a template. (E.g. the old monolithic `config.h` is
intentionally being replaced by the `board.h` + per-driver + `app_config.h` split.)
