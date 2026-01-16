import sqlite3
import json
import websocket
import sys
import os

# --- Configuration ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.abspath(os.path.join(BASE_DIR, "..", "data", "market_data.db"))
SYMBOL = "btcusdt"
WS_URL = f"wss://stream.binance.com:9443/ws/{SYMBOL}@ticker"
SMA_WINDOW = 30  # Number of samples for the Simple Moving Average

def init_db():
    """Initializes the database and sets WAL mode for concurrency."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # Enable Write-Ahead Logging (WAL)
    cursor.execute("PRAGMA journal_mode=WAL;")
    
    # Create table with SMA column for future reference
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS market_ticks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')),
            price REAL NOT NULL,
            volume REAL,
            sma REAL
        )
    ''')
    conn.commit()
    return conn

def calculate_sma(cursor):
    """Calculates the Simple Moving Average of the last N ticks."""
    query = f"SELECT price FROM market_ticks ORDER BY id DESC LIMIT {SMA_WINDOW}"
    cursor.execute(query)
    prices = [row[0] for row in cursor.fetchall()]
    
    if len(prices) < SMA_WINDOW:
        return None # Not enough data yet
    
    return sum(prices) / len(prices)

def on_message(ws, message):
    data = json.loads(message)
    # Binance 'ticker' stream uses 'c' for close price and 'v' for total volume
    current_price = float(data['c'])
    volume = float(data['v'])
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # Pre-flight calculation
    sma_value = calculate_sma(cursor)
    
    # Persistence
    cursor.execute(
        "INSERT INTO market_ticks (price, volume, sma) VALUES (?, ?, ?)",
        (current_price, volume, sma_value)
    )
    conn.commit()
    
    # Log to console for debugging
    print(f"[TICK] Price: {current_price} | SMA({SMA_WINDOW}): {sma_value if sma_value else 'Calculating...'}")
    
    conn.close()

def on_error(ws, error):
    print(f"!!! Connection Error: {error}")
    # Failsafe: Stop the system if internet drops
    print("CRITICAL: Shutting down system to prevent stale data execution.")
    sys.exit(1)

def on_close(ws, close_status_code, close_msg):
    print("### Connection Closed ###")
    sys.exit(1)

if __name__ == "__main__":
    # Ensure data directory exists
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
    
    print(f"Starting Market Data Feed for {SYMBOL.upper()}...")
    init_db()
    
    ws = websocket.WebSocketApp(
        WS_URL,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close
    )
    
    ws.run_forever()
