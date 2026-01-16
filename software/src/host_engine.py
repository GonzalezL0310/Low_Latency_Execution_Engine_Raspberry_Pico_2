import sqlite3
import struct
import serial
import threading
import time
from datetime import datetime
import os

# --- Configuration ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_MARKET = os.path.abspath(os.path.join(BASE_DIR, "..", "data", "market_data.db"))
DB_AUDIT = os.path.abspath(os.path.join(BASE_DIR, "..", "data", "audit.db"))
SERIAL_PORT = "/dev/ttyACM0"
BAUDRATE = 921600
FRAME_FORMAT = "<H B f f B" # Little-endian as in the firmware
HEADER = 0xAA55

def compute_crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80: crc = (crc << 1) ^ 0x07
            else: crc <<= 1
            crc &= 0xFF
    return crc

def init_audit_db():
    conn = sqlite3.connect(DB_AUDIT)
    conn.execute("PRAGMA journal_mode=WAL;") # Consistency and concurrency
    conn.execute("""
        CREATE TABLE IF NOT EXISTS trades (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')),
            side TEXT,
            price REAL,
            pico_crc TEXT
        )
    """)
    conn.commit()
    conn.close()

# --- Thread B: Ingress (Auditor) ---
def ingress_thread(ser):
    print("[THREAD B] Auditor started.")
    conn = sqlite3.connect(DB_AUDIT)
    cursor = conn.cursor()

    while True:
        if ser.in_waiting > 0:
            try:
                # Read until newline (Pico's ASCII format)
                line = ser.readline().decode('ascii').strip()
                if line.startswith("TRD:"):
                    # Format: TRD:{SIDE}:{PRICE}:{CRC}
                    parts = line.split(':')
                    if len(parts) == 4:
                        side_val = int(parts[1])
                        price = float(parts[2])
                        received_crc = int(parts[3], 16)
                        
                        # CRC VALIDATION: Pack exactly like the Pico to verify
                        # temp_buf[0]=side, temp_buf[1..4]=price
                        check_data = struct.pack("<Bf", side_val, price)
                        computed_crc = compute_crc8(check_data)

                        if computed_crc == received_crc:
                            side_str = "BUY" if side_val == 1 else "SELL"
                            cursor.execute(
                                "INSERT INTO trades (side, price, pico_crc) VALUES (?, ?, ?)",
                                (side_str, price, hex(received_crc))
                            )
                            conn.commit()
                            print(f"\a[AUDIT] ORDER EXECUTED: {side_str} at {price:.4f}")
                        else:
                            print(f"[ERROR] Data corruption in Ingress. CRC mismatch.")
            except Exception as e:
                print(f"[ERROR B] Parsing failed: {e}")

        else:
            time.sleep(0.05)

# --- Thread A: Egress (Dispatch) ---
def egress_thread(ser):
    print("[THREAD A] Dispatch started.")
    # Reuse polling logic from egress.py
    conn_mkt = sqlite3.connect(DB_MARKET)
    cursor_mkt = conn_mkt.cursor()

    while True:
        start_time = time.time()
        cursor_mkt.execute("SELECT price, sma FROM market_ticks ORDER BY id DESC LIMIT 1")
        row = cursor_mkt.fetchone()
        
        if row:
            price, sma = row
            status = 0x01 if price and sma else 0x00
            
            # Binary frame construction (12 bytes)
            pre_pack = struct.pack("<H B f f", HEADER, status, price, sma)
            crc = compute_crc8(pre_pack)
            frame = struct.pack(FRAME_FORMAT, HEADER, status, price, sma, crc)
            
            try:
                ser.write(frame)
                ser.flush()
            except serial.SerialTimeoutException:
                pass # Avoid hanging the thread if the buffer fills up

        # Keep 100ms interval to avoid bus saturation
        elapsed = time.time() - start_time
        time.sleep(max(0, 0.1 - elapsed))

def main():
    init_audit_db()
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.1)
        print(f"Connected to Pico 2 at {SERIAL_PORT}")
    except Exception as e:
        print(f"CRITICAL: {e}")
        return

    # Launch Threads
    t_a = threading.Thread(target=egress_thread, args=(ser,), daemon=True)
    t_b = threading.Thread(target=ingress_thread, args=(ser,), daemon=True)
    
    t_a.start()
    t_b.start()

    try:
        while True: time.sleep(1) # Keep main process alive
    except KeyboardInterrupt:
        print("\nShutting down system...")
        ser.close()

if __name__ == "__main__":
    main()
