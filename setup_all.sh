#!/bin/bash

# =================================================================
# Project: Low-Latency Execution Engine (Pico 2)
# Description: Automated installation and environment setup
# Target OS: Linux (Ubuntu/Debian recommended)
# =================================================================

# --- 1. System Dependency Installation ---
echo "[STEP 1] Installing system dependencies..."
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    python3 \
    python3-venv \
    sqlite3 \
    git \
    udev

# --- 2. Python Environment Setup ---
echo "[STEP 2] Setting up Python virtual environment..."
cd software || { echo "Error: 'software' directory not found"; exit 1; }
python3 -m venv venv
source venv/bin/activate

if [ -f "requirements.txt" ]; then
    pip install --upgrade pip
    pip install -r requirements.txt
else
    echo "Warning: requirements.txt not found. Installing core packages manually..."
    pip install ccxt==4.5.0 pyserial==3.5 websocket-client==1.6.1
fi
cd ..

# --- 3. Database Initialization ---
echo "[STEP 3] Initializing SQLite databases (WAL Mode)..."
mkdir -p software/data

# Create Market Data DB
if [ -f "software/src/schema.sql" ]; then
    sqlite3 software/data/market_data.db < software/src/schema.sql
    echo "Market database initialized."
fi

# Create Audit DB
if [ -f "software/src/audit_schema.sql" ]; then
    sqlite3 software/data/audit.db < software/src/audit_schema.sql
    echo "Audit database initialized."
fi

# --- 4. Permissions Configuration ---
echo "[STEP 4] Configuring serial port permissions..."
# Adding current user to 'dialout' to access /dev/ttyACM0 without sudo
sudo usermod -a -G dialout "$USER"

# --- 5. Firmware Build Verification ---
echo "[STEP 5] Checking firmware build prerequisites..."
if [ -z "$PICO_SDK_PATH" ]; then
    echo "-------------------------------------------------------"
    echo "ADVICE: PICO_SDK_PATH is not set in your environment."
    echo "To compile the firmware, please install the Pico SDK and"
    echo "export the path in your .bashrc file."
    echo "-------------------------------------------------------"
else
    echo "Pico SDK detected at: $PICO_SDK_PATH"
    chmod +x build_firmware.sh
    echo "You can now run ./build_firmware.sh to flash the Pico 2."
fi

# --- Final Instructions ---
echo ""
echo "======================================================="
echo " SETUP COMPLETED SUCCESSFULLY "
echo "======================================================="
echo "IMPORTANT: You must LOG OUT and LOG BACK IN for the"
echo "serial port permissions (dialout group) to take effect."
echo ""
echo "First run ./build_firmware.sh to flash the Pico 2."
echo "Then run ./system_run.sh"
echo "======================================================="
