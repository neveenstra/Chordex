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

// Raw UART counters for on-screen hardware diagnosis. The counter ticks up
// for any byte the UART successfully framed (regardless of whether the MIDI
// parser then accepted it), so a stuck-at-zero counter means "no UART
// activity on GPIO at all".
uint32_t din_midi_raw_byte_count(void);
uint8_t  din_midi_last_raw_byte(void);

#ifdef __cplusplus
}
#endif

#endif
