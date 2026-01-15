# Technical Note: Handling SerialTimeoutException in the Hot Path

The error serial.serialutil.SerialTimeoutException: Write timeout during the execution of egress.py is expected behavior under the following conditions:

- Consumer Absence: The Host is attempting to transmit 12-byte binary frames at a 921,600 baud rate. If the Raspberry Pi Pico 2 is not connected or is not running the Phase 3 firmware (which is responsible for clearing the UART buffer), the Linux kernel's output buffer will fill almost instantaneously.

- "Hot Path" Integrity: The script is configured with a write timeout. This exception ensures that the system does not hang while attempting to send stale data. In low-latency trading, it is preferable for the dispatch process to fail rather than send a price with delay.

- Solution: This error will resolve once the execution engine on the Pico 2 is operational and consuming data deterministically.
