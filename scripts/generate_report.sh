#!/bin/bash

# --- Path Configuration ---
DB_PATH="../software/data/audit.db"
REPORT_DIR="../reports"
OUTPUT_FILE="$REPORT_DIR/trades_hourly.csv"

mkdir -p "$REPORT_DIR"

# Check if the database exists
if [ ! -f "$DB_PATH" ]; then
    echo "[ERROR] Database not found."
    exit 1
fi

# Print the header only once at the beginning of the file
echo "Date_Hour,Trade_Count" > "$OUTPUT_FILE"

# Process data, sort it, and append (>>) to the file
sqlite3 -noheader -csv "$DB_PATH" "SELECT timestamp FROM trades;" | \
awk -F',' '
{
    gsub(/"/, "", $1)
    key = substr($1, 1, 13)
    if (length(key) > 0) stats[key]++
}
END {
    for (period in stats) {
        printf "%s:00,%d\n", period, stats[period]
    }
}' | sort >> "$OUTPUT_FILE"

echo "[SUCCESS] Report correctly generated at $OUTPUT_FILE"
