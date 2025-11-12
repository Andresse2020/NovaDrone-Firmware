#!/bin/bash
set -e  # Stop script immediately if any command fails

# ==============================
# Exemple of use
# ==============================
# flash <file_name>
# flash firmware

# ==============================
# 📂 Paths Configuration
# ==============================
# Determine the root directory of the project relative to this script
PROJECT_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"

# Path to the build script at the project root
BUILD_SCRIPT="$PROJECT_ROOT/build.sh"

# ==============================
# 🔹 Default ELF file
# ==============================
# Default firmware if no argument is provided
DEFAULT_FILE="firmware"

# Use first argument as the binary name, or default to DEFAULT_FILE
BINARY_NAME=${1:-$DEFAULT_FILE}

# Automatically append .elf extension
ELF_FILE_NAME="${BINARY_NAME}.elf"

# ==============================
# 🔹 Determine ELF file location
# ==============================
# If the binary name starts with "test_", it's a HIL test located in Build/Firmware/Tests
# Otherwise, it's the main firmware located in Build/Firmware/App
if [[ "$BINARY_NAME" == test_* ]]; then
    ELF_FILE="$PROJECT_ROOT/Build/Firmware/Tests/$ELF_FILE_NAME"
else
    ELF_FILE="$PROJECT_ROOT/Build/Firmware/App/$ELF_FILE_NAME"
fi

# ==============================
# 🔹 Linker Script Configuration
# ==============================
# Linker script used for MCU memory size extraction
LINKER_FILE="STM32G473CCTX_FLASH.ld"
LINKER="$PROJECT_ROOT/Linker/$LINKER_FILE"

# Path to OpenOCD log file
LOG_FILE="$PROJECT_ROOT/DebugTools/openocd_flash.log"

# OpenOCD target configuration file
TARGET_CFG="stm32g4x.cfg"

# ==============================
# 🔧 Build Project
# ==============================
echo ""
echo "🔧 Building the project..."
# Execute the build script
bash "$BUILD_SCRIPT"

# Verify that the ELF file exists after build
if [ ! -f "$ELF_FILE" ]; then
    echo "❌ Build failed or ELF file not found: $ELF_FILE"
    exit 1
fi

echo "✅ Build successful!"
echo ""

# ==============================
# 📑 Parse Linker Script for MCU Memory Sizes
# ==============================
# Check that linker script exists
if [ ! -f "$LINKER" ]; then
    echo "❌ Linker script not found: $LINKER"
    exit 1
fi

# Extract FLASH and RAM sizes from linker script (in KB)
FLASH_TOTAL=$(grep -Po 'FLASH.*LENGTH\s*=\s*\K[0-9]+K' "$LINKER" | tr -d 'K')
SRAM_TOTAL=$(grep -Po 'RAM.*LENGTH\s*=\s*\K[0-9]+K' "$LINKER" | tr -d 'K')

# Convert sizes to bytes
FLASH_TOTAL=$((FLASH_TOTAL * 1024))
SRAM_TOTAL=$((SRAM_TOTAL * 1024))

# Display MCU memory information
echo "💾 MCU memory (from linker: $LINKER_FILE):"
echo "   FLASH = $((FLASH_TOTAL/1024)) KB"
echo "   SRAM  = $((SRAM_TOTAL/1024)) KB"
echo ""

# ==============================
# 📊 Check Memory Usage of ELF
# ==============================
echo "📊 Checking memory usage..."
# Get .text, .data, .bss sizes from ELF
SIZE_OUTPUT=$(arm-none-eabi-size -B "$ELF_FILE" | tail -n1)
TEXT=$(echo "$SIZE_OUTPUT" | awk '{print $1}')
DATA=$(echo "$SIZE_OUTPUT" | awk '{print $2}')
BSS=$(echo "$SIZE_OUTPUT" | awk '{print $3}')

# Calculate used memory
FLASH_USED=$((TEXT + DATA))
SRAM_USED=$((DATA + BSS))

# Calculate percentage usage
FLASH_PCT=$((FLASH_USED * 100 / FLASH_TOTAL))
SRAM_PCT=$((SRAM_USED * 100 / SRAM_TOTAL))

# Display memory usage
echo "   FLASH: $FLASH_USED / $FLASH_TOTAL bytes (${FLASH_PCT}%)"
echo "   SRAM : $SRAM_USED / $SRAM_TOTAL bytes (${SRAM_PCT}%)"
echo ""

# ==============================
# 🔌 Check ST-LINK and MCU Connection
# ==============================
echo "🔍 Checking programmer and MCU..."
# Verify ST-LINK is connected via USB
if lsusb | grep -q "STLINK-V3"; then
    echo "✅ STLINK-V3 detected on USB."
else
    echo "❌ No ST-LINK found. Please connect your programmer."
    exit 1
fi

# Test MCU connection using OpenOCD
echo "🔍 Testing MCU connection with OpenOCD..."
if openocd -f interface/stlink.cfg -f "target/$TARGET_CFG" -c "init; shutdown" >/dev/null 2>&1; then
    echo "✅ MCU detected via OpenOCD."
else
    echo "❌ Failed to connect to MCU via OpenOCD."
    exit 1
fi

# ==============================
# ⚡ Flashing MCU
# ==============================
echo -e "⚡ Starting flash process for $ELF_FILE_NAME...\n"

# Flash ELF file using OpenOCD with verify and reset
openocd -f interface/stlink.cfg -f "target/$TARGET_CFG" \
    -c "program $ELF_FILE verify reset exit" >"$LOG_FILE" 2>&1

# Check flashing result and provide user feedback
if [ $? -eq 0 ]; then
    echo -e "✅ Flashing complete! MCU is now running $ELF_FILE_NAME.\n"
else
    echo "❌ Flashing failed. Check $LOG_FILE for details."
    exit 1
fi
