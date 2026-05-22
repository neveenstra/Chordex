#ifndef DIN_MIDI_H
#define DIN_MIDI_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// MIDI DIN-5 input wired through a 6N138 / H11L1 opto into a UART RX pin.
// GPIO 7 is broken out on the P1 header (pin 20) and doesn't conflict with
// LCD, touch, USB, or SD on the ESP32-S3-Touch-LCD-1.47.
#define DIN_MIDI_RX_GPIO    GPIO_NUM_7

void din_midi_init(void);

#ifdef __cplusplus
}
#endif

#endif
