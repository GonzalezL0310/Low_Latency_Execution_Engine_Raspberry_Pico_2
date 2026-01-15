#!/bin/bash

DB_PATH="../software/data/audit.db"
REPORT_DIR="../reports"
OUTPUT_FILE="$REPORT_DIR/trades_hourly.csv"

mkdir -p "$REPORT_DIR"

if [ ! -f "$DB_PATH" ]; then
    echo "[ERROR] No se encontró la base de datos."
    exit 1
fi

# Imprimimos el encabezado una sola vez al inicio del archivo
echo "Fecha_Hora,Cantidad_Operaciones" > "$OUTPUT_FILE"

# Procesamos los datos, los ordenamos y los anexamos (>>) al archivo
sqlite3 -noheader -csv "$DB_PATH" "SELECT timestamp FROM trades;" | \
awk -F',' '
{
    gsub(/"/, "", $1)
    clave = substr($1, 1, 13)
    if (length(clave) > 0) stats[clave]++
}
END {
    for (periodo in stats) {
        printf "%s:00,%d\n", periodo, stats[periodo]
    }
}' | sort >> "$OUTPUT_FILE"

echo "[SUCCESS] Reporte generado correctamente en $OUTPUT_FILE"
