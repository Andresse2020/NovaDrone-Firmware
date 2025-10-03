# ===============================
# CMakeLists for Firmware/App
# ===============================

cmake_minimum_required(VERSION 3.20)

file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp)

# Define the library or executable for the application
# Here we will build an executable (ELF) called "firmware.elf"
add_executable(firmware.elf
    ${SRC_FILES}         # Your main application entry point
)

# ===============================
# Include directories
# ===============================
# These directories will be used for header file lookup
target_include_directories(firmware.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include     # App headers
)

# ===============================
# Link libraries
# ===============================
# Link your app with previously built static libraries
target_link_libraries(firmware.elf PRIVATE
    control_API       # Control library
    services_API      # Servives library
    config_lib        # Configuration library
    drivers_lib       # Drivers library
    board_lib         # Board-specific library
# Add other libraries if you build services/interfaces as separate libs
)

# ===============================
# Post-build commands
# ===============================
# Optional: convert ELF to binary and show size
add_custom_command(TARGET firmware.elf POST_BUILD
    # COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware.elf> $<TARGET_FILE_DIR:firmware.elf>/firmware.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware.elf>
)
