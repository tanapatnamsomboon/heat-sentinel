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
| 5 | SH1106 128×64 OLED driver: init (with the 2 px column offset), `set_cursor`, 5×8 ASCII text, `clear`. | ✅ |
| 6 | DS1307 RTC driver: BCD conversion, get/set time, auto-seed from build time when the clock-halt bit is set; show time on the OLED. | ✅ |
| 7 | DHT11 driver: single-wire read, checksum validation, retry policy, interrupts masked during the timing-critical read. | ✅ |
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
- **Build fix (post-Step-4):** first build failed — `led.c` used parameterized wrapper macros (`LED_CH_ON_(pin)`/`LED_CH_OFF_(pin)`) routing through the 1-arg `gpio_high`/`gpio_low`, so the `(LETTER, BIT)` pin pair got pre-expanded into two args before reaching them (`error: macro 'gpio_low' passed 2 arguments, but takes just 1`). Rewrote `led.c` to set channels via the public `gpio_write(pin, level)` macro (3-arg internal target → pin pair expands correctly). Committed as `fix: led.c — drive RGB channels via gpio_write() so the (LETTER,BIT) pin pair expands correctly`.

### Step 5 — SH1106 128×64 OLED driver ✅
- `src/drivers/sh1106.{c,h}`: framebuffer-less SH1106 over the `hal/i2c` master (no 1 KB shadow buffer — text is written straight to the panel; cheap on a 2 KB-RAM part).
  - Config (overridable): `SH1106_I2C_ADDR` (default `0x3C`), `SH1106_COL_OFFSET` (default `2` — the 132-col controller centres the 128-px panel), `SH1106_FLIP_180` (segment-remap / COM-scan-dir choice in the init sequence). `SH1106_WIDTH/HEIGHT/PAGES/COLS` exported.
  - `sh1106_init()` probes `0x3C` first → returns `false` if absent; otherwise pushes a 25-byte init sequence (display off → clock/mux/offset/start-line → DC-DC on → seg-remap + COM-dir → COM-pins `0x12` → contrast/pre-charge/VCOMH → resume-to-RAM → non-inverted → display on) as one I²C transaction (PROGMEM), then `sh1106_clear()`.
  - Cursor model `(x: 0..127, page: 0..7)`; the 2-px offset is applied when programming column address. `sh1106_set_cursor` (clamps, batches the 3 page/column commands), `sh1106_clear_line`, `sh1106_clear` (whole panel, cursor → 0,0; clears re-sync the cursor with the controller afterwards since SH1106 auto-increments columns).
  - Text: a 5×8 PROGMEM font for printable ASCII `0x20..0x7E` (95 glyphs, ~475 B flash); 6-px cell ⇒ 21 chars/line. `sh1106_putc` (handles `'\n'` and right-edge wrap to the next page; out-of-range chars → `'?'`), `sh1106_puts`, `sh1106_put_uint` (decimal), `sh1106_put_hex8` (two hex digits), `sh1106_print_line(page, s)` (= clear that line + puts). Also `sh1106_set_inverted` / `sh1106_set_on`.
- `src/main.c`: `bool oled_ok = sh1106_init();` after the I²C scan. If OK: prints "Heat Sentinel", a rule, "I2C devices: N", and each found address as "0xNN" (4 per row). LED colour now 3-state — **green** (OLED up) / **yellow** (devices found, OLED init failed) / **red** (nothing on the bus). Boot chirp unchanged.
- `CMakeLists.txt`: added `src/drivers/sh1106.c`. README + CLAUDE.md updated.
- **Validation:**
  1. `cmake --build --preset default` compiles clean under `-Wall -Wextra`; `avr-size` grows ~0.7–1 KB flash (mostly the font + init data).
  2. *(hardware)* With the OLED wired (SDA=PC4, SCL=PC5, pull-ups, addr 0x3C) → on power-up the screen shows the banner + "I2C devices: N" + the address list, and the RGB LED is **green**. The 1-px column offset means text starts flush at the left edge (no 2-px stripe on the left, no wrapped sliver on the right).
  3. *(hardware — fallbacks)* DS1307 (or anything) on the bus but the OLED unplugged/at the wrong address → LED **yellow**, screen blank. Nothing on the bus at all → LED **red**.
  4. *(troubleshooting)* Image upside-down or mirrored → set `SH1106_FLIP_180 1` (or 0) and rebuild. A 2-px blank stripe on the left / garbage sliver on the right → adjust `SH1106_COL_OFFSET`. Module doesn't respond at all → it may be at `0x3D` (set `SH1106_I2C_ADDR`).
- *(bench note, end of Step 5)* User confirmed on hardware: OLED shows "I2C devices: 1 / 0x3C" — only the OLED was wired; the DS1307 (0x68) wasn't connected yet. RGB LED green, build/font/scan all good.

### Step 6 — DS1307 RTC driver ✅
- `src/drivers/ds1307.{c,h}`: DS1307 over the `hal/i2c` helpers (`i2c_read_reg` / `i2c_write_reg`). `ds1307_time_t` (sec/min/hour 24h, day/month/year-as-yy, weekday). API: `ds1307_present`, `ds1307_get_time`, `ds1307_set_time` (clears the CH bit ⇒ starts the oscillator, forces 24-hour mode, clamps day/month/weekday), `ds1307_init_or_seed` (boot helper, returns `1` = was halted+seeded / `0` = already running / `-1` = absent or I²C error).
  - Internal BCD↔decimal helpers; `ds1307_get_time` reads regs 0x00–0x06 in one transaction and copes with 12-hour mode (bit6/AM-PM) just in case, though we only ever write 24h.
  - `build_time()`: parses `__DATE__` ("Mmm dd yyyy", day space-padded) and `__TIME__` ("hh:mm:ss") at runtime into a `ds1307_time_t` (weekday set to 1 — not derivable). Used by `ds1307_init_or_seed` when CH is set.
- `src/main.c`: calls `ds1307_init_or_seed()` after `sh1106_init()`; a 1 Hz `rtc_task` reads the clock and shows `20YY-MM-DD` (line 2) and `HH:MM:SS` (line 3) on the OLED (printed once immediately, then via the scheduler). Boot screen now: banner / rule / date / time / blank / "I2C dev: N" / addresses / ("RTC set from build" if it was seeded, or "RTC: not found" on line 2 if absent). `rtc_task` self-disables and shows "RTC: read error" if a later read fails. A local `put2()` zero-pads 2-digit fields.
- `CMakeLists.txt`: added `src/drivers/ds1307.c`. README + CLAUDE.md updated.
- **Validation:**
  1. `cmake --build --preset default` compiles clean under `-Wall -Wextra`; `avr-size` grows ~0.3–0.5 KB flash.
  2. *(hardware — DS1307 wired, addr 0x68, with crystal; battery recommended)* On power-up the boot screen shows the date on line 2 and a **clock ticking once per second** on line 3, and "I2C dev: 2" / "0x3C 0x68". With a fresh DS1307 (or no battery), line 7 reads "RTC set from build" and the clock starts from roughly the firmware's build time. After a power cycle *with* a good battery, the time should keep advancing (CH stays clear) and line 7 is absent.
  3. *(hardware — DS1307 absent)* Line 2 reads "RTC: not found"; the OLED is still green and otherwise normal.
  4. *(sanity)* The seconds field should roll 59 → 00 and bump minutes; the date should be plausible (≈ the build date) on a cold start.

### Step 7 — DHT11 temperature/humidity driver ✅
- `src/drivers/dht11.{c,h}`: single-wire bit-bang read on `DHT11_PIN` (PB0). `dht11_status_t` (OK / BUSY / NO_RESPONSE / TIMEOUT / BAD_CHECKSUM), `dht11_reading_t { humidity, temperature }` (8-bit integers — DHT11 has no fractional part; RH = byte0, T = byte2; checksum = sum of bytes 0..3). `dht11_init()` parks the line released (idle-HIGH via the external pull-up) and primes the interval timer. `dht11_read(out)`: enforces `DHT11_MIN_INTERVAL_MS` (2000, returns DHT11_BUSY if polled sooner), then — 20 ms low start pulse (interrupts on) → `ATOMIC_BLOCK` (interrupts off, ~5 ms): release, wait line-HIGH, wait response-LOW/HIGH, then 40 bits (`_delay_us(40)` sample point between a '0' ~26 µs and a '1' ~70 µs high pulse), then validate the checksum. Phase waits are bounded by `DHT11_PHASE_LOOPS` (~200 iterations ≈ ~250 µs) so a stuck line can't hang the read. Timer0 `millis()` loses a few ms per read — immaterial (DS1307 is the real clock).
  - "Retry policy" = the scheduled task simply re-reads every 2 s and `main.c` keeps showing the last good value in between (rather than blocking the loop with `_delay_ms` retry spins inside one call).
- `src/main.c`: `dht11_init()` in the boot sequence; a 2 s `dht_task` reads the sensor and shows `Temp: NN C` (line 4) / `Humi: NN %` (line 5). Boot screen now: banner / rule / date / time / "Temp: -- C" / "Humi: -- %" / "I2C dev: N" / addresses. `dht_task` keeps the last good value on a transient error; after 3 consecutive failures with no prior reading it shows "Temp: no sensor?". A `dht_task` is added at 2000 ms (its first read fires ≈2 s after boot — also gives the DHT11 its post-power-on settle time).
- `CMakeLists.txt`: added `src/drivers/dht11.c`. README + CLAUDE.md updated.
- **Validation:**
  1. `cmake --build --preset default` compiles clean under `-Wall -Wextra`; `avr-size` grows ~0.4–0.6 KB flash.
  2. *(hardware — DHT11 wired to PB0 with its pull-up)* ≈2 s after boot, lines 4–5 change from `-- C` / `-- %` to a plausible reading (e.g. 22–28 °C / 40–60 %RH indoors), refreshing every 2 s. Breathe on the sensor → humidity rises within a few seconds; cup it in your hand → temperature rises a degree or two.
  3. *(hardware — DHT11 absent / mis-wired)* lines 4–5 stay at `-- C` / `-- %`; after ~6 s line 4 shows `Temp: no sensor?`. The rest of the UI (clock, green LED) is unaffected — a bad DHT11 read can't hang the loop.
  4. *(sanity)* No visible glitch in the 1 Hz clock when a DHT read happens (the ~5 ms interrupts-off window is invisible); the buzzer chirp at boot is its usual ~120 ms.
  5. *(troubleshooting)* Always `Temp: 0 C` / `Humi: 0 %` or wild values → check the pull-up and that `DHT11_PIN` matches your wiring. Persistent `no sensor?` → no pull-up, wrong pin, or the DHT11 wasn't given ~1 s after power-up (the 2 s first-read delay should cover it).

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

**Step 8 — USART driver + ESP-01 AT layer** (`hal/uart.{c,h}` + `drivers/esp01.{c,h}`): interrupt-driven USART RX ring buffer @ 9600 baud, plus a non-blocking AT state machine (reset → join AP → open TCP → HTTP GET → close), ticked from the superloop so a slow upload never freezes the OLED. WiFi SSID/pass + telemetry host/path come from an untracked `app_config.h` (generated from a committed `app_config.h.example`; created in this step or Step 10). May split 8a (UART) / 8b (ESP-01). `main.c` will show WiFi state on the OLED and upload temp/humidity periodically. Starts after the Step 7 commit.
