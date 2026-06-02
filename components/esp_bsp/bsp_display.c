#include "bsp_display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "esp_lcd_jd9853.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "bsp_display";

static uint8_t g_brightness = 0;

void bsp_display_init(esp_lcd_panel_io_handle_t *io_handle,
                      esp_lcd_panel_handle_t *panel_handle,
                      size_t max_transfer_sz)
{
    ESP_LOGI(TAG, "SPI bus init");
    spi_bus_config_t buscfg = {
        .sclk_io_num     = BSP_PIN_LCD_SCLK,
        .mosi_io_num     = BSP_PIN_LCD_MOSI,
        .miso_io_num     = BSP_PIN_LCD_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = max_transfer_sz * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BSP_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = JD9853_PANEL_IO_SPI_CONFIG(
        BSP_PIN_LCD_CS, BSP_PIN_LCD_DC, NULL, NULL);
    io_config.pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_SPI_HOST,
                                             &io_config, io_handle));

    // JD9853 driver + init sequence + color settings cloned verbatim from
    // Tick-Tac. The S3-Touch-LCD-1.47 ships with the same 172x320 panel as
    // Tick-Tac, so this is the known-good combination.
    ESP_LOGI(TAG, "Install JD9853 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_PIN_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9853(*io_handle, &panel_config, panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(*panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_handle, true));
}

void bsp_display_brightness_init(void)
{
    // LEDC PWM on the BL pin, matching Tick-Tac's bsp_display exactly.
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LCD_BL_LEDC_MODE,
        .timer_num       = LCD_BL_LEDC_TIMER,
        .duty_resolution = LCD_BL_LEDC_DUTY_RES,
        .freq_hz         = LCD_BL_LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LCD_BL_LEDC_MODE,
        .channel    = LCD_BL_LEDC_CHANNEL,
        .timer_sel  = LCD_BL_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = BSP_PIN_LCD_BL,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void bsp_display_set_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        brightness = 100;
        ESP_LOGE(TAG, "Brightness value out of range");
    }
    g_brightness  = brightness;
    uint32_t duty = (brightness * (LCD_BL_LEDC_DUTY - 1)) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL));
    ESP_LOGI(TAG, "LCD brightness set to %d%%", brightness);
}

uint8_t bsp_display_get_brightness(void)
{
    return g_brightness;
}
