# Heat Sentinel — Project Log

Running record of what's done and what's next. Updated at the end of every step.

**Status legend:** ✅ done · 🔜 in progress · ⬜ not started

## Roadmap

| Step | Description | Status |
|------|-------------|--------|
| 1 | Repo skeleton: cross-platform CMake + avr-gcc toolchain file + `flash`/`size` targets, `.gitignore`, `README.md`, `PROJECT_LOG.md`, `CLAUDE.md`, MIT `LICENSE`, and a placeholder blinky (`src/main.c`) to verify the build on Windows & macOS. | ✅ |
| 2 | Core infrastructure: `board.h` (pin map, clocked from `F_CPU`), Timer0 `millis()`, GPIO helpers, LED driver, cooperative scheduler skeleton. | ✅ |
| 3 | I²C (TWI) master HAL driver + a tiny bus scanner for bring-up. | ✅ |
| 4 | Output devices: rework `drivers/led` for the **RGB LED module (HW-479)** — 3 channels / named colours — and add `drivers/buzzer`; `main.c` switches the boot-scan indicator to status colours and adds a startup beep. | ✅ |
| 5 | SH1106 128×64 OLED driver: init (with the 2 px column offset), `set_cursor`, 5×8 ASCII text, `clear`. | ⬜ |
| 6 | DS1307 RTC driver: BCD conversion, get/set time, auto-seed from build time when the clock-halt bit is set; show time on the OLED. | ⬜ |
| 7 | DHT11 driver: single-wire read, checksum validation, retry policy, interrupts masked during the timing-critical read. | ⬜ |
| 8 | USART driver (interrupt-driven RX ring buffer) + ESP-01 non-blocking AT state machine (reset → join AP → TCP → HTTP GET → close). May split into 8a/8b. | ⬜ |
| 9 | Application integration: state machine — sample → OLED with timestamp → threshold check → RGB-LED + buzzer alert → telemetry upload; graceful degradation when WiFi/ESP is unavailable. | ⬜ |
| 10 | Polish: `app_config.h(.example)` for thresholds & WiFi creds, watchdog timer, error/status surfaced on the OLED, README/log finalization. | ⬜ |

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
- `src/main.c`: now `i2c_init()` + a boot-time `i2c_scan()`; the alert-LED blink rate encodes the result — **slow ~1 Hz** if ≥1 device ACKed, **fast ~3 Hz** if none. (Becomes status *colours* in Step 4; a proper on-screen device list arrives with the OLED in Step 5.)
- `CMakeLists.txt`: added `src/hal/i2c.c`.
- README + CLAUDE.md updated (status, repo layout, notes).
- **Validation:**
  1. `cmake --build --preset default` compiles clean under `-Wall -Wextra`; `avr-size` grows by a few hundred bytes.
  2. *(static)* `TWBR` should be 32 for F_CPU=8 MHz, SCL=100 kHz (`grep TWBR build/heat_sentinel.lst` if you generate a listing, or just trust the formula).
  3. *(hardware)* With proper pull-ups and at least one of the SH1106 / DS1307 connected → power up → LED does the **slow** ~1 Hz blink. Remove all I²C devices (or the pull-ups) → LED switches to the **fast** ~3 Hz blink.
  4. *(optional, deeper)* Logic analyzer / scope on SDA & SCL: see START, `addr<<1` byte + ACK, STOP repeating at ~100 kHz during the boot scan.

### Step 4 — Output devices: RGB LED (HW-479) + buzzer ✅
- Roadmap grew 9 → 10 steps; SH1106/DS1307/DHT11/ESP-01/app/polish each shifted down by one. `board.h` reworked: `LED_PIN` removed; added `LED_R_PIN`/`LED_G_PIN`/`LED_B_PIN` (`PB1/PB2/PB3`) + `LED_RGB_ACTIVE_HIGH = 1` (HW-479 common-cathode, `-` `R` `G` `B` pins), and `BUZZER_PIN = PD3` (= OC2B) + `BUZZER_ACTIVE_HIGH = 1`.
- `src/drivers/led.{c,h}`: rewritten as an RGB driver — `led_color_t` enum (`LED_OFF`/`RED`/`GREEN`/`BLUE`/`YELLOW`/`CYAN`/`MAGENTA`/`WHITE` as channel-bit ORs), `led_init`/`led_set`/`led_get`/`led_off`/`led_toggle(color)` (toggle = show colour, or off if already showing it — for blinking one colour). Channel polarity from `LED_RGB_ACTIVE_HIGH`; glitch-free init (park inactive, then enable outputs).
- `src/drivers/buzzer.{c,h}`: works for either buzzer type — `buzzer_on/off` + non-blocking `buzzer_beep(ms)` for an active buzzer; non-blocking `buzzer_tone(freq_hz, ms)` for a passive one (Timer2 in CTC mode, OC2B toggled on compare-match with `OCR2B == OCR2A` → 50 % square wave; smallest prescaler chosen so `OCR2A` ∈ 0..255). `buzzer_tick()` (called from the loop) switches off a beep/tone when its deadline passes (wraparound-safe). `buzzer_off()` disconnects OC2B and returns PD3 to GPIO. Polarity from `BUZZER_ACTIVE_HIGH` (note: MH-FMD-style boards are often active-low). Timer2 is touched only while a tone is sounding.
- `src/main.c`: `led_init()` + `buzzer_init()`; after the boot `i2c_scan()`, sets the blink colour (green = ≥1 device, red = none), fires a `buzzer_tone(2300, 120)` boot chirp, and runs `buzzer_tick()` in the superloop alongside `sched_tick()`.
- `CMakeLists.txt`: added `src/drivers/buzzer.c`. README + CLAUDE.md updated (status, repo layout, peripherals, architecture, conventions).
- **Validation:**
  1. `cmake --build --preset default` compiles clean under `-Wall -Wextra`; `avr-size` grows a few hundred bytes.
  2. *(hardware — LED)* Power up: a single colour should blink. With ≥1 I²C device wired (+ pull-ups) → **green**, slow (~1 Hz). With nothing on the bus → **red**, fast (~3 Hz). If the colour is wrong-but-consistent (e.g. you get cyan where you expected green) the `LED_R/G/B_PIN` order in `board.h` doesn't match your wiring; if a colour is inverted/always-on, flip `LED_RGB_ACTIVE_HIGH`.
  3. *(hardware — buzzer)* You should hear a short ~120 ms chirp once at power-up. Silent or just a faint click on a beep → it's a passive buzzer (the chirp already uses the tone path, so it should chirp); if the buzzer is **continuously on** at rest → set `BUZZER_ACTIVE_HIGH = 0` in `board.h`. To sanity-check the tone path, scope PD3 during the chirp → ~2.3 kHz square wave.

## Key decisions

- **Language:** C, `-std=gnu11`, `-Os -g -Wall -Wextra`, sections GC'd.
- **Build:** CMake ≥ 3.20 + Ninja; toolchain file auto-applied. avrdude `flash` target with `AVRDUDE_PROGRAMMER` / `AVRDUDE_PORT` / `AVRDUDE_BAUD` cache vars.
- **Architecture:** bare-metal superloop + Timer0 `millis()` + a small cooperative scheduler (period/last-run/fn table). No RTOS.
- **ESP-01:** interrupt-driven USART RX ring buffer + non-blocking AT state machine, **9600 baud** (clean timing at 8 MHz; reconfigure the module with `AT+UART_DEF=9600,8,1,0,0`).
- **Debug:** none on UART (it's the ESP-01's); status/errors shown on the OLED. Optional compile-time `DEBUG_UART` hook for later.
- **Config layout:** no monolithic `config.h`; `board.h` (pins), per-driver headers, untracked `app_config.h` (secrets) from a committed `.example`.
- **RTC:** on boot, if the DS1307 clock-halt (CH) bit is set, seed time from build-time `__DATE__`/`__TIME__`; `rtc_set_time()` also available.
- **Alert (visual):** RGB LED module **HW-479** (assumed common-cathode → active-high; one toggle in `board.h` to flip). Digital on/off per channel → 7 colours; e.g. green = normal, red blink = over `TEMP_ALERT_C` (default 30 °C). Hardware-PWM colour mixing is a possible later extension. Buzzer added alongside (see below).
- **Alert (audible):** buzzer module on its own GPIO; short beep on entering the alert state (optional periodic chirp while in alert). Active-buzzer (GPIO on/off) vs passive-buzzer (timer-driven tone) handling TBD by the hardware.
- **Watchdog:** added in Step 10 (~2 s, kicked from the loop).
- **Provisional pins** (in `board.h`, user owns wiring): DHT11 → `PB0`; RGB LED → `PB1` (R) / `PB2` (G) / `PB3` (B); buzzer → `PD3`; I²C → `PC4/PC5` (fixed); UART → `PD0/PD1` (fixed); optional `ESP_RST` left commented (assumes CH_PD tied high).
- **Host unit tests:** skipped for now (firmware-only); may add later for BCD/checksum/parse logic.
- **Branch:** `master`. **License:** MIT.

## Next up

**Step 5 — SH1106 128×64 OLED driver** (`drivers/sh1106.{c,h}`): init sequence, the 2-px column offset, page/column addressing, `clear`, `set_cursor`, 5×8 ASCII text (`print_char`/`print_string` with auto-wrap). Built on the I²C HAL; `main.c` will show the I²C scan result (device list) and a banner on screen. Starts after the Step 4 commit.
