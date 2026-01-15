-- software/src/audit_schema.sql
CREATE TABLE IF NOT EXISTS trades (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')),
    side TEXT CHECK(side IN ('BUY', 'SELL')),
    price REAL NOT NULL,
    pico_crc TEXT NOT NULL
);
