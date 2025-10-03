# ===============================
# CMakeLists for Firmware/Tests
# ===============================
# This CMake file is used to automatically build all HIL (Hardware-in-the-Loop) test executables
# for the firmware. It scans for all test source files, creates corresponding ELF executables,
# sets up include directories, and provides a custom target for flashing each test to the STM32 MCU.

cmake_minimum_required(VERSION 3.20)  # Require at least CMake 3.20

# Recursively find all HIL test source files matching the pattern test_*_hil.c
file(GLOB_RECURSE HIL_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*/test_*_hil.c")

# Loop over each discovered HIL test source
foreach(TEST_SRC ${HIL_TEST_SOURCES})
    # Extract the filename without extension to use as the test name
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)  # e.g., test_uart_hil

    # Create an ELF executable for the test
    add_executable(${TEST_NAME}.elf
        ${TEST_SRC}
    )

    # Specify include directories required to build the test
    # These typically include all interface libraries, board-specific headers, and drivers
    target_include_directories(${TEST_NAME}.elf PRIVATE
        interface_actuators_lib     # Actuators interfaces
        interface_transport_lib     # Transport interfaces
        interface_sensors_lib       # Sensors interfaces
        interface_utilities_lib     # Utilities interfaces
        interface_system_lib        # System interfaces
    )

    # Create a custom target for flashing the test onto the STM32 via OpenOCD
    # Usage: make flash_<TEST_NAME>
    add_custom_target(flash_${TEST_NAME}
        COMMAND ${CMAKE_SOURCE_DIR}/DebugTools/flash.sh  ${TEST_NAME}
        DEPENDS ${TEST_NAME}.elf
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Flashing ${TEST_NAME}.elf to STM32 with OpenOCD"
    )
endforeach()
