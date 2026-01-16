#!/bin/bash

# --- Path Configuration ---
BUILD_DIR="firmware/build"
PICO_MOUNT_POINT="/media/$USER/RP2350" # Adjust this according to your Linux distribution

echo "--- Starting Build Process (Target: RP2350) ---"

# 1. Create build directory if it does not exist
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

# 2. Enter build directory
cd "$BUILD_DIR" || exit

# 3. Generate Makefiles with CMake
# Note: Ensure the PICO_SDK_PATH environment variable is set
cmake ..

# 4. Compile using all available cores
make -j$(nproc)

# 5. Verify if the .uf2 file was generated
if [ -f "pico_engine.uf2" ]; then
    echo "--- Build Successful: pico_engine.uf2 generated ---"
    
    # 6. Attempt automatic deployment if the Pico is in BOOTSEL mode
    if [ -d "$PICO_MOUNT_POINT" ]; then
        echo "Detected Pico 2 at $PICO_MOUNT_POINT. Flashing..."
        cp pico_engine.uf2 "$PICO_MOUNT_POINT"
        echo "Flashing complete. The Pico will restart automatically."
    else
        echo "Warning: Pico 2 not detected at $PICO_MOUNT_POINT."
        echo "Put the Pico in BOOTSEL mode and manually copy the file: $BUILD_DIR/pico_engine.uf2"
    fi
else
    echo "ERROR: Build failed."
    exit 1
fi
