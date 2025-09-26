#!/bin/bash

# ==============================
# ST-LINK Detection Script
# ==============================

# ==============================
# Configuration
# ==============================
TARGET_CFG="stm32f4x.cfg"
TARGET_CPU="stm32f4x.cpu"

# Step 1: Check if ST-LINK is connected via USB
if lsusb | grep -q "0483:374b"; then
    echo "✅ ST-LINK detected on USB."
else
    echo "❌ Programmer not detected (USB)."
    exit 1
fi

# Step 2: Test connection with OpenOCD
echo "🔍 Testing connection with OpenOCD..."
OPENOCD_OUTPUT=$(openocd -f interface/stlink.cfg -f "target/$TARGET_CFG" -c "init; reset; exit" 2>&1)

if echo "$OPENOCD_OUTPUT" | grep -q "$TARGET_CPU"; then
    echo "✅ MCU detected via OpenOCD."
else
    echo "❌ Programmer detected but MCU not reachable."
    echo "OpenOCD details:"
    echo "$OPENOCD_OUTPUT"
    exit 1
fi
