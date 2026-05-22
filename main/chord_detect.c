#include "chord_detect.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Chord templates as pitch-class bitmasks rooted at 0 (C).
// Listed roughly longest-first so richer matches win ties.
typedef struct {
    uint16_t    mask;
    const char *suffix;   // appended to root name, e.g. "m7", "maj7", ""
    uint8_t     note_count;
} chord_template_t;

static const chord_template_t TEMPLATES[] = {
    // 7 notes — full 13th chords
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<2)|(1<<5)|(1<<9),  "13",      7 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<11)|(1<<2)|(1<<5)|(1<<9),  "maj13",   7 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10)|(1<<2)|(1<<5)|(1<<9),  "m13",     7 },
    // 6 notes — 11th chords (full) and 13ths missing the 11th (very common)
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<2)|(1<<5),         "11",      6 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10)|(1<<2)|(1<<5),         "m11",     6 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<11)|(1<<2)|(1<<5),         "maj11",   6 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<2)|(1<<9),         "13",      6 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10)|(1<<2)|(1<<9),         "m13",     6 },
    // 5 notes
    { (1<<0)|(1<<4)|(1<<7)|(1<<9)|(1<<2),                 "6/9",     5 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<9)|(1<<2),                 "m6/9",    5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<2),                "9",       5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<11)|(1<<2),                "maj9",    5 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10)|(1<<2),                "m9",      5 },
    // Altered dominants & related — root, 3rd, 5th, b7 plus an altered tone
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<1),                "7b9",     5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<3),                "7#9",     5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<6),                "7#11",    5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<8),                "7b13",    5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<11)|(1<<6),                "maj7#11", 5 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10)|(1<<1),                "m7b9",    5 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<11)|(1<<2),                "mMaj9",   5 },
    { (1<<0)|(1<<3)|(1<<6)|(1<<10)|(1<<2),                "m9b5",    5 },
    { (1<<0)|(1<<3)|(1<<6)|(1<<10)|(1<<1),                "m7b5b9",  5 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10)|(1<<9),                "m13(no9)",5 },
    { (1<<0)|(1<<5)|(1<<7)|(1<<10)|(1<<2),                "9sus4",   5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<5),                "11(no9)", 5 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<9),                "13(no9)", 5 },
    // 4 notes
    { (1<<0)|(1<<4)|(1<<7)|(1<<11),         "maj7",    4 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<10),         "7",       4 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<10),         "m7",      4 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<11),         "mMaj7",   4 },
    { (1<<0)|(1<<3)|(1<<6)|(1<<9),          "dim7",    4 },
    { (1<<0)|(1<<3)|(1<<6)|(1<<10),         "m7b5",    4 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<9),          "6",       4 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<9),          "m6",      4 },
    { (1<<0)|(1<<5)|(1<<7)|(1<<10),         "7sus4",   4 },
    { (1<<0)|(1<<2)|(1<<7)|(1<<10),         "7sus2",   4 },
    { (1<<0)|(1<<4)|(1<<7)|(1<<2),          "add9",    4 },
    { (1<<0)|(1<<3)|(1<<7)|(1<<2),          "madd9",   4 },
    { (1<<0)|(1<<5)|(1<<7)|(1<<9),          "6sus4",   4 },
    { (1<<0)|(1<<3)|(1<<6)|(1<<11),         "dim(maj7)", 4 },
    // Altered dominants without the 5 — common jazz voicings
    { (1<<0)|(1<<4)|(1<<6)|(1<<10),         "7b5",     4 },
    { (1<<0)|(1<<4)|(1<<8)|(1<<10),         "7#5",     4 },
    { (1<<0)|(1<<4)|(1<<6)|(1<<11),         "maj7b5",  4 },
    { (1<<0)|(1<<4)|(1<<8)|(1<<11),         "maj7#5",  4 },
    // 3 notes
    { (1<<0)|(1<<4)|(1<<7),                 "",        3 },   // major
    { (1<<0)|(1<<3)|(1<<7),                 "m",       3 },
    { (1<<0)|(1<<3)|(1<<6),                 "dim",     3 },
    { (1<<0)|(1<<4)|(1<<8),                 "aug",     3 },
    { (1<<0)|(1<<2)|(1<<7),                 "sus2",    3 },
    { (1<<0)|(1<<5)|(1<<7),                 "sus4",    3 },
    { (1<<0)|(1<<4)|(1<<10),                "7(no5)",  3 },
    { (1<<0)|(1<<3)|(1<<10),                "m7(no5)", 3 },
    // 2 notes (intervals)
    { (1<<0)|(1<<7),                        "5",       2 },   // power chord
    { (1<<0)|(1<<3),                        "m3",      2 },
    { (1<<0)|(1<<4),                        "M3",      2 },
};

#define TEMPLATE_COUNT (sizeof(TEMPLATES) / sizeof(TEMPLATES[0]))

static const char *PITCH_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// ----- state -----------------------------------------------------------------

static SemaphoreHandle_t s_mtx;
static bool              s_held[CHORDEX_NOTE_COUNT];
static chord_result_t    s_last;

// ----- helpers ---------------------------------------------------------------

static uint16_t rotate_mask(uint16_t mask, uint8_t shift)
{
    // Rotate a 12-bit pitch-class mask left by 'shift' semitones.
    shift %= 12;
    return ((mask << shift) | (mask >> (12 - shift))) & 0x0FFF;
}

static int popcount12(uint16_t m)
{
    int c = 0;
    for (int i = 0; i < 12; i++) if (m & (1 << i)) c++;
    return c;
}

// Try to identify a chord from a pitch-class set + lowest pitch class.
// Returns true and writes name into out if a confident match is found.
static bool identify_chord(uint16_t pcs, uint8_t bass_pc, char *out, size_t out_sz)
{
    int active = popcount12(pcs);
    if (active < 2) return false;

    // Pass 1 — try every template with the bass note as the root. Extended
    // chords like Em13 / A13 share the same pitch-class set; without this
    // pass the detector would commit to the first template that matched at
    // *any* root, robbing the bass of its naming authority.
    for (size_t t = 0; t < TEMPLATE_COUNT; t++) {
        if (TEMPLATES[t].note_count != active) continue;
        if (rotate_mask(TEMPLATES[t].mask, bass_pc) == pcs) {
            snprintf(out, out_sz, "%s%s",
                     PITCH_NAMES[bass_pc], TEMPLATES[t].suffix);
            return true;
        }
    }

    // Pass 2 — fall back to slash chords (root != bass). Walk templates in
    // order so richer suffixes (defined earlier) win over plainer ones.
    for (size_t t = 0; t < TEMPLATE_COUNT; t++) {
        if (TEMPLATES[t].note_count != active) continue;
        for (uint8_t root = 0; root < 12; root++) {
            if (root == bass_pc) continue;
            if (rotate_mask(TEMPLATES[t].mask, root) == pcs) {
                snprintf(out, out_sz, "%s%s/%s",
                         PITCH_NAMES[root], TEMPLATES[t].suffix,
                         PITCH_NAMES[bass_pc]);
                return true;
            }
        }
    }
    return false;
}

static void recompute_locked(void)
{
    uint16_t pcs = 0;
    uint8_t  notes[16];
    uint8_t  count = 0;
    int      lowest = -1;

    for (int n = 0; n < CHORDEX_NOTE_COUNT; n++) {
        if (!s_held[n]) continue;
        pcs |= (1u << (n % 12));
        if (lowest < 0) lowest = n;
        if (count < (uint8_t)(sizeof(notes))) {
            notes[count++] = (uint8_t)n;
        }
    }

    chord_result_t r = { 0 };
    r.note_count    = count;
    memcpy(r.notes, notes, count);
    r.pitch_classes = pcs;
    r.is_chord      = false;

    if (count == 0) {
        snprintf(r.name, sizeof(r.name), "-");
    } else if (count == 1) {
        // Single note: "C4"
        int octave = (lowest / 12) - 1;   // MIDI 60 = C4
        snprintf(r.name, sizeof(r.name), "%s%d",
                 PITCH_NAMES[lowest % 12], octave);
    } else {
        uint8_t bass_pc = (uint8_t)(lowest % 12);
        if (identify_chord(pcs, bass_pc, r.name, sizeof(r.name))) {
            r.is_chord = true;
        } else {
            // Fallback: print pitch classes in ascending order from bass.
            char *p   = r.name;
            char *end = r.name + sizeof(r.name);
            for (int i = 0; i < 12 && p < end; i++) {
                uint8_t pc = (bass_pc + i) % 12;
                if (pcs & (1u << pc)) {
                    int written = snprintf(p, end - p, "%s%s",
                                           (p == r.name) ? "" : " ",
                                           PITCH_NAMES[pc]);
                    if (written < 0 || written >= end - p) break;
                    p += written;
                }
            }
        }
    }

    s_last = r;
}

// ----- public ----------------------------------------------------------------

void chord_detect_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    memset(s_held, 0, sizeof(s_held));
    memset(&s_last, 0, sizeof(s_last));
    snprintf(s_last.name, sizeof(s_last.name), "-");
}

void chord_detect_note_on(uint8_t note, uint8_t velocity)
{
    if (note >= CHORDEX_NOTE_COUNT) return;
    if (velocity == 0) { chord_detect_note_off(note); return; }
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_held[note] = true;
    recompute_locked();
    xSemaphoreGive(s_mtx);
}

void chord_detect_note_off(uint8_t note)
{
    if (note >= CHORDEX_NOTE_COUNT) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_held[note] = false;
    recompute_locked();
    xSemaphoreGive(s_mtx);
}

void chord_detect_all_off(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    memset(s_held, 0, sizeof(s_held));
    recompute_locked();
    xSemaphoreGive(s_mtx);
}

void chord_detect_get(chord_result_t *out)
{
    if (!out) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *out = s_last;
    xSemaphoreGive(s_mtx);
}

const char *chord_detect_pitch_name(uint8_t pc)
{
    return PITCH_NAMES[pc % 12];
}
