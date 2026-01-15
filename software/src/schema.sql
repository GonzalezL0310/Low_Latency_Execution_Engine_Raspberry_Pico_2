-- Market Data persistence table
-- Optimized for time-series insertion
CREATE TABLE IF NOT EXISTS market_ticks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')),
    price REAL NOT NULL,
    volume REAL,
    sma_30 REAL -- Pre-calculated SMA for the last 30 samples
);

-- System events for auditing connection drops
CREATE TABLE IF NOT EXISTS system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    event_type TEXT, -- 'CONNECTION_LOST', 'STARTUP', 'ERROR'
    message TEXT
);
