cmake_minimum_required(VERSION 3.20)

# Toolchain file must be set before project()
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/CMake/Toolchain.cmake" CACHE STRING "")

project(PROJECT_NAME LANGUAGES C CXX ASM)

# Include the Board subdirectory so the board_lib target is built
add_subdirectory(Firmware)


set(FIRMWARE_TARGET "firmware.elf" CACHE STRING "Target firmware for flashing")

add_custom_target(flash
    COMMAND ${CMAKE_SOURCE_DIR}/DebugTools/flash.sh firmware
    DEPENDS firmware.elf
)