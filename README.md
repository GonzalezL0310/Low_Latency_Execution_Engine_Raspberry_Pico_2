# Low-Latency Execution Engine (Pico 2)

A deterministic trading execution engine architecture. It utilizes a Linux Host for data orchestration and a Raspberry Pi Pico 2 for high-speed, hardware-level execution.

## System Architecture
- **Host (Linux):** Manages market data ingestion (WebSockets), persistence (SQLite3 WAL), and signal processing (SMA).
- **Target (RP2350):** Deterministic state machine for execution logic and GPIO signaling.
- **Communication:** High-speed UART (921,600 baud) with custom binary framing and CRC8 validation.

## Tech Stack
- **Languages:** Python 3.10+, C++ (Pico SDK).
- **Database:** SQLite3 with Write-Ahead Logging.
- **Tools:** CMake, Bash, AWK, Git.

## Setup Instructions
(To be updated as development progresses)
