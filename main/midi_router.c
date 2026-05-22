#include "midi_router.h"
#include "chord_detect.h"

#include "esp_timer.h"

#define SOURCE_COUNT 2

static volatile bool    s_connected[SOURCE_COUNT];
static volatile int64_t s_last_activity[SOURCE_COUNT];

void midi_router_init(void)
{
    for (int i = 0; i < SOURCE_COUNT; i++) {
        s_connected[i]     = false;
        s_last_activity[i] = 0;
    }
}

void midi_router_handle(midi_source_t src, uint8_t status, uint8_t d1, uint8_t d2)
{
    if (status < 0x80) return;
    if (status >= 0xF8) return;   // ignore real-time clock / system bytes

    uint8_t cmd = status & 0xF0;

    switch (cmd) {
    case 0x90:   // note on
        if (d2 == 0) {
            chord_detect_note_off(d1);
        } else {
            chord_detect_note_on(d1, d2);
            s_last_activity[src] = esp_timer_get_time();
        }
        break;
    case 0x80:   // note off
        chord_detect_note_off(d1);
        break;
    case 0xB0:   // CC — handle "all notes off" (123) and "reset all" (121)
        if (d1 == 123 || d1 == 120 || d1 == 121) {
            chord_detect_all_off();
        }
        break;
    default:
        break;
    }
}

void midi_router_set_connected(midi_source_t src, bool connected)
{
    if (src >= SOURCE_COUNT) return;
    s_connected[src] = connected;
    if (!connected) {
        // When a source disconnects, clear any notes that source might own.
        // We don't track ownership per-source; safest is to drop everything
        // so a stuck note from a yanked cable doesn't linger.
        chord_detect_all_off();
    }
}

bool midi_router_is_connected(midi_source_t src)
{
    if (src >= SOURCE_COUNT) return false;
    return s_connected[src];
}

int64_t midi_router_last_activity_us(midi_source_t src)
{
    if (src >= SOURCE_COUNT) return 0;
    return s_last_activity[src];
}
