# ============================================
#             MCU configuration
# ============================================

# Minimum CMake version (good practice)
cmake_minimum_required(VERSION 3.20)

# Define MCU-specific compiler macros and flags
set(MCU_DEFINE "-DSTM32G473xx")

# MCU-specific compiler flags
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -Wall -Wextra -ffunction-sections -fdata-sections -O2 ${MCU_DEFINE}")

# MCU-specific linker script
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/Linker/STM32G473CCTX_FLASH.ld")

