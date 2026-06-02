#include "din_midi.h"
#include "midi_router.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "din_midi";

#define DIN_UART_NUM       UART_NUM_1
#define DIN_UART_BAUD      31250
#define DIN_UART_BUF       512

// Diagnostic counters — readable from the UI so we can see whether the UART
// is actually getting any electrical activity on GPIO 7 without needing a
// serial monitor (USB port is busy with USB MIDI).
static volatile uint32_t s_raw_byte_count;
static volatile uint8_t  s_last_raw_byte;

uint32_t din_midi_raw_byte_count(void) { return s_raw_byte_count; }
uint8_t  din_midi_last_raw_byte(void)  { return s_last_raw_byte; }

// Activity-based "is the cable plugged in" inference: if no MIDI bytes for
// this long, mark the source disconnected. A keyboard sitting idle still
// emits Active Sensing (0xFE) every 300 ms by spec, so a 1 s window is safe.
#define DIN_IDLE_TIMEOUT_US (1500 * 1000LL)

typedef struct {
    uint8_t status;       // running status byte, 0 if none yet
    uint8_t data[2];      // partial data bytes
    uint8_t data_pos;     // 0..1
    uint8_t data_needed;  // how many data bytes the current status expects
} midi_parser_t;

static int data_bytes_for(uint8_t status)
{
    switch (status & 0xF0) {
    case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 2;
    case 0xC0: case 0xD0: return 1;
    default: break;
    }
    if (status == 0xF2) return 2;
    if (status == 0xF1 || status == 0xF3) return 1;
    return 0;
}

static void parser_feed(midi_parser_t *p, uint8_t b)
{
    // Real-time messages (0xF8..0xFF) can interleave; pass through and don't
    // affect running status.
    if (b >= 0xF8) {
        // Active sensing & friends — count as activity but ignore content.
        return;
    }

    if (b & 0x80) {
        // System common (0xF0..0xF7) cancels running status.
        if (b >= 0xF0 && b <= 0xF7) {
            p->status      = 0;
            p->data_pos    = 0;
            p->data_needed = 0;
            return;
        }
        p->status      = b;
        p->data_pos    = 0;
        p->data_needed = data_bytes_for(b);
        return;
    }

    if (p->status == 0 || p->data_needed == 0) return;

    p->data[p->data_pos++] = b;
    if (p->data_pos >= p->data_needed) {
        midi_router_handle(MIDI_SOURCE_DIN, p->status,
                           p->data[0],
                           (p->data_needed >= 2) ? p->data[1] : 0);
        p->data_pos = 0;
    }
}

static void din_midi_task(void *arg)
{
    midi_parser_t parser = { 0 };
    uint8_t       buf[64];

    int64_t last_byte_us = 0;
    bool    connected    = false;

    for (;;) {
        int n = uart_read_bytes(DIN_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(50));
        if (n > 0) {
            last_byte_us = esp_timer_get_time();
            s_raw_byte_count += n;
            s_last_raw_byte   = buf[n - 1];
            if (!connected) {
                connected = true;
                midi_router_set_connected(MIDI_SOURCE_DIN, true);
            }
            for (int i = 0; i < n; i++) parser_feed(&parser, buf[i]);
        } else if (connected &&
                   (esp_timer_get_time() - last_byte_us) > DIN_IDLE_TIMEOUT_US) {
            connected = false;
            parser.status = 0;
            midi_router_set_connected(MIDI_SOURCE_DIN, false);
        }
    }
}

void din_midi_init(void)
{
    const uart_config_t cfg = {
        .baud_rate  = DIN_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(DIN_UART_NUM, DIN_UART_BUF, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(DIN_UART_NUM, &cfg));
    // RX only — a DIN INPUT is a one-way path from the keyboard.
    ESP_ERROR_CHECK(uart_set_pin(DIN_UART_NUM, UART_PIN_NO_CHANGE, DIN_MIDI_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Internal pull-down on the RX pin. Holds the line LOW when the MIDI
    // cable is unplugged so the UART doesn't see noise from nearby digital
    // lines and frame phantom bytes.
    ESP_ERROR_CHECK(gpio_pulldown_en(DIN_MIDI_RX_GPIO));

    xTaskCreate(din_midi_task, "din_midi", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "DIN MIDI listening on GPIO %d @ %d baud",
             DIN_MIDI_RX_GPIO, DIN_UART_BAUD);
}
