#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"

// --- Configuración de Hardware ---
#define LED_POSITION     PICO_DEFAULT_LED_PIN // LED Onboard: ON = Compra
#define LED_DEBUG        2                    // GPIO 2: Status/Reportes/Errores

// --- Protocolo de Comunicación (Host -> Pico) ---
#define FRAME_HEADER         0xAA55
#define FRAME_SIZE           12
#define HEARTBEAT_TIMEOUT_MS 2000

// --- Tiempos de Interfaz (No bloqueantes) ---
#define BLINK_REPORT_MS      50   // Parpadeo corto por envío exitoso
#define BLINK_CRC_ERROR_MS   100  // Velocidad de parpadeo por error de CRC

enum ParserState {
    SEARCH_HEADER,
    COLLECT_PAYLOAD,
    VERIFY_CRC
};

struct __attribute__((packed)) MarketFrame {
    uint16_t header;
    uint8_t  status;
    float    price;
    float    sma;
    uint8_t  crc;
};

// --- Funciones de Utilidad ---

// CRC8 para validación de tramas entrantes y salientes
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

// Envío de reporte de ejecución (Paso 4)
// Formato: TRD:{SIDE}:{PRICE}:{CRC}\n
void send_execution_report(bool side, float price) {
    char report[64];
    uint8_t side_val = side ? 1 : 0;
    
    // Preparamos el cuerpo del mensaje para calcular un CRC simple del contenido
    // Solo calculamos el CRC sobre el Side y el Precio para integridad básica
    uint8_t temp_buf[5];
    temp_buf[0] = side_val;
    memcpy(&temp_buf[1], &price, 4);
    uint8_t crc = compute_crc8(temp_buf, 5);

    // Formateo ASCII para el Host
    int len = snprintf(report, sizeof(report), "TRD:%d:%.4f:%02X\n", side_val, price, crc);
    
    // En el SDK de la Pico, printf a través de USB CDC es internamente buferizado.
    // Al ser un reporte corto y esporádico, no bloqueará el flujo principal.
    if (len > 0) {
        printf("%s", report);
    }
}

int main() {
    stdio_init_all();
    stdio_set_translate_crlf(&stdio_usb, false);

    gpio_init(LED_POSITION);
    gpio_set_dir(LED_POSITION, GPIO_OUT);
    gpio_init(LED_DEBUG);
    gpio_set_dir(LED_DEBUG, GPIO_OUT);

    ParserState current_state = SEARCH_HEADER;
    uint8_t buffer[FRAME_SIZE];
    size_t bytes_received = 0;
    
    // Variables de Estado de Trading
    bool in_position = false;
    bool last_in_position = false;
    absolute_time_t last_valid_frame_time = get_absolute_time();
    
    // Variables para gestión de LEDs de Debug (Sin bloqueo)
    absolute_time_t debug_timer = get_absolute_time();
    bool debug_led_active = false;
    bool crc_error_flag = false;

    while (true) {
        absolute_time_t now = get_absolute_time();

        // 1. Watchdog: Failsafe por pérdida de conexión
        // Si hay timeout, apagado total (Paso 4.3)
        if (absolute_time_diff_us(last_valid_frame_time, now) > (HEARTBEAT_TIMEOUT_MS * 1000)) {
            gpio_put(LED_POSITION, 0);
            gpio_put(LED_DEBUG, 0); 
            in_position = false;
            last_in_position = false; // Reset para disparar reporte al reconectar
        }

        // 2. Gestión de parpadeo de LED_DEBUG (No bloqueante)
        if (debug_led_active) {
            uint32_t diff = absolute_time_diff_us(debug_timer, now) / 1000;
            if (crc_error_flag) {
                // Parpadeo rápido para error de CRC
                gpio_put(LED_DEBUG, (diff / BLINK_CRC_ERROR_MS) % 2);
                if (diff > 500) { // Limitar el parpadeo de error a 500ms
                    debug_led_active = false;
                    crc_error_flag = false;
                    gpio_put(LED_DEBUG, 0);
                }
            } else {
                // Pulso corto para reporte enviado
                if (diff > BLINK_REPORT_MS) {
                    gpio_put(LED_DEBUG, 0);
                    debug_led_active = false;
                }
            }
        }

        // 3. Lectura de flujo serial (Hot Path)
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
                            buffer[0] = buffer[1];
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
                        last_valid_frame_time = now;
                        MarketFrame *frame = (MarketFrame *)buffer;
                        
                        // Lógica de Decisión
                        if (frame->status == 0x01) {
                            if (!in_position && frame->price < (frame->sma * 0.995f)) {
                                in_position = true;
                            } else if (in_position && frame->price > frame->sma) {
                                in_position = false;
                            }
                        } else {
                            in_position = false;
                        }

                        // --- PASO 4: Detección de cambio de estado y Reporte ---
                        if (in_position != last_in_position) {
                            gpio_put(LED_POSITION, in_position ? 1 : 0);
                            
                            // Disparo de reporte ASCII
                            send_execution_report(in_position, frame->price);
                            
                            // Feedback visual en LED Debug
                            gpio_put(LED_DEBUG, 1);
                            debug_timer = now;
                            debug_led_active = true;
                            crc_error_flag = false;

                            last_in_position = in_position;
                        }

                    } else {
                        // Error de CRC: Activamos parpadeo rápido
                        crc_error_flag = true;
                        debug_led_active = true;
                        debug_timer = now;
                    }
                    
                    current_state = SEARCH_HEADER;
                    bytes_received = 0;
                    break;
            }
        }
    }
}
