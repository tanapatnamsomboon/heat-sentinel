# Heat Sentinel — Project Log

Running record of what's done and what's next. Updated at the end of every step.

**Status legend:** ✅ done · 🔜 in progress · ⬜ not started

## Roadmap

| Step | Description | Status |
|------|-------------|--------|
| 1 | Repo skeleton: cross-platform CMake + avr-gcc toolchain file + `flash`/`size` targets, `.gitignore`, `README.md`, `PROJECT_LOG.md`, `CLAUDE.md`, MIT `LICENSE`, and a placeholder blinky (`src/main.c`) to verify the build on Windows & macOS. | ✅ |
| 2 | Core infrastructure: `board.h` (pin map, clocked from `F_CPU`), Timer0 `millis()`, GPIO helpers, LED driver, cooperative scheduler skeleton. | ✅ |
| 3 | I²C (TWI) master HAL driver + a tiny bus scanner for bring-up. | ⬜ |
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
- Committed as `Step 1: project skeleton — cross-platform CMake/avr-gcc build, flash target, docs, blinky`.

### Step 2 — Core infrastructure ✅
- `src/board.h`: the single home for the pin map (pins as `(LETTER, BIT)` pairs) and the `F_CPU` fallback. Provisional: `LED_PIN = PB1`, `DHT11_PIN = PB0`; commented optional `ESP_RST_PIN`; I²C/USART pins noted for reference.
- `src/hal/gpio.h`: zero-cost GPIO macros (`gpio_output/input/input_pullup/high/low/toggle/write/read`) that consume a board.h pin pair; set/clear/toggle compile to one instruction.
- `src/hal/timer.{c,h}`: Timer0 in CTC mode → 1 ms compare-match interrupt → `millis()` (32-bit, atomic read). `OCR0A` derived from `F_CPU` (exact 1 ms at 8 MHz); needs `sei()` after `timer_init()`.
- `src/drivers/led.{c,h}`: `led_init/on/off/toggle/set` on `LED_PIN` (glitch-free init: drive low, then set output).
- `src/app/scheduler.{c,h}`: cooperative scheduler — `sched_add(period_ms, fn)` into a fixed table (`SCHED_MAX_TASKS`, default 8), `sched_tick()` from the superloop; wraparound-safe due-time check (`period_ms == 0` ⇒ run every tick).
- `src/main.c`: replaced the raw blinky — now `led_init()` + `timer_init()` + `sei()`, registers a 500 ms heartbeat-LED task, runs `sched_tick()` in the superloop.
- `CMakeLists.txt`: added the three new `.c` files to `add_executable`, added `target_include_directories(... PRIVATE src)`.
- README repo-layout and status/limitations sections updated; CLAUDE.md "Current state" updated.
- Note: IDE flags `main.c` line ~30 "Endless loop" — that's the intended bare-metal superloop, not a bug.
- **Pending:** git commit (message in chat) + a build check once `avr-gcc` is installed on the user's machine.

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

**Step 3 — I²C (TWI) master HAL** (`hal/i2c.{c,h}`: init, start/stop, write, read-ack/nack, blocking-with-timeout) plus a small I²C bus scanner to confirm the SH1106 and DS1307 addresses on the bench. Starts after the Step 2 commit.
