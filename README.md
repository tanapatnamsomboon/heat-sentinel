# Heat Sentinel

Bare-metal firmware for an ATmega328P-based temperature & humidity monitor.
It reads a DHT11, timestamps readings with a DS1307 RTC, shows status on an
SH1106 OLED, raises an RGB-LED + buzzer alert above a threshold, and pushes
telemetry to the cloud over an ESP-01 WiFi module (UART AT commands).

> **Status:** Step 6 of 10 complete — build system, a 1 ms `millis()` tick, GPIO
> helpers, a cooperative scheduler, a blocking-with-timeout I²C (TWI) master +
> bus scanner, the RGB-LED (HW-479) driver, a buzzer driver, the SH1106 OLED
> driver, and the DS1307 RTC driver are in place. At boot the app chirps, scans
> the I²C bus, seeds the RTC from the build time if it was halted, and shows the
> scan result + a live date/time clock on the OLED. See
> [`PROJECT_LOG.md`](PROJECT_LOG.md) for the full roadmap and progress.

## Hardware

| Role             | Part                  | Interface                       |
|------------------|-----------------------|---------------------------------|
| MCU              | ATmega328P            | 8 MHz (`F_CPU = 8000000UL`)     |
| Temp / Humidity  | DHT11                 | single-wire GPIO                |
| Display          | SH1106 OLED           | I²C (TWI), 128×64               |
| Real-time clock  | DS1307                | I²C (TWI)                       |
| WiFi             | ESP-01                | USART, AT commands @ 9600 baud  |
| Alert (visual)   | RGB LED module HW-479 | 3× GPIO (one pin per colour)    |
| Alert (audible)  | Buzzer module         | GPIO                            |

Wiring, level shifting and circuit design are handled outside this repo; the
firmware treats the pin map as configuration (see `board.h`, added in Step 2).

## Repository layout

```
heat-sentinel/
├─ CMakeLists.txt                  # build configuration
├─ CMakePresets.json               # "default" preset: Ninja + AVR toolchain
├─ cmake/avr-gcc-toolchain.cmake   # avr-gcc cross-compile toolchain file
├─ src/
│  ├─ board.h                      # pin map + F_CPU (the only place pins live)
│  ├─ main.c                       # superloop: init + register scheduled jobs
│  ├─ hal/                         # hardware layer
│  │  ├─ gpio.h                    # zero-cost GPIO macros, pins as (LETTER, BIT)
│  │  ├─ timer.{c,h}               # Timer0 -> 1 ms millis() tick
│  │  └─ i2c.{c,h}                 # TWI master (blocking + timeout) + bus scanner
│  ├─ drivers/
│  │  ├─ led.{c,h}                 # RGB LED (HW-479) on LED_R/G/B_PIN — named colours
│  │  ├─ buzzer.{c,h}              # buzzer: on/off + beep(ms); tone(freq,ms) via Timer2/OC2B
│  │  ├─ sh1106.{c,h}              # SH1106 128×64 OLED — text (5×8 font), no framebuffer
│  │  └─ ds1307.{c,h}              # DS1307 RTC — BCD time get/set; seed from build time on cold start
│  └─ app/
│     └─ scheduler.{c,h}           # cooperative {period, last_run, fn} scheduler
├─ README.md                       # this file (kept up to date each step)
├─ PROJECT_LOG.md                  # step-by-step progress log
├─ CLAUDE.md                       # build/architecture notes for tooling
└─ LICENSE                         # MIT
```

## Prerequisites

You need `avr-gcc` + `avr-libc`, `cmake` (≥ 3.20), `ninja`, and `avrdude`
(for flashing). The same workflow works on Windows and macOS.

### Windows (via [Scoop](https://scoop.sh/))

```powershell
scoop install avr-gcc avrdude cmake ninja
```

(Chocolatey alternative: `choco install avrgcc avrdude cmake ninja`.)

### macOS (via [Homebrew](https://brew.sh/))

```bash
brew tap osx-cross/avr
brew install avr-gcc avrdude
brew install cmake ninja
```

Verify everything is on `PATH`:

```bash
avr-gcc --version
cmake --version
ninja --version
avrdude -?
```

## Build

Using the preset (recommended — works the same on both OSes):

```bash
cmake --preset default
cmake --build --preset default
```

Or without presets:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Build artifacts land in `build/`:

- `heat_sentinel.elf` — linked firmware (with debug symbols)
- `heat_sentinel.hex` — Intel HEX image for flashing
- `heat_sentinel.map` — linker map

The build also prints flash/RAM usage at the end (`avr-size`). Re-print it any
time with `cmake --build build --target size`.

## Flash

```bash
cmake --build build --target flash
```

`avrdude` settings are CMake cache variables — override them at configure time:

| Variable             | Default  | Meaning                                   |
|----------------------|----------|-------------------------------------------|
| `AVRDUDE_PROGRAMMER` | `usbasp` | `avrdude -c` (e.g. `usbasp`, `arduino`)   |
| `AVRDUDE_PORT`       | `usb`    | `avrdude -P` (e.g. `usb`, `COM3`, `/dev/ttyUSB0`) |
| `AVRDUDE_BAUD`       | *(empty)*| `avrdude -b` (only needed for some programmers) |

Examples:

```bash
# USBasp (default)
cmake --preset default

# Arduino-as-ISP on Windows COM5
cmake --preset default -DAVRDUDE_PROGRAMMER=stk500v1 -DAVRDUDE_PORT=COM5 -DAVRDUDE_BAUD=19200

# Bootloader (Arduino) on macOS
cmake --preset default -DAVRDUDE_PROGRAMMER=arduino -DAVRDUDE_PORT=/dev/tty.usbserial-XXXX -DAVRDUDE_BAUD=57600
```

### Fuses (one-time, do it yourself)

This project assumes the MCU runs at **8 MHz**. If you are using the internal
8 MHz RC oscillator with no clock divide, that's the factory default minus the
`CKDIV8` fuse:

```bash
# Internal 8 MHz RC, CKDIV8 disabled (do NOT change without checking your hardware)
avrdude -c usbasp -p atmega328p -U lfuse:w:0xE2:m -U hfuse:w:0xD9:m -U efuse:w:0xFF:m
```

If you wire an external 8 MHz crystal instead, the low fuse differs — pick the
value from a fuse calculator for your exact setup. (Fuse changes are not part of
the `flash` target on purpose.)

## Notes / current limitations

- The application logic is still a stand-in: `main.c` chirps the buzzer, scans
  the I²C bus, brings up the OLED, seeds the RTC from the build time if it was
  halted, prints the scan result, and refreshes a date/time clock once a second.
  RGB LED: **green** if the OLED is up, **yellow** if devices were found but the
  OLED didn't init, **red** if nothing answered. The threshold-driven alert and
  the live sensor / WiFi screens land in Steps 7–9.
- OLED driver is **framebuffer-less** (text written straight to the SH1106 — no
  1 KB shadow buffer): a 5×8 font, 21 chars × 8 lines, `'\n'`/right-edge wrap.
  `SH1106_I2C_ADDR` (default `0x3C`), `SH1106_FLIP_180` and `SH1106_COL_OFFSET`
  are overridable; if the image is upside-down/mirrored set `SH1106_FLIP_180 1`.
- RTC: the DS1307's clock-halt bit is checked on boot; if set (cold start / dead
  backup battery) the clock is seeded from the firmware build timestamp
  (`__DATE__`/`__TIME__`) — there's no on-device way to set the time yet, so the
  clock will be a few minutes off until you set it (e.g. reflash, or `ds1307_set_time`).
  The DS1307 backup-battery cell and 32.768 kHz crystal are part of your hardware.
- RGB LED is digital on/off per channel (8 states: off + 7 colours); polarity is
  `LED_RGB_ACTIVE_HIGH` in `board.h` (HW-479 is common-cathode → `1`). PWM colour
  mixing is a possible later extension.
- Buzzer: `BUZZER_PIN` (default `PD3` / OC2B). `BUZZER_ACTIVE_HIGH` toggles
  polarity — **many MH-FMD-style boards are active-low**, so set it to `0` if the
  buzzer is on at rest. For a passive buzzer the tone comes from Timer2 driving
  OC2B; Timer2 is only used while a tone is sounding.
- I²C runs at 100 kHz (the DS1307's maximum); SDA/SCL **external pull-ups are
  part of your hardware** — the firmware does not enable the AVR's internal ones.
- No DHT11 / ESP-01 drivers yet; they arrive in Steps 7–8.
- WiFi credentials and the telemetry endpoint will live in an untracked
  `app_config.h` generated from a committed `app_config.h.example` (Step 10).

## License

[MIT](LICENSE) © 2026 Tanapat Namsomboon
