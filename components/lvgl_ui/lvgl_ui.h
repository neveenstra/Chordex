#ifndef __LVGL_UI_H__
#define __LVGL_UI_H__

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_ui_init(void);

// Push the latest snapshot from chord_detect into the UI. Caller must hold
// the LVGL port lock. Idempotent — only redraws when something changed.
void lvgl_ui_refresh(void);

// Reflect connection state of a MIDI source (0 = USB, 1 = DIN) on the
// header pills.
void lvgl_ui_set_source_connected(int source, bool connected);

// Pulse the matching pill for one frame (e.g. on each note-on).
void lvgl_ui_pulse_source(int source);

void lvgl_ui_set_dimmed(bool dimmed);

// Temporary on-screen hardware probe for the DIN UART. Counter is the total
// bytes the UART has framed since boot; last_byte is the most recent one.
// Stuck at zero = nothing electrically reaching GPIO 7.
void lvgl_ui_set_din_debug(uint32_t count, uint8_t last_byte);

#ifdef __cplusplus
}
#endif

#endif
