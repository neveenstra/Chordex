#include "usb_midi_device.h"
#include "midi_router.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/midi/midi_device.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "usb_midi_dev";

enum { ITF_NUM_MIDI = 0, ITF_NUM_MIDI_STREAMING, ITF_COUNT };
enum { EP_EMPTY = 0, EPNUM_MIDI };

#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

static const char *s_str_desc[] = {
    (char[]){0x09, 0x04},   // 0: English (0x0409)
    "Crazysoap inc.",       // 1: Manufacturer
    "Chordex",              // 2: Product
    "CHORDEX0001",          // 3: Serial
    "Chordex MIDI",         // 4: MIDI interface
};

static const uint8_t s_midi_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64),
};

// MIDI byte-stream parser. tud_midi_stream_read returns raw MIDI bytes —
// the USB-MIDI 1.0 4-byte event packets are demuxed by TinyUSB — so we run
// the same running-status parser as the DIN UART path.
typedef struct {
    uint8_t status;
    uint8_t data[2];
    uint8_t data_pos;
    uint8_t data_needed;
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
    if (b >= 0xF8) return;
    if (b & 0x80) {
        if (b >= 0xF0 && b <= 0xF7) {
            p->status = 0;
            p->data_pos = 0;
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
        midi_router_handle(MIDI_SOURCE_USB, p->status,
                           p->data[0],
                           (p->data_needed >= 2) ? p->data[1] : 0);
        p->data_pos = 0;
    }
}

static void usb_midi_task(void *arg)
{
    midi_parser_t parser = { 0 };
    uint8_t       buf[64];
    bool          last_mounted = false;

    for (;;) {
        bool mounted = tud_midi_mounted();
        if (mounted != last_mounted) {
            midi_router_set_connected(MIDI_SOURCE_USB, mounted);
            last_mounted = mounted;
            if (!mounted) parser.status = 0;
        }

        if (mounted) {
            uint32_t n = tud_midi_stream_read(buf, sizeof(buf));
            if (n > 0) {
                for (uint32_t i = 0; i < n; i++) parser_feed(&parser, buf[i]);
                continue;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void usb_midi_device_init(void)
{
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.string            = s_str_desc;
    tusb_cfg.descriptor.string_count      = sizeof(s_str_desc) / sizeof(s_str_desc[0]);
    tusb_cfg.descriptor.full_speed_config = s_midi_cfg_desc;
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    xTaskCreate(usb_midi_task, "usb_midi", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "USB MIDI device installed (Chordex)");
}
