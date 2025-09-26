#!/bin/bash
set -e  # Stop script immediately if any command fails

# ==============================
# 📂 Paths Configuration
# ==============================
# Determine the root directory of the project relative to this script
PROJECT_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"

# Path to the build script at the project root
BUILD_SCRIPT="$PROJECT_ROOT/build.sh"

# Path to the generated ELF file after build
ELF_FILE="$PROJECT_ROOT/Build/Firmware/App/firmware.elf"

# ==============================
# Dedicated variable for linker script filename
# ==============================
LINKER_FILE="STM32G473CCTX_FLASH.ld"
LINKER="$PROJECT_ROOT/Linker/$LINKER_FILE"

# Path to OpenOCD log file
LOG_FILE="$PROJECT_ROOT/DebugTools/openocd_flash.log"

# ==============================
# 🔧 OpenOCD Target Configuration
# ==============================
TARGET_CFG="stm32g4x.cfg"   # OpenOCD target configuration file

# ==============================
# 🔧 Build Project
# ==============================
echo ""
echo "🔧 Building the project..."

# Execute the build script via bash to avoid path issues
bash "$BUILD_SCRIPT"
BUILD_EXIT=$?

# Check if build succeeded and ELF file exists
if [ $BUILD_EXIT -ne 0 ] || [ ! -f "$ELF_FILE" ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Build successful!"
echo ""

# ==============================
# 📑 Parse Linker Script for MCU Memory Sizes
# ==============================
# Ensure linker script exists
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

echo "💾 MCU memory (from linker: $LINKER_FILE):"
echo "   FLASH = $((FLASH_TOTAL/1024)) KB"
echo "   SRAM  = $((SRAM_TOTAL/1024)) KB"
echo ""

# ==============================
# 📊 Check Memory Usage of Firmware
# ==============================
echo "📊 Checking memory usage..."

# Use arm-none-eabi-size to get .text, .data, .bss sizes
SIZE_OUTPUT=$(arm-none-eabi-size -B "$ELF_FILE" | tail -n1)
TEXT=$(echo "$SIZE_OUTPUT" | awk '{print $1}')
DATA=$(echo "$SIZE_OUTPUT" | awk '{print $2}')
BSS=$(echo "$SIZE_OUTPUT" | awk '{print $3}')

FLASH_USED=$((TEXT + DATA))
SRAM_USED=$((DATA + BSS))

FLASH_PCT=$((FLASH_USED * 100 / FLASH_TOTAL))
SRAM_PCT=$((SRAM_USED * 100 / SRAM_TOTAL))

echo "   FLASH: $FLASH_USED / $FLASH_TOTAL bytes (${FLASH_PCT}%)"
echo "   SRAM : $SRAM_USED / $SRAM_TOTAL bytes (${SRAM_PCT}%)"
echo ""

# ==============================
# 🔌 Check Programmer (ST-LINK) and MCU Connection
# ==============================
echo "🔍 Checking programmer and MCU..."

# Check if ST-LINK is connected via USB
if lsusb | grep -q "ST-LINK"; then
    echo "✅ ST-LINK detected on USB."
else
    echo "❌ No ST-LINK found. Please connect your programmer."
    exit 1
fi

# Test MCU connection using OpenOCD
echo "🔍 Testing connection with OpenOCD..."
if openocd -f interface/stlink.cfg -f "target/$TARGET_CFG" -c "init; shutdown" >/dev/null 2>&1; then
    echo "✅ MCU detected via OpenOCD."
else
    echo "❌ Failed to connect to MCU via OpenOCD."
    exit 1
fi

# ==============================
# ⚡ Flashing MCU
# ==============================
echo -e "⚡ Starting flash process...\n"

# Flash ELF file to MCU using OpenOCD and verify
openocd -f interface/stlink.cfg -f "target/$TARGET_CFG" \
    -c "program $ELF_FILE verify reset exit" >"$LOG_FILE" 2>&1

# Check flash result
if [ $? -eq 0 ]; then
    echo -e "✅ Flashing complete! MCU should be running the new firmware.\n"
else
    echo "❌ Flashing failed. Check $LOG_FILE for details."
    exit 1
fi
