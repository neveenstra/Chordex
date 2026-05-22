#include "bsp_display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7789t.h"

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
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num         = BSP_PIN_LCD_DC,
        .cs_gpio_num         = BSP_PIN_LCD_CS,
        .pclk_hz             = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_SPI_HOST,
                                             &io_config, io_handle));

    // ST7789T-family panel — Waveshare's 1.47" 172x320 modules ship with
    // the same Vernon vendor init across the S3-LCD-1.47B and C6-Touch-LCD-1.47
    // boards. The stock IDF ST7789 driver leaves the gamma/VCOM/power
    // registers unset and the panel stays dark, so we use the vendor one.
    ESP_LOGI(TAG, "Install ST7789T panel driver");
    esp_lcd_panel_dev_st7789t_config_t panel_config = {
        .reset_gpio_num = BSP_PIN_LCD_RST,
        // Matches Tick-Tac's known-good color config: RGB element order,
        // inversion ON (Vernon's init writes 0x21, which is what we want).
        .rgb_endian     = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789t(*io_handle, &panel_config, panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));
    // ST7789T variant on this Waveshare 1.47" panel renders correct colors
    // with inversion OFF, even though Tick-Tac's JD9853 needs it ON. Vernon's
    // init sequence ends with 0x21 (INVON), so we explicitly send 0x20 here
    // to cancel it.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_handle, true));
}

// TEMP: bypass LEDC and drive BL as a plain GPIO HIGH while we verify the
// panel is actually getting clocks. If this lights the screen, the LEDC PWM
// path was the problem; if it stays dark, the panel init or SPI is.
void bsp_display_brightness_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BSP_PIN_LCD_BL,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(BSP_PIN_LCD_BL, 1);
    ESP_LOGI(TAG, "BL pin %d driven HIGH (active-high backlight)", BSP_PIN_LCD_BL);
}

void bsp_display_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    g_brightness = brightness;
    gpio_set_level(BSP_PIN_LCD_BL, brightness > 0 ? 1 : 0);
}

uint8_t bsp_display_get_brightness(void)
{
    return g_brightness;
}
