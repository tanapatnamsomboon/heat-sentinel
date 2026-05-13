# Heat Sentinel

Bare-metal firmware for an ATmega328P-based temperature & humidity monitor.
It reads a DHT11, timestamps readings with a DS1307 RTC, shows status on an
SH1106 OLED, raises an RGB-LED + buzzer alert above a threshold, and pushes
telemetry to the cloud over an ESP-01 WiFi module (UART AT commands).

> **Status:** Step 8b of 10 complete — added non-blocking ESP-01 AT state machine and WiFi upload. See
> [`PROJECT_LOG.md`](PROJECT_LOG.md) for the full roadmap and progress.

## Hardware

| Role            | Part                  | Interface                      |
|-----------------|-----------------------|--------------------------------|
| MCU             | ATmega328P            | 8 MHz (`F_CPU = 8000000UL`)    |
| Temp / Humidity | DHT11                 | single-wire GPIO               |
| Temp (Analog)   | TMP35                 | Analog (ADC)                   |
| Display         | SH1106 OLED           | I²C (TWI), 128×64              |
| Real-time clock | DS1307                | I²C (TWI)                      |
| WiFi            | ESP-01                | USART, AT commands @ 9600 baud |
| Alert (visual)  | RGB LED module HW-479 | 3× GPIO (one pin per colour)   |
| Alert (audible) | Buzzer module         | GPIO                           |

Wiring, level shifting and circuit design are handled outside this repo; the
firmware treats the pin map as configuration. All assignments live in one place,
[`src/board.h`](src/board.h) — change them there, the drivers don't care.

### Pin map (defaults in `board.h`)

| Signal                          | ATmega328P pin                | Goes to                                        | Notes                                                                                                |
|---------------------------------|-------------------------------|------------------------------------------------|------------------------------------------------------------------------------------------------------|
| RGB LED — R / G / B             | **PB0 / PB1 / PB2**           | HW-479 `R` / `G` / `B`                         | common cathode → HW-479 `−` to GND; series resistors are on the module                               |
| DHT11 data                      | **PC0**                       | DHT11 `DATA`/`OUT`                             | needs an external pull-up on the line (it's on the usual DHT11 breakout)                             |
| TMP35 VOUT                      | **PC1**                       | TMP35 VOUT                                     | Analog input (ADC1); formula assumes 5V system                                                       |
| Buzzer signal                   | **PD3**                       | MH-FMD `S`/`I/O`                               | = OC2B, so Timer2 can tone-drive a passive buzzer; if it's on at rest, set `BUZZER_ACTIVE_HIGH = 0`  |
| I²C SDA / SCL                   | **PC4 / PC5** (fixed)         | SH1106 `SDA`/`SCL` **and** DS1307 `SDA`/`SCL`  | shared bus — wire SDAs together, SCLs together; one ~4.7 kΩ pull-up pair to VCC                      |
| UART TXD → ESP-01 RX            | **PD1** (fixed)               | ESP-01 `RX` (`U0RXD`)                          | **MCU TX → ESP RX** — the ESP-01's RX/TX cross over                                                  |
| UART RXD ← ESP-01 TX            | **PD0** (fixed)               | ESP-01 `TX` (`U0TXD`)                          | **MCU RX → ESP TX**                                                                                  |
| ESP-01 reset (optional)         | PD4                           | ESP-01 `RST`                                   | only if you uncomment `ESP_RST_PIN`; otherwise tie ESP `RST` + `CH_PD` to 3.3 V                      |
| ISP — MOSI / MISO / SCK / RESET | PB3 / PB4 / PB5 / PC6 (RESET) | your programmer                                | **reserved for the loader** — don't reuse PB3/PB4/PB5                                                |
| XTAL1 / XTAL2                   | PB6 / PB7                     | 8 MHz crystal, *if used*                       | free GPIO only when running the internal 8 MHz RC                                                    |

Only the ESP-01 uses the UART; the OLED + RTC share the I²C bus; the DHT11, the
three RGB-LED channels and the buzzer are plain GPIO. The ESP-01 is a 3.3 V part
— if the MCU runs at 5 V, level-shift the **MCU TX → ESP RX** line down to 3.3 V.

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
│  │  ├─ adc.{c,h}                 # 10-bit ADC driver (polling)
│  │  ├─ gpio.h                    # zero-cost GPIO macros, pins as (LETTER, BIT)
│  │  ├─ timer.{c,h}               # Timer0 -> 1 ms millis() tick
│  │  ├─ i2c.{c,h}                 # TWI master (blocking + timeout) + bus scanner
│  │  └─ uart.{c,h}                # USART0 8-N-1: IRQ RX ring buffer + blocking TX
│  ├─ drivers/
│  │  ├─ led.{c,h}                 # RGB LED (HW-479) on LED_R/G/B_PIN — named colours
│  │  ├─ buzzer.{c,h}              # buzzer: on/off + beep(ms); tone(freq,ms) via Timer2/OC2B
│  │  ├─ sh1106.{c,h}              # SH1106 128×64 OLED — text (5×8 font), no framebuffer
│  │  ├─ ds1307.{c,h}              # DS1307 RTC — BCD time get/set; seed from build time on cold start
│  │  ├─ dht11.{c,h}               # DHT11 temp/humidity — bit-banged, checksum, cli() during the read
│  │  └─ tmp35.{c,h}               # TMP35 analog temp sensor (10mV/C)
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

| Variable             | Default   | Meaning                                           |
|----------------------|-----------|---------------------------------------------------|
| `AVRDUDE_PROGRAMMER` | `usbasp`  | `avrdude -c` (e.g. `usbasp`, `arduino`)           |
| `AVRDUDE_PORT`       | `usb`     | `avrdude -P` (e.g. `usb`, `COM3`, `/dev/ttyUSB0`) |
| `AVRDUDE_BAUD`       | *(empty)* | `avrdude -b` (only needed for some programmers)   |

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

- The application logic is still a stand-in: `main.c` brings up the OLED / RTC /
  DHT11, pings the ESP-01 over the UART (shown on line 7: `ESP-01: ready` /
  `UART rx: …` / `UART: no reply`), chirps the buzzer, then runs scheduled jobs —
  a 1 Hz clock refresh and a 2 s DHT11 read. RGB LED: **green** if the OLED is
  up, **yellow** if devices were found but the OLED didn't init, **red** if
  nothing answered. The ESP-01 AT state machine + WiFi upload (Step 8b) and the
  threshold-driven RGB/buzzer alert (Step 9) are still to come.
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
- DHT11: single-wire bit-bang read on `DHT11_PIN` (default `PB0`), checksum-
  validated, ≥1 s between reads (the driver enforces `DHT11_MIN_INTERVAL_MS`,
  2 s). Interrupts are masked for the ~5 ms of bit timing — `millis()` loses a
  few ms per read (immaterial; the DS1307 is the authoritative clock). The data
  line's pull-up is part of your hardware (it's on the usual DHT11 breakout).
- UART: USART0 (RXD=PD0, TXD=PD1) at **9600 baud** — your ESP-01 must be set to
  that, e.g. `AT+UART_DEF=9600,8,1,0,0` via a USB-serial adapter (its factory
  default is usually 115200, which has too much error at 8 MHz). RX is
  interrupt-driven into a 64-byte ring buffer; TX is blocking.
- I²C runs at 100 kHz (the DS1307's maximum); SDA/SCL **external pull-ups are
  part of your hardware** — the firmware does not enable the AVR's internal ones.
- No ESP-01 AT layer yet; it arrives in Step 8b (along with the untracked
  `app_config.h` for WiFi credentials + telemetry endpoint).
- WiFi credentials and the telemetry endpoint will live in an untracked
  `app_config.h` generated from a committed `app_config.h.example` (Step 10).

## License

[MIT](LICENSE) © 2026 Tanapat Namsomboon
