# ============================================
# Toolchain configuration for ARM Cortex-M
# ============================================

# System and target processor
set(CMAKE_SYSTEM_NAME Generic)         # Bare-metal target
set(CMAKE_SYSTEM_PROCESSOR arm)        # ARM architecture

# Toolchain prefix (change here if using another variant)
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Compiler executables
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)

# Additional tools (for post-build steps)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE    ${TOOLCHAIN_PREFIX}size)

# Default Libs
set(CMAKE_SYSROOT /opt/arm-gnu)

# MCU Model to build
set(MCU_MODEL STM32G473xx)


# Include MCU configuration file
include("${CMAKE_CURRENT_LIST_DIR}/MCU/${MCU_MODEL}.cmake")

# C language flags
set(CMAKE_C_FLAGS "${MCU_FLAGS} --sysroot=${CMAKE_SYSROOT} -g -O0" CACHE STRING "C compiler flags" FORCE)

# C++ language flags
set(CMAKE_CXX_FLAGS "${MCU_FLAGS} --sysroot=${CMAKE_SYSROOT} -fno-exceptions -fno-rtti -g -O0" CACHE STRING "C++ compiler flags" FORCE)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ASM flags
set(CMAKE_ASM_FLAGS "${MCU_FLAGS}" CACHE STRING "ASM compiler flags" FORCE)

# Linker flags
# - Use custom linker script from firmware/linker/
# - Remove default startup files
# - Optimize unused sections
set(CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} -Wl,--gc-sections -specs=nano.specs" CACHE STRING "Linker flags" FORCE)


# Optional: Show binary size after build
set(CMAKE_POST_BUILD_COMMAND
    "${CMAKE_SIZE} ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.elf"
)
