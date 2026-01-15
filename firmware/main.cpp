#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"

// --- Configuración de Hardware ---
#define LED_POSITION     PICO_DEFAULT_LED_PIN // LED Onboard: ON = Compra/Posición
#define LED_ERROR        2                    // GPIO 2: Error de CRC o Latencia

// --- Protocolo de Comunicación ---
#define FRAME_HEADER         0xAA55
#define FRAME_SIZE           12
#define HEARTBEAT_TIMEOUT_MS 2000

enum ParserState {
    SEARCH_HEADER,
    COLLECT_PAYLOAD,
    VERIFY_CRC
};

// Estructura empaquetada para evitar padding del compilador
struct __attribute__((packed)) MarketFrame {
    uint16_t header;
    uint8_t  status;
    float    price;
    float    sma;
    uint8_t  crc;
};

// CRC8 idéntico a la implementación en Python del Host
uint8_t compute_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

int main() {
    // Inicialización de periféricos y stack USB
    stdio_init_all();
    
    // CRÍTICO: Desactivar traducción de caracteres para flujo binario puro
    stdio_set_translate_crlf(&stdio_usb, false);

    // Configuración de LEDs
    gpio_init(LED_POSITION);
    gpio_set_dir(LED_POSITION, GPIO_OUT);
    gpio_init(LED_ERROR);
    gpio_set_dir(LED_ERROR, GPIO_OUT);

    // Variables de estado del Parser y Trading
    ParserState current_state = SEARCH_HEADER;
    uint8_t buffer[FRAME_SIZE];
    size_t bytes_received = 0;
    absolute_time_t last_valid_frame_time = get_absolute_time();
    bool in_position = false;

    while (true) {
        // 1. Watchdog: Failsafe por pérdida de conexión
        if (absolute_time_diff_us(last_valid_frame_time, get_absolute_time()) > (HEARTBEAT_TIMEOUT_MS * 1000)) {
            gpio_put(LED_POSITION, 0);
            gpio_put(LED_ERROR, 1); 
            in_position = false;
        }

        // 2. Lectura no bloqueante del flujo USB CDC
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            uint8_t byte = (uint8_t)c;

            switch (current_state) {
                case SEARCH_HEADER:
                    buffer[bytes_received++] = byte;
                    if (bytes_received == 2) {
                        uint16_t header = (buffer[0] << 8) | buffer[1];
                        if (header == FRAME_HEADER) {
                            current_state = COLLECT_PAYLOAD;
                        } else {
                            buffer[0] = buffer[1]; // Shift para buscar sincronía
                            bytes_received = 1;
                        }
                    }
                    break;

                case COLLECT_PAYLOAD:
                    buffer[bytes_received++] = byte;
                    if (bytes_received == (FRAME_SIZE - 1)) {
                        current_state = VERIFY_CRC;
                    }
                    break;

                case VERIFY_CRC:
                    uint8_t received_crc = byte;
                    uint8_t computed_crc = compute_crc8(buffer, FRAME_SIZE - 1);

                    if (received_crc == computed_crc) {
                        last_valid_frame_time = get_absolute_time();
                        gpio_put(LED_ERROR, 0); 

                        MarketFrame *frame = (MarketFrame *)buffer;
                        
                        // 3. Lógica de Decisión (The Strategy)
                        if (frame->status == 0x01) {
                            // Compra: Precio < 99.5% de la SMA
                            if (!in_position && frame->price < (frame->sma * 0.995f)) {
                                in_position = true;
                                gpio_put(LED_POSITION, 1);
                            } 
                            // Venta: Precio > SMA
                            else if (in_position && frame->price > frame->sma) {
                                in_position = false;
                                gpio_put(LED_POSITION, 0);
                            }
                        } else {
                            // Status 0x00: Host en modo "Safe"
                            in_position = false;
                            gpio_put(LED_POSITION, 0);
                        }
                    } else {
                        gpio_put(LED_ERROR, 1); // Error de integridad
                    }
                    
                    // Reset para siguiente trama
                    current_state = SEARCH_HEADER;
                    bytes_received = 0;
                    break;
            }
        }
    }
}
