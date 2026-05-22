#ifndef CHORD_DETECT_H
#define CHORD_DETECT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHORDEX_NOTE_COUNT 128

typedef struct {
    // Short symbol shown on the main display, e.g. "Cmaj7", "F#m", "G/B".
    char    name[24];
    // Number of distinct held notes contributing to the result.
    uint8_t note_count;
    // MIDI note numbers of the held notes, sorted ascending.
    uint8_t notes[16];
    // Pitch-class set rooted at C, bit i set means note class i is present.
    uint16_t pitch_classes;
    // True when name is a confident chord match (vs. raw note list / "—").
    bool    is_chord;
} chord_result_t;

void chord_detect_init(void);

// Note on / off — call from any task / ISR-free context. Internally locked.
void chord_detect_note_on(uint8_t note, uint8_t velocity);
void chord_detect_note_off(uint8_t note);
void chord_detect_all_off(void);

// Snapshot the current chord result. Safe to call from the LVGL task.
void chord_detect_get(chord_result_t *out);

// Resolve a pitch class (0..11) to a note name string ("C", "C#", ...).
const char *chord_detect_pitch_name(uint8_t pc);

#ifdef __cplusplus
}
#endif

#endif
