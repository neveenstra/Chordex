#ifndef MIDI_ROUTER_H
#define MIDI_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MIDI_SOURCE_USB = 0,
    MIDI_SOURCE_DIN = 1,
} midi_source_t;

void midi_router_init(void);

// Push a single 3-byte MIDI message (status, data1, data2) from a source.
// Status bytes < 0x80 are ignored. Real-time bytes (0xF8+) are tolerated.
void midi_router_handle(midi_source_t src, uint8_t status, uint8_t d1, uint8_t d2);

// Mark a source as connected/disconnected. Reflected on the UI status pills.
void midi_router_set_connected(midi_source_t src, bool connected);

bool midi_router_is_connected(midi_source_t src);

// Time of the most recent note-on for this source, in microseconds since boot.
// Returns 0 if no note has been seen yet. Used by the UI to flash the pill.
int64_t midi_router_last_activity_us(midi_source_t src);

#ifdef __cplusplus
}
#endif

#endif
