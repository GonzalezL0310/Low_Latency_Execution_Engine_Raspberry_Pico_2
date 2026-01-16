#!/bin/bash

# =================================================================
# Project: Low-Latency Execution Engine (Pico 2)
# Description: Main execution sequence with cron configuration
# =================================================================

# 1. Path definitions - Robustly locating the project directory
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_PATH="$PROJECT_ROOT/software/venv/bin/activate"
CAPTURE_SCRIPT="$PROJECT_ROOT/software/src/capture.py"
EGRESS_SCRIPT="$PROJECT_ROOT/software/src/egress.py"
ENGINE_SCRIPT="$PROJECT_ROOT/software/src/host_engine.py"
REPORT_SCRIPT="$PROJECT_ROOT/scripts/generate_report.sh"

echo "--- Starting System Sequence ---"

# 2. Check for virtual environment
if [ ! -f "$VENV_PATH" ]; then
    echo "[ERROR] Virtual environment not found. Run setup_all.sh first."
    exit 1
fi

source "$VENV_PATH"

# 3. Step 1: Launch Capture (Background)
echo "[STEP 1] Launching Market Data Capture..."
python3 "$CAPTURE_SCRIPT" &
CAPTURE_PID=$!

# 4. Step 2: Delay for stabilization
echo "[STEP 2] Waiting 10 seconds for data stabilization..."
sleep 10

# 5. Step 3: Launch Egress (Background)
echo "[STEP 3] Launching Egress Hot Path..."
python3 "$EGRESS_SCRIPT" &
EGRESS_PID=$!

# 6. Step 4: Additional delay
echo "[STEP 4] Waiting 1 second..."
sleep 1

# 7. Step 5: Launch Host Engine (Background)
echo "[STEP 5] Launching Integrated Host Engine & Auditor..."
python3 "$ENGINE_SCRIPT" &
ENGINE_PID=$!

# 8. Step 6: Configure Cron for Hourly Reports
echo "[STEP 6] Configuring Cron Job for generate_report.sh..."
CRON_JOB="0 * * * * /bin/bash $REPORT_SCRIPT"
(crontab -l 2>/dev/null | grep -v "$REPORT_SCRIPT"; echo "$CRON_JOB") | crontab -
echo "Cron configured: generate_report.sh will run every hour."

echo "--- System is now RUNNING ---"
echo "PIDs: Capture($CAPTURE_PID), Egress($EGRESS_PID), Engine($ENGINE_PID)"
echo "Use 'pkill -f python3' to stop all processes."
