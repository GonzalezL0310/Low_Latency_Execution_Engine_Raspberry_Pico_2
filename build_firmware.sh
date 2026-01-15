#!/bin/bash

# --- Configuración de Rutas ---
BUILD_DIR="firmware/build"
PICO_MOUNT_POINT="/media/$USER/RP2350" # Ajusta esto según tu distribución Linux

echo "--- Iniciando proceso de Compilación (Target: RP2350) ---"

# 1. Crear directorio de build si no existe
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

# 2. Entrar al directorio de construcción
cd "$BUILD_DIR" || exit

# 3. Generar Makefiles con CMake
# Nota: Asegúrate de tener la variable PICO_SDK_PATH configurada en tu entorno
cmake ..

# 4. Compilar usando todos los núcleos disponibles
make -j$(nproc)

# 5. Verificar si se generó el archivo .uf2
if [ -f "pico_engine.uf2" ]; then
    echo "--- Compilación Exitosa: pico_engine.uf2 generado ---"
    
    # 6. Intento de despliegue automático si la Pico está en modo BOOTSEL
    if [ -d "$PICO_MOUNT_POINT" ]; then
        echo "Detectada Pico 2 en $PICO_MOUNT_POINT. Flasheando..."
        cp pico_engine.uf2 "$PICO_MOUNT_POINT"
        echo "Flasheo completado. La Pico se reiniciará automáticamente."
    else
        echo "Aviso: No se detectó la Pico 2 montada en $PICO_MOUNT_POINT."
        echo "Pon la Pico en modo BOOTSEL y copia manualmente el archivo: $BUILD_DIR/pico_engine.uf2"
    fi
else
    echo "ERROR: Falló la compilación."
    exit 1
fi
