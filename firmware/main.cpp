#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"

// --- Hardware Configuration ---
#define LED_POSITION     PICO_DEFAULT_LED_PIN // Onboard LED: ON = Buy
#define LED_DEBUG        2                    // GPIO 2: Status/Reports/Errors

// --- Communication Protocol (Host -> Pico) ---
#define FRAME_HEADER         0xAA55
#define FRAME_SIZE           12
#define HEARTBEAT_TIMEOUT_MS 2000

// --- Interface Timing (Non-blocking) ---
#define BLINK_REPORT_MS      50   // Short blink for successful transmission
#define BLINK_CRC_ERROR_MS   100  // Blink speed for CRC error

// --- Cost variation percentage
#define VARIATION 0.99995f

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

// --- Utility Functions ---

// CRC8 for incoming and outgoing frame validation
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

// Execution report transmission (Step 4)
// Format: TRD:{SIDE}:{PRICE}:{CRC}\n
void send_execution_report(bool side, float price) {
    char report[64];
    uint8_t side_val = side ? 1 : 0;
    
    // Prepare the message body to calculate a simple CRC for the content
    // The CRC is only calculated over Side and Price for basic integrity
    uint8_t temp_buf[5];
    temp_buf[0] = side_val;
    memcpy(&temp_buf[1], &price, 4);
    uint8_t crc = compute_crc8(temp_buf, 5);

    // ASCII formatting for the Host
    int len = snprintf(report, sizeof(report), "TRD:%d:%.4f:%02X\n", side_val, price, crc);
    
    // In the Pico SDK, printf via USB CDC is internally buffered.
    // Since it is a short and sporadic report, it will not block the main flow.
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
    
    // Trading State Variables
    bool in_position = false;
    bool last_in_position = false;
    absolute_time_t last_valid_frame_time = get_absolute_time();
    
    // Debug LED management variables (Non-blocking)
    absolute_time_t debug_timer = get_absolute_time();
    bool debug_led_active = false;
    bool crc_error_flag = false;

    while (true) {
        absolute_time_t now = get_absolute_time();

        // 1. Watchdog: Failsafe for connection loss
        // Total shutdown upon timeout (Step 4.3)
        if (absolute_time_diff_us(last_valid_frame_time, now) > (HEARTBEAT_TIMEOUT_MS * 1000)) {
            gpio_put(LED_POSITION, 0);
            gpio_put(LED_DEBUG, 0); 
            in_position = false;
            last_in_position = false; // Reset to trigger report upon reconnection
        }

        // 2. Debug LED blink management (Non-blocking)
        if (debug_led_active) {
            uint32_t diff = absolute_time_diff_us(debug_timer, now) / 1000;
            if (crc_error_flag) {
                // Fast blink for CRC error
                gpio_put(LED_DEBUG, (diff / BLINK_CRC_ERROR_MS) % 2);
                if (diff > 500) { // Limit error blink to 500ms
                    debug_led_active = false;
                    crc_error_flag = false;
                    gpio_put(LED_DEBUG, 0);
                }
            } else {
                // Short pulse for sent report
                if (diff > BLINK_REPORT_MS) {
                    gpio_put(LED_DEBUG, 0);
                    debug_led_active = false;
                }
            }
        }

        // 3. Serial stream reading (Hot Path)
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            uint8_t byte = (uint8_t)c;

            switch (current_state) {
                case SEARCH_HEADER:
                    buffer[bytes_received++] = byte;
                    if (bytes_received == 2) {
                        uint16_t header = (buffer[1] << 8) | buffer[0];
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
                        
                        // Decision Logic
                        if (frame->status == 0x01) {
                            if (!in_position && frame->price < (frame->sma * VARIATION)) {
                                in_position = true;
                            } else if (in_position && frame->price > frame->sma) {
                                in_position = false;
                            }
                        } else {
                            in_position = false;
                        }

                        // --- STEP 4: State change detection and Report ---
                        if (in_position != last_in_position) {
                            gpio_put(LED_POSITION, in_position ? 1 : 0);
                            
                            // ASCII report trigger
                            send_execution_report(in_position, frame->price);
                            
                            // Visual feedback on Debug LED
                            gpio_put(LED_DEBUG, 1);
                            debug_timer = now;
                            debug_led_active = true;
                            crc_error_flag = false;

                            last_in_position = in_position;
                        }

                    } else {
                        // CRC Error: Activate fast blinking
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
