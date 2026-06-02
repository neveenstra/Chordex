#include "lvgl_ui.h"
#include "chord_detect.h"

#include <string.h>
#include <stdio.h>

// Logo uses the built-in Montserrat 28 — Comfortaa was deliberately a
// glyph-subset font (Tick-Tac only ever needed t/i/c/k/a) so it can't
// render arbitrary text on this device.
#define LOGO_FONT (&lv_font_montserrat_28)

// Vaporwave palette — kept in sync with the Tick-Tac UI so the family looks
// consistent across devices.
#define COLOR_BG       lv_color_make(10,  0,  25)
#define COLOR_PANEL    lv_color_make(20,  5,  45)
#define COLOR_PINK     lv_color_make(255, 50, 200)
#define COLOR_CYAN     lv_color_make(0,  220, 255)
#define COLOR_TEXT     lv_color_make(240, 240, 255)
#define COLOR_DIM      lv_color_make(120, 100, 160)
#define COLOR_GREEN    lv_color_make(0,  255, 150)
#define COLOR_RED      lv_color_make(255,  60, 100)
#define COLOR_PILL_OFF lv_color_make(15,   5,  35)

// ---- handles updated at runtime --------------------------------------------

static lv_obj_t *chord_label;
static lv_obj_t *notes_label;
static lv_obj_t *pill_usb;
static lv_obj_t *pill_usb_label;
static lv_obj_t *pill_din;
static lv_obj_t *pill_din_label;
static lv_obj_t *pc_dots[12];
static lv_obj_t *din_debug_label;

static bool s_connected[2] = { false, false };
static char s_last_chord[24];
static char s_last_notes[64];
static uint16_t s_last_pcs = 0xFFFF;

// ---- helpers ----------------------------------------------------------------

static void make_glowline(lv_obj_t *parent, int y, lv_color_t color)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_pos(line, 0, y);
    lv_obj_set_size(line, lv_obj_get_width(parent), 2);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(line,     color,        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(line,       LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0,            LV_PART_MAIN);
    lv_obj_set_style_radius(line,       0,            LV_PART_MAIN);
    lv_obj_set_style_shadow_color(line, color,        LV_PART_MAIN);
    lv_obj_set_style_shadow_width(line, 10,           LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(line,   LV_OPA_80,    LV_PART_MAIN);
}

static lv_obj_t *make_pill(lv_obj_t *parent, const char *text,
                           lv_coord_t x, lv_coord_t y, lv_color_t accent,
                           lv_obj_t **out_label)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, 44, 18);
    lv_obj_set_pos(pill, x, y);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(pill,     COLOR_PILL_OFF,    LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pill,       LV_OPA_COVER,      LV_PART_MAIN);
    lv_obj_set_style_border_color(pill, accent,            LV_PART_MAIN);
    lv_obj_set_style_border_width(pill, 1,                 LV_PART_MAIN);
    lv_obj_set_style_border_opa(pill,   LV_OPA_COVER,      LV_PART_MAIN);
    lv_obj_set_style_radius(pill,       LV_RADIUS_CIRCLE,  LV_PART_MAIN);
    lv_obj_set_style_shadow_color(pill, accent,            LV_PART_MAIN);
    lv_obj_set_style_shadow_width(pill, 8,                 LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(pill,   LV_OPA_30,         LV_PART_MAIN);
    lv_obj_set_style_pad_all(pill,      0,                 LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(pill);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl,  &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_DIM,              LV_PART_MAIN);
    lv_obj_center(lbl);

    if (out_label) *out_label = lbl;
    return pill;
}

static void apply_pill_state(lv_obj_t *pill, lv_obj_t *label, bool on, lv_color_t accent)
{
    if (on) {
        lv_obj_set_style_bg_color(pill,     accent,        LV_PART_MAIN);
        lv_obj_set_style_text_color(label,  COLOR_BG,      LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(pill,   LV_OPA_80,     LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(pill,     COLOR_PILL_OFF, LV_PART_MAIN);
        lv_obj_set_style_text_color(label,  COLOR_DIM,      LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(pill,   LV_OPA_30,      LV_PART_MAIN);
    }
}

// ---- public API -------------------------------------------------------------

void lvgl_ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, COLOR_BG,     LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr,   LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr,  0,            LV_PART_MAIN);

    // Root container — 320 wide × 172 tall, landscape. Layout:
    //   y =  0.. 28   header (logo left, USB/DIN pills right)
    //   y = 28.. 30   cyan glowline
    //   y = 30..136   chord readout (huge, centered)
    //   y =136..138   pink glowline
    //   y =138..172   bottom bar — held-note list above pitch-class strip
    lv_obj_t *root = lv_obj_create(scr);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, 320, 172);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root,     COLOR_BG,     LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root,       LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0,            LV_PART_MAIN);
    lv_obj_set_style_pad_all(root,      0,            LV_PART_MAIN);
    lv_obj_set_style_radius(root,       0,            LV_PART_MAIN);

    make_glowline(root,  28, COLOR_CYAN);
    make_glowline(root, 136, COLOR_PINK);

    // Header logo — single word "chordex" in cyan, matching the Tick-Tac
    // family's "tick tac" logo color.
    lv_obj_t *logo = lv_label_create(root);
    lv_label_set_text(logo, "chordex");
    lv_obj_set_style_text_font(logo,  LOGO_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(logo, COLOR_CYAN, LV_PART_MAIN);
    lv_obj_set_pos(logo, 12, 0);

    // Source pills, top right
    pill_usb = make_pill(root, "USB", 320 - 12 - 44,      5, COLOR_CYAN,  &pill_usb_label);
    pill_din = make_pill(root, "DIN", 320 - 12 - 44 - 50, 5, COLOR_GREEN, &pill_din_label);

    // Big chord readout
    chord_label = lv_label_create(root);
    lv_label_set_text(chord_label, "-");
    lv_obj_set_style_text_font(chord_label,  &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(chord_label, COLOR_TEXT,             LV_PART_MAIN);
    lv_obj_set_style_text_align(chord_label, LV_TEXT_ALIGN_CENTER,   LV_PART_MAIN);
    lv_obj_set_width(chord_label, 320);
    lv_obj_align(chord_label, LV_ALIGN_TOP_MID, 0, 50);

    // Held-note list — small, dim, under the chord
    notes_label = lv_label_create(root);
    lv_label_set_text(notes_label, "");
    lv_obj_set_style_text_font(notes_label,  &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(notes_label, COLOR_DIM,              LV_PART_MAIN);
    lv_obj_set_style_text_align(notes_label, LV_TEXT_ALIGN_CENTER,   LV_PART_MAIN);
    lv_obj_set_width(notes_label, 320);
    lv_obj_align(notes_label, LV_ALIGN_TOP_MID, 0, 108);

    // Pitch-class dots — 12 across the bottom strip
    int dot_w  = 16;
    int dot_h  = 8;
    int gap    = 4;
    int total  = 12 * dot_w + 11 * gap;
    int x0     = (320 - total) / 2;
    int y0     = 152;
    for (int i = 0; i < 12; i++) {
        lv_obj_t *d = lv_obj_create(root);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, dot_w, dot_h);
        lv_obj_set_pos(d, x0 + i * (dot_w + gap), y0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(d,   3,                LV_PART_MAIN);
        lv_obj_set_style_bg_color(d, COLOR_PILL_OFF,   LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d,   LV_OPA_COVER,     LV_PART_MAIN);
        lv_obj_set_style_border_color(d, COLOR_DIM,    LV_PART_MAIN);
        lv_obj_set_style_border_width(d, 1,            LV_PART_MAIN);
        pc_dots[i] = d;
    }

    // Temp DIN probe — tiny dim text in the bottom-left. Stays at "din 0" if
    // nothing reaches GPIO 7.
    din_debug_label = lv_label_create(root);
    lv_obj_set_style_text_font(din_debug_label,  &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(din_debug_label, COLOR_DIM,              LV_PART_MAIN);
    lv_label_set_text(din_debug_label, "din 0");
    lv_obj_align(din_debug_label, LV_ALIGN_TOP_LEFT, 4, 142);

    apply_pill_state(pill_usb, pill_usb_label, false, COLOR_CYAN);
    apply_pill_state(pill_din, pill_din_label, false, COLOR_GREEN);

    s_last_chord[0] = '\0';
    s_last_notes[0] = '\0';
    s_last_pcs      = 0xFFFF;
}

void lvgl_ui_refresh(void)
{
    chord_result_t r;
    chord_detect_get(&r);

    if (strncmp(r.name, s_last_chord, sizeof(s_last_chord)) != 0) {
        lv_label_set_text(chord_label, r.name);
        // Major/strong matches in white; non-chord aggregates in pink.
        lv_obj_set_style_text_color(chord_label,
                                    r.is_chord ? COLOR_TEXT : COLOR_PINK,
                                    LV_PART_MAIN);
        strncpy(s_last_chord, r.name, sizeof(s_last_chord) - 1);
        s_last_chord[sizeof(s_last_chord) - 1] = '\0';
    }

    // Build a notes list "C4 - E4 - G4". buf MUST be zeroed first — when
    // note_count is 0 the loop doesn't run and an uninitialized buf would
    // leak stack garbage onto the screen ("7@" etc.).
    char  buf[64];
    buf[0] = '\0';
    char *p   = buf;
    char *end = buf + sizeof(buf);
    for (uint8_t i = 0; i < r.note_count && p < end; i++) {
        uint8_t n = r.notes[i];
        int oct  = (n / 12) - 1;
        int w = snprintf(p, end - p, "%s%s%d",
                         (i == 0) ? "" : " - ",
                         chord_detect_pitch_name(n), oct);
        if (w < 0 || w >= end - p) break;
        p += w;
    }
    if (strncmp(buf, s_last_notes, sizeof(s_last_notes)) != 0) {
        lv_label_set_text(notes_label, buf);
        strncpy(s_last_notes, buf, sizeof(s_last_notes) - 1);
        s_last_notes[sizeof(s_last_notes) - 1] = '\0';
    }

    if (r.pitch_classes != s_last_pcs) {
        for (int i = 0; i < 12; i++) {
            bool on = (r.pitch_classes >> i) & 1;
            lv_obj_set_style_bg_color(pc_dots[i],
                                      on ? COLOR_CYAN : COLOR_PILL_OFF,
                                      LV_PART_MAIN);
            lv_obj_set_style_border_color(pc_dots[i],
                                          on ? COLOR_CYAN : COLOR_DIM,
                                          LV_PART_MAIN);
        }
        s_last_pcs = r.pitch_classes;
    }
}

void lvgl_ui_set_source_connected(int source, bool connected)
{
    if (source < 0 || source > 1) return;
    if (s_connected[source] == connected) return;
    s_connected[source] = connected;
    if (source == 0) {
        apply_pill_state(pill_usb, pill_usb_label, connected, COLOR_CYAN);
    } else {
        apply_pill_state(pill_din, pill_din_label, connected, COLOR_GREEN);
    }
}

void lvgl_ui_pulse_source(int source)
{
    // The pill brightens momentarily on each note-on by jacking the shadow.
    lv_obj_t *pill = (source == 0) ? pill_usb : pill_din;
    if (!pill) return;
    lv_obj_set_style_shadow_width(pill, 18, LV_PART_MAIN);
}

void lvgl_ui_set_dimmed(bool dimmed)
{
    (void)dimmed;
    // Reserved hook — the idle-dim task in main.c can ramp the backlight,
    // but the UI itself doesn't need to change layout when dimmed.
}

void lvgl_ui_set_din_debug(uint32_t count, uint8_t last_byte)
{
    if (!din_debug_label) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "din %lu  %02X",
             (unsigned long)count, last_byte);
    lv_label_set_text(din_debug_label, buf);
}

