# Heat Sentinel

Bare-metal firmware for an ATmega328P-based temperature & humidity monitor.
It reads a DHT11, timestamps readings with a DS1307 RTC, shows status on an
SH1106 OLED, raises a GPIO LED alert above a threshold, and pushes telemetry to
the cloud over an ESP-01 WiFi module (UART AT commands).

> **Status:** Step 1 of 9 complete — toolchain, CMake build, and `flash` target
> are in place with a placeholder blinky. See [`PROJECT_LOG.md`](PROJECT_LOG.md)
> for the full roadmap and progress.

## Hardware

| Role            | Part        | Interface                          |
|-----------------|-------------|------------------------------------|
| MCU             | ATmega328P  | 8 MHz (`F_CPU = 8000000UL`)        |
| Temp / Humidity | DHT11       | single-wire GPIO                   |
| Display         | SH1106 OLED | I²C (TWI), 128×64                   |
| Real-time clock | DS1307      | I²C (TWI)                          |
| WiFi            | ESP-01      | USART, AT commands @ 9600 baud     |
| Alert           | LED         | GPIO                               |

Wiring, level shifting and circuit design are handled outside this repo; the
firmware treats the pin map as configuration (see `board.h`, added in Step 2).

## Repository layout

```
heat-sentinel/
├─ CMakeLists.txt                  # build configuration
├─ CMakePresets.json               # "default" preset: Ninja + AVR toolchain
├─ cmake/avr-gcc-toolchain.cmake   # avr-gcc cross-compile toolchain file
├─ src/                            # firmware sources (currently: main.c blinky)
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

- `src/main.c` is a temporary blinky on `PB1` for toolchain verification only.
- No peripheral drivers yet; they arrive in Steps 3–7 (I²C, OLED, RTC, DHT11, ESP-01).
- WiFi credentials and the telemetry endpoint will live in an untracked
  `app_config.h` generated from a committed `app_config.h.example` (Step 9).

## License

[MIT](LICENSE) © 2026 Tanapat Namsomboon
