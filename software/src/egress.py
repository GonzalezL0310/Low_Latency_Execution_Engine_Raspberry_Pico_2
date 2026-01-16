import sqlite3
import struct
import serial
import time
import os

# --- Configuration ---
DB_PATH = os.path.join("..", "data", "market_data.db")
SERIAL_PORT = "/dev/ttyACM0" 
BAUDRATE = 921600
FRAME_FORMAT = "<H B f f B"
HEADER = 0xAA55
SEND_INTERVAL = 0.1 

def compute_crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF
    return crc

def get_latest_data():
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        cursor.execute("SELECT price, sma, timestamp FROM market_ticks ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        conn.close()
        return row
    except Exception as e:
        print(f"Error reading DB: {e}")
        return None

def main():
    print(f"Starting Egress Process (Hot Path) on Linux...")
    
    try:
        # In Linux, we open the port without attempting to configure internal Windows buffers
        ser = serial.Serial(
            port=SERIAL_PORT, 
            baudrate=BAUDRATE, 
            timeout=0.1,
            write_timeout=0.1
        )
    except Exception as e:
        print(f"CRITICAL: Could not open serial port: {e}")
        return

    try:
        while True:
            start_time = time.time()
            data = get_latest_data()
            
            status = 0x00 
            price = 0.0
            sma = 0.0
            
            if data:
                price, sma, ts = data
                status = 0x01 if (price is not None and sma is not None) else 0x00
            
            # Construction of the 12-byte frame
            pre_pack = struct.pack("<H B f f", HEADER, status, price, sma)
            crc = compute_crc8(pre_pack)
            frame = struct.pack(FRAME_FORMAT, HEADER, status, price, sma, crc)
            
            ser.write(frame)
            ser.flush()  # Crucial: forces immediate physical transmission

            elapsed = time.time() - start_time
            sleep_time = max(0, SEND_INTERVAL - elapsed)
            time.sleep(sleep_time)

    except KeyboardInterrupt:
        print("\nClosing communication...")
        # Kill Switch: Notify the Pico that the Host is disconnecting
        kill_frame = struct.pack(FRAME_FORMAT, HEADER, 0x00, 0.0, 0.0, 0x00)
        try:
            ser.write(kill_frame)
            ser.flush()
        except:
            pass
        ser.close()

if __name__ == "__main__":
    main()
