# AVR (avr-gcc) cross-compilation toolchain.
# Auto-selected by CMakeLists.txt; override with -DCMAKE_TOOLCHAIN_FILE=<path>.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

# Compile a static lib for try_compile so CMake does not attempt to run AVR binaries.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_C_COMPILER   avr-gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER avr-g++)
find_program(CMAKE_ASM_COMPILER avr-gcc)

# Search for host programs normally; restrict library/include search to the AVR sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
