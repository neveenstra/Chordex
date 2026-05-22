idf_component_register(SRCS "main.c"
                            "chord_detect.c"
                            "midi_router.c"
                            "din_midi.c"
                            "usb_midi_device.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES
                        nvs_flash
                        esp_bsp
                        lvgl_ui
                        espressif__esp_lvgl_port
                        espressif__esp_tinyusb
                        esp_timer
                        driver
                        log
                        freertos)
