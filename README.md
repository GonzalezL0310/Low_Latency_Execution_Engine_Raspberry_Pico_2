# Low-Latency Execution Engine (Pico 2)

A deterministic trading execution engine architecture designed for high-frequency environments. The system is split into a **Linux Host** for orchestration and data persistence, and a **Raspberry Pi Pico 2 (RP2350)** acting as a dedicated, hardware-level execution motor.

## System Architecture

### 1. Market Data Ingestion & Persistence (Phase 1)
- **Ingestion:** A Python-based process polls financial APIs (WebSockets prioritized) to minimize latency.
- **Persistence:** Each tick is stored in `market_data.db` using SQLite3 with **Write-Ahead Logging (WAL)** to ensure non-blocking concurrent reads.
- **Processing:** Real-time calculation of a **Simple Moving Average (SMA)** over a sliding window before dispatching to the target.

### 2. The "Hot Path" - Egress (Phase 2)
- **Transmission:** Pure binary stream over UART (USB-Serial) at **921,600 baud**.
- **Protocol:** 12-byte packed frame (`struct.pack('<H B f f B')`):
    - **Header:** 2 bytes (`0xAA55`) for synchronization.
    - **Status:** 1 byte (Heartbeat and connectivity status).
    - **Payload:** 8 bytes (2 Floats: Current Price + SMA).
    - **Integrity:** 1 byte (CRC8 validation).

### 3. Execution Motor - Pico 2 (Phase 3)
- **Methodology:** Single-core optimized **Finite State Machine (FSM)** for deterministic serial parsing.
- **Trading Logic:** Evaluates the condition: `Price < (SMA * Threshold)`.
- **Hardware Feedback:** - **Main LED:** Indicates an open position (BUY).
    - **Debug LED:** System status indicator (CRC Errors / Heartbeat Loss).
- **Failsafe:** Software Watchdog implementation. If no valid frames are received within the defined interval, all positions are forced to OFF.

### 4. Feedback Loop - Ingress (Phase 4)
- **Reporting:** The Pico 2 sends **Execution Reports** only upon state changes.
- **Format:** Structured ASCII string: `TRD:{Side}:{Price}:{CRC}\n`.
- **Asynchronous Parsing:** ASCII is utilized here for easier non-critical path handling on the Host.

### 5. Multithreading & Auditing (Phase 5)
- **Host Architecture:** Dual-thread execution:
    - **Thread A (Egress):** Constant API-to-Pico data flow.
    - **Thread B (Ingress):** Permanent serial listener for trade confirmation.
- **Audit Log:** Reports are persisted in `audit.db` with system-level timestamps for regulatory and performance tracking.

### 6. ETL Pipeline & BI (Phase 6)
- **Automation:** Hourly Cron jobs trigger Bash/AWK scripts for post-processing.
- **Aggregation:** Data is extracted from `audit.db` to generate incremental CSV reports (trades per hour/day).
- **Analysis:** Support for manual export to Power BI / Excel for performance metrics and dashboards.

## Tech Stack
- **Languages:** Python 3.12.3, C++ (Pico SDK).
- **Database:** SQLite3 (WAL Mode).
- **Tools:** CMake, Bash, AWK, Linux Cron.

## Setup & Execution

### 1. Environment Preparation
Before starting, ensure your Linux system is up to date and you have the **Pico SDK** installed and exported in your environment variables (`$PICO_SDK_PATH`).

```bash
# Clone the repository
git clone <your-repository-url>
cd <project-folder>

# Grant execution permissions to all scripts
chmod +x *.sh scripts/*.sh
```

### 2. Automated Installation
The following script handles system dependencies, Python virtual environment setup, and SQLite database initialization (WAL Mode).

```bash
./setup_all.sh
```

### 3. Firmware deploy
Deploy the deterministic execution motor to the hardware.

- Connect the Raspberry Pi Pico 2 to your PC while holding the BOOTSEL button.
- Compile and flash the firmware:

```bash
./build_firmware.sh
```

### 4. Running the System
The execution sequence follows a strict order to ensure data stabilization and synchronization.

```bash
./run_system.sh
```

This command will:

- Launch the Market Data Capture via WebSocket.
- Initialize the Egress Hot Path.
- Start the Host Engine & Auditor thread.
- Configure a Cron Job for hourly performance reports.

## Extra
- Monitoring: Use tail -f software/data/market data.db (or a sqlite browser) to verify real-time ingestion.
- Logs: System events and trade audits are stored in software/data/audit.db.
- Cleanup: To stop all background processes, use pkill -f python3.
