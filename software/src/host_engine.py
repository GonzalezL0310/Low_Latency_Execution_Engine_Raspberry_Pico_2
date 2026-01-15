import sqlite3
import struct
import serial
import threading
import time
from datetime import datetime
import os

# --- Configuración ---
DB_MARKET = os.path.join("..", "data", "market_data.db")
DB_AUDIT = os.path.join("..", "data", "audit.db")
SERIAL_PORT = "/dev/ttyACM0"
BAUDRATE = 921600
FRAME_FORMAT = "<H B f f B" # Little-endian como en el firmware
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
    conn.execute("PRAGMA journal_mode=WAL;") # Consistencia y concurrencia
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

# --- Hilo B: Ingress (Auditor) ---
def ingress_thread(ser):
    print("[THREAD B] Auditor iniciado.")
    conn = sqlite3.connect(DB_AUDIT)
    cursor = conn.cursor()

    while True:
        if ser.in_waiting > 0:
            try:
                # Leemos hasta el salto de línea (Formato ASCII de la Pico)
                line = ser.readline().decode('ascii').strip()
                if line.startswith("TRD:"):
                    # Formato: TRD:{SIDE}:{PRICE}:{CRC}
                    parts = line.split(':')
                    if len(parts) == 4:
                        side_val = int(parts[1])
                        price = float(parts[2])
                        received_crc = int(parts[3], 16)
                        
                        # VALIDACIÓN CRC: Empaquetamos igual que la Pico para verificar
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
                            print(f"\a[AUDIT] ORDEN EJECUTADA: {side_str} a {price:.4f}")
                        else:
                            print(f"[ERROR] Corrupción de datos en Ingress. CRC mismatch.")
            except Exception as e:
                print(f"[ERROR B] Fallo en el parseo: {e}")

# --- Hilo A: Egress (Despacho) ---
def egress_thread(ser):
    print("[THREAD A] Despacho iniciado.")
    # Reutilizamos la lógica de polling de egress.py
    conn_mkt = sqlite3.connect(DB_MARKET)
    cursor_mkt = conn_mkt.cursor()

    while True:
        start_time = time.time()
        cursor_mkt.execute("SELECT price, sma FROM market_ticks ORDER BY id DESC LIMIT 1")
        row = cursor_mkt.fetchone()
        
        if row:
            price, sma = row
            status = 0x01 if price and sma else 0x00
            
            # Construcción de trama binaria de 12 bytes
            pre_pack = struct.pack("<H B f f", HEADER, status, price, sma)
            crc = compute_crc8(pre_pack)
            frame = struct.pack(FRAME_FORMAT, HEADER, status, price, sma, crc)
            
            try:
                ser.write(frame)
                ser.flush()
            except serial.SerialTimeoutException:
                pass # Evitamos colgar el hilo si el buffer se llena

        # Mantenemos el intervalo de 100ms para no saturar el bus
        elapsed = time.time() - start_time
        time.sleep(max(0, 0.1 - elapsed))

def main():
    init_audit_db()
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.1)
        print(f"Conectado a Pico 2 en {SERIAL_PORT}")
    except Exception as e:
        print(f"CRÍTICO: {e}")
        return

    # Lanzamiento de Hilos
    t_a = threading.Thread(target=egress_thread, args=(ser,), daemon=True)
    t_b = threading.Thread(target=ingress_thread, args=(ser,), daemon=True)
    
    t_a.start()
    t_b.start()

    try:
        while True: time.sleep(1) # Mantener vivo el proceso principal
    except KeyboardInterrupt:
        print("\nApagando sistema...")
        ser.close()

if __name__ == "__main__":
    main()
