# Heat Sentinel — Project Log

Running record of what's done and what's next. Updated at the end of every step.

**Status legend:** ✅ done · 🔜 in progress · ⬜ not started

## Roadmap

| Step | Description | Status |
|------|-------------|--------|
| 1 | Repo skeleton: cross-platform CMake + avr-gcc toolchain file + `flash`/`size` targets, `.gitignore`, `README.md`, `PROJECT_LOG.md`, `CLAUDE.md`, MIT `LICENSE`, and a placeholder blinky (`src/main.c`) to verify the build on Windows & macOS. | ✅ |
| 2 | Core infrastructure: `board.h` (pin map, clocked from `F_CPU`), Timer0 `millis()`, GPIO helpers, LED driver, cooperative scheduler skeleton. | ✅ |
| 3 | I²C (TWI) master HAL driver + a tiny bus scanner for bring-up. | ✅ |
| 4 | SH1106 128×64 OLED driver: init (with the 2 px column offset), `set_cursor`, 5×8 ASCII text, `clear`. | ⬜ |
| 5 | DS1307 RTC driver: BCD conversion, get/set time, auto-seed from build time when the clock-halt bit is set; show time on the OLED. | ⬜ |
| 6 | DHT11 driver: single-wire read, checksum validation, retry policy, interrupts masked during the timing-critical read. | ⬜ |
| 7 | USART driver (interrupt-driven RX ring buffer) + ESP-01 non-blocking AT state machine (reset → join AP → TCP → HTTP GET → close). May split into 7a/7b. | ⬜ |
| 8 | Application integration: state machine — sample → OLED with timestamp → threshold check → LED alert → telemetry upload; graceful degradation when WiFi/ESP is unavailable. | ⬜ |
| 9 | Polish: `app_config.h(.example)` for thresholds & WiFi creds, watchdog timer, error/status surfaced on the OLED, README/log finalization. | ⬜ |

## Step log

### Step 1 — Project setup & CMake ✅
- Cross-platform CMake build (`CMakeLists.txt`) targeting `atmega328p`, `F_CPU=8000000UL`.
- `cmake/avr-gcc-toolchain.cmake` — auto-selected; `STATIC_LIBRARY` try-compile so CMake never runs AVR binaries.
- `CMakePresets.json` — `default` preset (Ninja + toolchain file).
- Post-build: `.hex` via `avr-objcopy`, flash/RAM usage via `avr-size`; extra `flash` (avrdude, configurable via cache vars) and `size` targets.
- `.gitignore` (build dirs, `.idea/`, `.claude/settings.local.json`, generated `app_config.h`, OS cruft), MIT `LICENSE`, `README.md` with Windows/macOS setup + build + flash + fuse notes, this log, and `CLAUDE.md`.
- `src/main.c`: placeholder blinky on `PB1` to validate the toolchain on both OSes.
- **Validation:** `cmake --preset default && cmake --build --preset default` produces `build/heat_sentinel.{elf,hex}` + an `avr-size` report (blinky ≲ 200 B flash); flash it and the LED on `PB1` blinks at ~1 Hz.
- Committed: `feat: project skeleton — cross-platform CMake/avr-gcc build, flash target, docs, blinky` (`d22c4ef`).

### Step 2 — Core infrastructure ✅
- `src/board.h`: the single home for the pin map (pins as `(LETTER, BIT)` pairs) and the `F_CPU` fallback. Provisional: `LED_PIN = PB1`, `DHT11_PIN = PB0`; commented optional `ESP_RST_PIN`; I²C/USART pins noted for reference.
- `src/hal/gpio.h`: zero-cost GPIO macros (`gpio_output/input/input_pullup/high/low/toggle/write/read`) that consume a board.h pin pair; set/clear/toggle compile to one instruction.
- `src/hal/timer.{c,h}`: Timer0 in CTC mode → 1 ms compare-match interrupt → `millis()` (32-bit, atomic read). `OCR0A` derived from `F_CPU` (exact 1 ms at 8 MHz); needs `sei()` after `timer_init()`.
- `src/drivers/led.{c,h}`: `led_init/on/off/toggle/set` on `LED_PIN` (glitch-free init: drive low, then set output).
- `src/app/scheduler.{c,h}`: cooperative scheduler — `sched_add(period_ms, fn)` into a fixed table (`SCHED_MAX_TASKS`, default 8), `sched_tick()` from the superloop; wraparound-safe due-time check (`period_ms == 0` ⇒ run every tick).
- `src/main.c`: replaced the raw blinky — now `led_init()` + `timer_init()` + `sei()`, registers a 500 ms heartbeat-LED task, runs `sched_tick()` in the superloop.
- `CMakeLists.txt`: added the three new `.c` files to `add_executable`, added `target_include_directories(... PRIVATE src)`.
- `CMakeLists.txt`: added the three new `.c` files to `add_executable`, added `target_include_directories(... PRIVATE src)`.
- README repo-layout and status/limitations sections updated; CLAUDE.md "Current state" updated.
- Note: IDE flags `main.c` "Endless loop" on the `for (;;)` — that's the intended bare-metal superloop, not a bug.
- **Validation:** builds clean (`-Wall -Wextra`); `avr-size` grows by only tens of bytes vs Step 1; on hardware the `PB1` LED toggles every 500 ms (≈1 Hz) — confirms Timer0 `millis()` + scheduler. Scope the LED pin if you want to verify the ~2 ms timer-ISR cadence indirectly via the steady 500 ms period.
- Committed: `feat: board.h, Timer0 millis(), GPIO macros, LED driver, cooperative scheduler` (`a4bb7ee`).

### Step 3 — I²C (TWI) master HAL ✅
- `src/hal/i2c.{c,h}`: hardware-TWI master, blocking with a timeout on every wait (`I2C_TIMEOUT_LOOPS`, ~100× margin over a 100 kHz byte) so a stuck bus can't hang the firmware.
  - Bus clock from `I2C_SCL_HZ` (default **100 kHz** — the DS1307 limit); `TWBR` computed from `F_CPU` with `#error` guards if it can't be represented at prescaler 1 (= 32 for 8 MHz/100 kHz).
  - Low-level primitives: `i2c_init`, `i2c_start`/`i2c_rep_start(addr7, read)` (address byte + ACK check, using `<util/twi.h>` status codes), `i2c_stop` (waits out `TWSTO`), `i2c_write`, `i2c_read_ack`/`i2c_read_nack` (return `0xFF` on timeout). 7-bit addresses; R/W bit added internally. Caller issues `i2c_stop()` after a failed primitive.
  - Helpers: `i2c_probe(addr7)`, `i2c_scan(found, max)` (probes 0x08..0x77, returns count), `i2c_write_reg`/`i2c_read_reg` (the "register pointer then payload" pattern for the DS1307) — these `i2c_stop()` on every path.
  - Internal pull-ups deliberately left off — SDA (PC4)/SCL (PC5) pull-ups are the user's hardware. Bus-recovery (9 SCL pulses on a stuck SDA) noted as a possible future addition.
- `src/main.c`: now `i2c_init()` + a boot-time `i2c_scan()`; the alert-LED blink rate encodes the result — **slow ~1 Hz** if ≥1 device ACKed, **fast ~3 Hz** if none. (Proper on-screen device list arrives with the OLED in Step 4.)
- `CMakeLists.txt`: added `src/hal/i2c.c`.
- README + CLAUDE.md updated (status, repo layout, notes).
- **Validation:**
  1. `cmake --build --preset default` compiles clean under `-Wall -Wextra`; `avr-size` grows by a few hundred bytes.
  2. *(static)* `TWBR` should be 32 for F_CPU=8 MHz, SCL=100 kHz (`grep TWBR build/heat_sentinel.lst` if you generate a listing, or just trust the formula).
  3. *(hardware)* With proper pull-ups and at least one of the SH1106 / DS1307 connected → power up → LED does the **slow** ~1 Hz blink. Remove all I²C devices (or the pull-ups) → LED switches to the **fast** ~3 Hz blink.
  4. *(optional, deeper)* Logic analyzer / scope on SDA & SCL: see START, `addr<<1` byte + ACK, STOP repeating at ~100 kHz during the boot scan.

## Key decisions

- **Language:** C, `-std=gnu11`, `-Os -g -Wall -Wextra`, sections GC'd.
- **Build:** CMake ≥ 3.20 + Ninja; toolchain file auto-applied. avrdude `flash` target with `AVRDUDE_PROGRAMMER` / `AVRDUDE_PORT` / `AVRDUDE_BAUD` cache vars.
- **Architecture:** bare-metal superloop + Timer0 `millis()` + a small cooperative scheduler (period/last-run/fn table). No RTOS.
- **ESP-01:** interrupt-driven USART RX ring buffer + non-blocking AT state machine, **9600 baud** (clean timing at 8 MHz; reconfigure the module with `AT+UART_DEF=9600,8,1,0,0`).
- **Debug:** none on UART (it's the ESP-01's); status/errors shown on the OLED. Optional compile-time `DEBUG_UART` hook for later.
- **Config layout:** no monolithic `config.h`; `board.h` (pins), per-driver headers, untracked `app_config.h` (secrets) from a committed `.example`.
- **RTC:** on boot, if the DS1307 clock-halt (CH) bit is set, seed time from build-time `__DATE__`/`__TIME__`; `rtc_set_time()` also available.
- **LED alert:** blinks ~2 Hz while above `TEMP_ALERT_C` (default 30 °C), off otherwise.
- **Watchdog:** added in Step 9 (~2 s, kicked from the loop).
- **Provisional pins** (in `board.h`, user owns wiring): DHT11 → `PB0`, LED → `PB1`, I²C → `PC4/PC5` (fixed), UART → `PD0/PD1` (fixed), optional `ESP_RST` left commented (assumes CH_PD tied high).
- **Host unit tests:** skipped for now (firmware-only); may add later for BCD/checksum/parse logic.
- **Branch:** `master`. **License:** MIT.

## Next up

**Step 4 — SH1106 128×64 OLED driver** (`drivers/sh1106.{c,h}`): init sequence, the 2-px column offset, page/column addressing, `clear`, `set_cursor`, 5×8 ASCII text (`print_char`/`print_string` with auto-wrap). Built on the I²C HAL; `main.c` will show the I²C scan result and a banner on screen. Starts after the Step 3 commit.
