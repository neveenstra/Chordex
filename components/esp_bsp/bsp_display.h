#ifndef __BSP_DISPLAY_H__
#define __BSP_DISPLAY_H__

#include <stdio.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

// Waveshare ESP32-S3-Touch-LCD-1.47 — ST7789-family panel, 172x320, with
// CST816D capacitive touch. Pinout per the official schematic (J3 connector).
#define BSP_SPI_HOST           SPI2_HOST
#define BSP_LCD_PIXEL_CLOCK_HZ (12 * 1000 * 1000)

#define BSP_PIN_LCD_MISO       GPIO_NUM_NC
#define BSP_PIN_LCD_MOSI       GPIO_NUM_39
#define BSP_PIN_LCD_SCLK       GPIO_NUM_38
#define BSP_PIN_LCD_CS         GPIO_NUM_21
#define BSP_PIN_LCD_DC         GPIO_NUM_45
#define BSP_PIN_LCD_RST        GPIO_NUM_40
// BL drives the base of an SS8050 NPN that pulls the backlight LED cathode
// to ground when high — active HIGH.
#define BSP_PIN_LCD_BL         GPIO_NUM_46

// Capacitive touch (CST816D) on its own I2C bus.
#define BSP_PIN_TOUCH_SDA      GPIO_NUM_42
#define BSP_PIN_TOUCH_SCL      GPIO_NUM_41
#define BSP_PIN_TOUCH_RST      GPIO_NUM_47
#define BSP_PIN_TOUCH_INT      GPIO_NUM_48

// Native panel resolution. We render in landscape (rotation 90) so the visible
// frame becomes 320 x 172.
#define BSP_LCD_NATIVE_W       172
#define BSP_LCD_NATIVE_H       320
#define BSP_LCD_OFFSET_X       34   // ST7789 RAM is 240 wide; (240-172)/2 = 34
#define BSP_LCD_OFFSET_Y       0

// Backlight PWM
#define LCD_BL_LEDC_TIMER      LEDC_TIMER_0
#define LCD_BL_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LCD_BL_LEDC_CHANNEL    LEDC_CHANNEL_0
#define LCD_BL_LEDC_DUTY_RES   LEDC_TIMER_10_BIT
#define LCD_BL_LEDC_DUTY       (1024)
#define LCD_BL_LEDC_FREQUENCY  (5000)

#ifdef __cplusplus
extern "C" {
#endif

void    bsp_display_init(esp_lcd_panel_io_handle_t *io_handle,
                         esp_lcd_panel_handle_t *panel_handle,
                         size_t max_transfer_sz);
void    bsp_display_brightness_init(void);
void    bsp_display_set_brightness(uint8_t brightness);
uint8_t bsp_display_get_brightness(void);

#ifdef __cplusplus
}
#endif

#endif
