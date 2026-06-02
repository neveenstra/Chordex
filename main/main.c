#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "bsp_display.h"
#include "lvgl_ui.h"

#include "chord_detect.h"
#include "midi_router.h"
#include "din_midi.h"
#include "usb_midi_device.h"

// Landscape: 320 wide × 172 tall. Achieved by setting MV (swap_xy) and MX
// (mirror_x) in MADCTL. The visible 172 pixels in the chip's 240-wide RAM
// are still on the X axis post-rotation, so the gap stays on x.
#define DISPLAY_ROTATION 90
#define LCD_H_RES (320)
#define LCD_V_RES (172)

#define LCD_DRAW_BUFF_HEIGHT (20)
#define LCD_DRAW_BUFF_DOUBLE (1)

static const char *TAG = "chordex";

static esp_lcd_panel_io_handle_t io_handle    = NULL;
static esp_lcd_panel_handle_t    panel_handle = NULL;
static lv_display_t             *lvgl_disp    = NULL;

// ---------------------------------------------------------------------------

static esp_err_t app_lvgl_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority     = 4,
        .task_stack        = 1024 * 10,
        .task_affinity     = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms   = 5,
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = io_handle,
        .panel_handle = panel_handle,
        .buffer_size  = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = LCD_DRAW_BUFF_DOUBLE,
        .hres         = LCD_H_RES,
        .vres         = LCD_V_RES,
        .monochrome   = false,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma   = true,
        },
    };

    // Landscape: with MV=1, the ST7789's CASET range becomes 0..319 (long
    // axis) and RASET becomes 0..239 (short axis), so the 34-pixel offset
    // moves onto the y_gap. Both mirror_x and mirror_y are needed in this
    // rotation — matches Waveshare's LV_DISP_ROT_90 path.
    disp_cfg.rotation.swap_xy  = true;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = false;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Background task: poll chord state + MIDI source connection state and push
// changes into the UI. Decoupling from the MIDI callback paths keeps both
// USB-host and UART parsers free of LVGL locking concerns.

static void ui_refresh_task(void *arg)
{
    bool     last_usb = false, last_din = false;
    uint32_t last_din_count = 0xFFFFFFFFu;

    for (;;) {
        bool usb = midi_router_is_connected(MIDI_SOURCE_USB);
        bool din = midi_router_is_connected(MIDI_SOURCE_DIN);

        uint32_t din_count = din_midi_raw_byte_count();
        uint8_t  din_last  = din_midi_last_raw_byte();

        if (lvgl_port_lock(0)) {
            if (usb != last_usb) {
                lvgl_ui_set_source_connected(0, usb);
                last_usb = usb;
            }
            if (din != last_din) {
                lvgl_ui_set_source_connected(1, din);
                last_din = din;
            }
            if (din_count != last_din_count) {
                lvgl_ui_set_din_debug(din_count, din_last);
                last_din_count = din_count;
            }
            lvgl_ui_refresh();
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(33));   // ~30 Hz UI tick
    }
}

// ---------------------------------------------------------------------------

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    bsp_display_init(&io_handle, &panel_handle, LCD_H_RES * LCD_DRAW_BUFF_HEIGHT);
    ESP_ERROR_CHECK(app_lvgl_init());

    bsp_display_brightness_init();
    bsp_display_set_brightness(100);

    chord_detect_init();
    midi_router_init();

    if (lvgl_port_lock(0)) {
        lvgl_ui_init();
        lvgl_port_unlock();
    }

    // MIDI inputs come up after the UI so the user sees the splash before
    // any pills light up.
    din_midi_init();
    usb_midi_device_init();

    xTaskCreate(ui_refresh_task, "ui_refresh", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Chordex ready");
}
