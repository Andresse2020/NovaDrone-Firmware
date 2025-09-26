#!/bin/bash

# Start timer
START_TIME=$(date +%s)

SOURCE_DIR="$(dirname "$0")"   # This points to DevE-Templete-Bare-metal
BUILD_DIR="$SOURCE_DIR/Build"

echo -e "\n=== Configuring build ==="
cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" || { echo "CMake configuration failed"; exit 1; }

echo -e "\n=== Building project ==="
cmake --build "$BUILD_DIR" || { echo "Build failed"; exit 1; }

# End timer
END_TIME=$(date +%s)

ELAPSED=$((END_TIME - START_TIME))
echo -e "\n=== Build time = ${ELAPSED} seconds ===\n"

