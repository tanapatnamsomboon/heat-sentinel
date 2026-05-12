cmake_minimum_required(VERSION 3.20)

# --- Auto-select the AVR toolchain (must happen before project()) -------------
# Override on the command line with -DCMAKE_TOOLCHAIN_FILE=<path> if needed.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/avr-gcc-toolchain.cmake")
endif()

project(heat_sentinel
    VERSION 0.1.0
    DESCRIPTION "Bare-metal temperature/humidity monitor for ATmega328P"
    LANGUAGES C
)

# --- Target hardware ---------------------------------------------------------
set(MCU   "atmega328p" CACHE STRING "Target AVR MCU (-mmcu)")
set(F_CPU "8000000UL"  CACHE STRING "CPU clock in Hz, e.g. 8000000UL")

# --- Flashing (avrdude) ------------------------------------------------------
set(AVRDUDE_PROGRAMMER "usbasp" CACHE STRING "avrdude -c programmer (usbasp, arduino, ...)")
set(AVRDUDE_PORT       "usb"    CACHE STRING "avrdude -P port (usb, COM3, /dev/ttyUSB0, ...)")
set(AVRDUDE_BAUD       ""       CACHE STRING "avrdude -b baud (leave empty for programmer default)")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Firmware target ---------------------------------------------------------
set(FW_TARGET heat_sentinel)

add_executable(${FW_TARGET}
    src/main.c
    src/hal/timer.c
    src/drivers/led.c
    src/app/scheduler.c
)

set_target_properties(${FW_TARGET} PROPERTIES SUFFIX ".elf")

target_include_directories(${FW_TARGET} PRIVATE src)

target_compile_definitions(${FW_TARGET} PRIVATE F_CPU=${F_CPU})

target_compile_options(${FW_TARGET} PRIVATE
    -mmcu=${MCU}
    -std=gnu11
    -Os -g
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
)

target_link_options(${FW_TARGET} PRIVATE
    -mmcu=${MCU}
    -Wl,--gc-sections
    -Wl,-Map=${FW_TARGET}.map
)

# --- Post-build: generate .hex and print flash/RAM usage ---------------------
find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE    avr-size    REQUIRED)

set(HEX_FILE "${CMAKE_BINARY_DIR}/${FW_TARGET}.hex")

add_custom_command(TARGET ${FW_TARGET} POST_BUILD
    COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom "$<TARGET_FILE:${FW_TARGET}>" "${HEX_FILE}"
    COMMAND ${AVR_SIZE} --format=avr --mcu=${MCU} "$<TARGET_FILE:${FW_TARGET}>"
    BYPRODUCTS "${HEX_FILE}"
    VERBATIM
    COMMENT "Creating ${FW_TARGET}.hex and reporting size"
)

# --- `flash` target ----------------------------------------------------------
set(_avrdude_args -c ${AVRDUDE_PROGRAMMER} -p ${MCU} -P ${AVRDUDE_PORT})
if(AVRDUDE_BAUD)
    list(APPEND _avrdude_args -b ${AVRDUDE_BAUD})
endif()
list(APPEND _avrdude_args -U "flash:w:${HEX_FILE}:i")

add_custom_target(flash
    COMMAND avrdude ${_avrdude_args}
    VERBATIM
    COMMENT "Flashing ${HEX_FILE} via avrdude (${AVRDUDE_PROGRAMMER} on ${AVRDUDE_PORT})"
)
add_dependencies(flash ${FW_TARGET})

# --- `size` convenience target ----------------------------------------------
add_custom_target(size
    COMMAND ${AVR_SIZE} --format=avr --mcu=${MCU} "$<TARGET_FILE:${FW_TARGET}>"
    VERBATIM
    COMMENT "Flash/RAM usage for ${FW_TARGET}.elf"
)
add_dependencies(size ${FW_TARGET})
