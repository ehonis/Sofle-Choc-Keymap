/* Copyright 2023 Brian Low — default header retained where applicable.
 * Key placements mirror the user's VIA/web config screenshots (layers 0 & 1).
 */
#include QMK_KEYBOARD_H

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h"
#endif

#ifdef SPLIT_KEYBOARD
#    include "transactions.h"
#endif

#include <string.h>

/*
 * Layer order matters: nav overlays MUST be the highest layer numbers.
 * QMK resolves keys from the highest active layer downward.
 *
 * _NAV_SYM and _NAV_SYM_WIN are separate so Windows can use LCTL (not LALT) for
 * word/line navigation while Mac/Linux keep LALT; both share F1–F10 on the left
 * and right-hand I/J/K/L arrows.
 *
 * Stack examples:
 *   Mac typing:     layer 0
 *   Mac + nav:      layers 0 + 2  → highest 2 (_NAV_SYM)
 *   Win typing:     layer 1
 *   Win + nav:      layers 1 + 3  → highest 3 (_NAV_SYM_WIN)
 */
enum sofle_layers { _BASE = 0, _BASE_WIN = 1, _NAV_SYM = 2, _NAV_SYM_WIN = 3 };

/*
 * Nav-layer keys where raw QMK aliases misbehave (always emit the shifted char).
 *
 * PLU_EQ  — `=` unshifted, `+` with Shift  (KC_PLUS / KC_EQL)
 * LBR_BRC — `[` unshifted, `{` with Shift  (KC_LCBR is Shift+LBRC only)
 * RBR_BRC — `]` unshifted, `}` with Shift  (KC_RCBR is Shift+RBRC only)
 * ZOOM_IN — browser/page zoom in using the active profile's native shortcut:
 *           Command+Plus on macOS or Ctrl+Plus on Windows.
 * ZOOM_OUT — the inverse action used when the nav key is held:
 *            Command+Minus on macOS or Ctrl+Minus on Windows.
 */
enum custom_keycodes { PLU_EQ = SAFE_RANGE, LBR_BRC, RBR_BRC, ZOOM_IN, ZOOM_OUT };

/*
 * Profile = is the _BASE_WIN bit set in the (split-synced) layer_state.
 *
 *   Mac:        layer_state = 0b001       (default layer 0 only)
 *   Win:        layer_state = 0b011       (default 0 + _BASE_WIN toggled on)
 *   Mac + nav:  layer_state = 0b101  (+ MO(_NAV_SYM))
 *   Win + nav:  layer_state = 0b1011 (+ MO(_NAV_SYM_WIN))
 *
 * Because SPLIT_LAYER_STATE_ENABLE is set in config.h, both halves see the same
 * layer_state, so both OLEDs render the same icon.
 */
static bool is_windows_profile_active(void) {
    return (layer_state & (1UL << _BASE_WIN)) != 0;
}

/*
 * Split boards: only the USB-connected half is "master" for RGB. The other half's `rgb_matrix_config`
 * is overwritten every transport frame from the master (`PUT_RGB_MATRIX` in split_common). Running
 * EEPROM fixes on the slave does nothing visible because the master keeps pushing its state.
 *
 * Also, `eeconfig_update_rgb_matrix_default()` updates RAM + EEPROM but does not bump `rgb_task_state`,
 * so the running animation can stick until something restarts the effect; `rgb_matrix_mode_noeeprom`
 * forces a fresh STARTING phase.
 */
#ifdef RGB_MATRIX_ENABLE
/*
 * Layer color palette (HSV 0–255). Brightness is still capped by RGB_MATRIX_MAXIMUM_BRIGHTNESS.
 *
 *   Mac base (_BASE)           — neon light blue
 *   Windows base (_BASE_WIN)   — deep amethyst purple
 *   Nav overlays (_NAV_SYM*)   — white (in-between / held-nav cue)
 */
static void set_layer_rgb(layer_state_t state) {
    if (!is_keyboard_master()) {
        return;
    }

    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    switch (get_highest_layer(state)) {
        case _NAV_SYM:
        case _NAV_SYM_WIN:
            /* Nav overlay — white so held-nav reads as a clean “in between” state. */
            rgb_matrix_sethsv_noeeprom(0, 0, 255);
            break;
        case _BASE_WIN:
            /* Windows typing profile — deep amethyst purple. */
            rgb_matrix_sethsv_noeeprom(191, 255, 255);
            break;
        case _BASE:
        default:
            /* Mac typing profile — bright neon light blue. */
            rgb_matrix_sethsv_noeeprom(128, 255, 255);
            break;
    }
}
#endif

/*
 * ---------------------------------------------------------------------------
 * Lifetime keypress counter (replaces the old WPM readout)
 * ---------------------------------------------------------------------------
 *
 * Counts every physical key press - not releases, and not encoder turns, which
 * also arrive at process_record_user() but as ENCODER_*_EVENT rather than
 * KEY_EVENT. Both OLEDs show the running total as 437 / 11.3k / 1.5m.
 *
 * Persistence: the live total lives in RAM and is flushed to user EEPROM only
 * periodically. Writing on every keystroke would destroy the chip - EEPROM cells
 * endure on the order of 100k writes, which at typing speed is a matter of hours.
 * Instead we flush when either:
 *   - CHARCOUNT_FLUSH_CHARS presses have accumulated since the last write, or
 *   - typing has been idle for CHARCOUNT_IDLE_MS with at least
 *     CHARCOUNT_IDLE_MIN presses pending.
 *
 * Net effect: a few dozen writes a day (many years of EEPROM life). The most a
 * hard unplug can cost you is the presses since the last flush - and since the
 * idle flush fires 30 s after you stop typing, in practice that is usually zero.
 */
#define CHARCOUNT_FLUSH_CHARS 2000
#define CHARCOUNT_IDLE_MS 30000
#define CHARCOUNT_IDLE_MIN 20

static uint32_t charcount_total    = 0; /* live total rendered on both OLEDs */
static uint32_t charcount_saved    = 0; /* last total written to EEPROM      */
static uint32_t charcount_last_key = 0; /* timer for the idle flush          */

/*
 * QMK calls this when the user EEPROM block is missing or stale (a fresh board,
 * or after an EEPROM clear). Seed the stored total at zero.
 */
void eeconfig_init_user_datablock(void) {
    uint32_t zero = 0;
    eeconfig_update_user_datablock(&zero, 0, sizeof(zero));
}

static void charcount_load(void) {
    if (eeconfig_is_user_datablock_valid()) {
        eeconfig_read_user_datablock(&charcount_total, 0, sizeof(charcount_total));
    } else {
        charcount_total = 0;
        eeconfig_update_user_datablock(&charcount_total, 0, sizeof(charcount_total));
    }
    charcount_saved = charcount_total;
}

static void charcount_flush(void) {
    if (charcount_total == charcount_saved) {
        return;
    }
    eeconfig_update_user_datablock(&charcount_total, 0, sizeof(charcount_total));
    charcount_saved = charcount_total;
}

static void charcount_maybe_flush(void) {
    uint32_t pending = charcount_total - charcount_saved;
    if (pending == 0) {
        return;
    }
    if (pending >= CHARCOUNT_FLUSH_CHARS || (pending >= CHARCOUNT_IDLE_MIN && timer_elapsed32(charcount_last_key) >= CHARCOUNT_IDLE_MS)) {
        charcount_flush();
    }
}

#ifdef SPLIT_KEYBOARD
/*
 * Slave side of the CHARCOUNT_SYNC transaction: only the USB master runs
 * process_record_user(), so the slave has no way to count. It just adopts
 * whatever total the master pushes, which keeps both OLEDs showing the same number.
 */
static void charcount_sync_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(uint32_t)) {
        memcpy(&charcount_total, in_data, sizeof(uint32_t));
    }
}
#endif

void keyboard_post_init_user(void) {
#ifdef SPLIT_KEYBOARD
    /* Both halves register: the slave needs the receive callback installed. */
    transaction_register_rpc(CHARCOUNT_SYNC, charcount_sync_handler);
#endif

    if (is_keyboard_master()) {
        charcount_load();
    }

#ifdef RGB_MATRIX_ENABLE
    if (!is_keyboard_master()) {
        return;
    }
    eeconfig_update_rgb_matrix_default();
    set_layer_rgb(layer_state);
#endif
}

void housekeeping_task_user(void) {
    if (!is_keyboard_master()) {
        return;
    }

    charcount_maybe_flush();

#ifdef SPLIT_KEYBOARD
    /* Push the total to the slave at most ~4x a second, and only when it moved. */
    static uint32_t last_push_at    = 0;
    static uint32_t last_push_value = UINT32_MAX;
    if (charcount_total != last_push_value && timer_elapsed32(last_push_at) > 250) {
        if (transaction_rpc_send(CHARCOUNT_SYNC, sizeof(charcount_total), &charcount_total)) {
            last_push_value = charcount_total;
        }
        last_push_at = timer_read32();
    }
#endif
}

layer_state_t layer_state_set_user(layer_state_t state) {
#ifdef RGB_MATRIX_ENABLE
    set_layer_rgb(state);
#endif
    return state;
}

/*
 * Profile switching is just `TG(_BASE_WIN)` on the nav layer — see keymap below.
 * TG flips the _BASE_WIN layer bit on/off, which:
 *   - Mac (off) → Windows (on): layer_state gets bit 1 set, _BASE_WIN keys win over _BASE
 *   - Windows (on) → Mac (off): bit 1 cleared, _BASE keys are top of stack again
 * No custom keycode handler needed — QMK + split_layer_state do the rest.
 */

#ifdef OLED_ENABLE
/*
 * Landscape OLED view: one large centred number, the lifetime keypress total.
 *
 * Design goal:
 * - no labels (layer text, modifiers, etc.), just the number plus the OS badge
 * - abbreviate once it gets long: 437 -> 11.3k -> 1.5m
 *
 * Implementation note:
 * We draw custom 7-segment style glyphs with oled_write_pixel so we are not limited
 * by the tiny built-in 6x8 text font, and without pulling in heavy bitmap assets.
 *
 * Orientation (128 w x 32 h). The readout runs along the long axis of the panel
 * because that is the only way a 5-glyph number (11.3k) fits at a legible size -
 * the 32 px short axis only ever had room for two 13 px digits.
 *
 * If the display reads upside down on your build, change this to OLED_ROTATION_0.
 */
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_180;
}

/*
 * Draw one thick segment rectangle (filled) for a 7-segment digit.
 * x,y are top-left pixel coordinates in the rotated (landscape) OLED space.
 */
static void draw_seg(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint8_t yy = 0; yy < h; yy++) {
        for (uint8_t xx = 0; xx < w; xx++) {
            oled_write_pixel((uint8_t)(x + xx), (uint8_t)(y + yy), true);
        }
    }
}

/*
 * Draw a large 7-segment digit.
 *
 * Segment map:
 *   ---a---
 *  |       |
 *  f       b
 *  |---g---|
 *  e       c
 *  |       |
 *   ---d---
 *
 * Sized for the landscape layout: GLYPH_W x GLYPH_H (17 x 29) with a 3 px stroke,
 * which fits four glyphs plus a decimal point alongside the 16 px profile badge.
 */
/*
 * 16×16 profile icons in flash (PROGMEM). One uint16_t per row; bit 15 (MSB) is the
 * leftmost pixel (col 0), bit 0 (LSB) is the rightmost (col 15).
 *
 * Apple logo (each '.' is off, 'X' is on):
 *   col:    0123456789012345
 *   R0      ................   blank top margin
 *   R1      ..........X.....   leaf tip (single pixel poking up)
 *   R2      .........XXX....   leaf curl
 *   R3      ........XX......   leaf attaching to body
 *   R4      ....XXXXX.XX....   top of body with bite cutout on the right
 *   R5      ...XXXXXXX.XX...   bite narrowing
 *   R6      ..XXXXXXXXX.XX..   bite almost closed
 *   R7      ..XXXXXXXXXXXXX.   widest row (bite finished)
 *   R8-R11  ..XXXXXXXXXXXXX.   solid body, 13 px wide
 *   R12     ...XXXXXXXXXXX..
 *   R13     ....XXXXXXXXX...
 *   R14     ....XXX...XXX...   classic two-foot split at bottom
 *   R15     ................   blank bottom margin
 *
 * Mapping each lit column to a bit:  bit = 15 - col
 *   e.g. R1 col 10 -> bit 5  -> 0x0020
 *        R4 cols 4..8 + 10,11 -> bits 11,10,9,8,7 + 5,4 -> 0x0FB0
 */
static const uint16_t PROGMEM icon_mac[16] = {
    0x0000,  /* R0   ................ */
    0x0020,  /* R1   ..........X..... */
    0x0070,  /* R2   .........XXX.... */
    0x00C0,  /* R3   ........XX...... */
    0x0FB0,  /* R4   ....XXXXX.XX.... */
    0x1FD8,  /* R5   ...XXXXXXX.XX... */
    0x3FEC,  /* R6   ..XXXXXXXXX.XX.. */
    0x3FFE,  /* R7   ..XXXXXXXXXXXXX. */
    0x3FFE,  /* R8   ..XXXXXXXXXXXXX. */
    0x3FFE,  /* R9   ..XXXXXXXXXXXXX. */
    0x3FFE,  /* R10  ..XXXXXXXXXXXXX. */
    0x3FFE,  /* R11  ..XXXXXXXXXXXXX. */
    0x1FFC,  /* R12  ...XXXXXXXXXXX.. */
    0x0FF8,  /* R13  ....XXXXXXXXX... */
    0x0E38,  /* R14  ....XXX...XXX... */
    0x0000,  /* R15  ................ */
};

/*
 * Windows logo — four equal square panes in a 2×2 grid, separated by 1-pixel gaps.
 * Pane size: 5 cols × 4 rows each.
 *   Left panes:  cols 1-5
 *   Col gap:     col 6
 *   Right panes: cols 7-11
 *   Top panes:   rows 3-6   (0x7DF0 = 0111 1101 1111 0000)
 *   Row gap:     row 7
 *   Bottom panes:rows 8-11
 */
static const uint16_t PROGMEM icon_win[16] = {
    0x0000,  /* R0  empty */
    0x0000,  /* R1  empty */
    0x0000,  /* R2  empty */
    0x7DF0,  /* R3  top panes    .XXXXX.XXXXX.... */
    0x7DF0,  /* R4  top panes                     */
    0x7DF0,  /* R5  top panes                     */
    0x7DF0,  /* R6  top panes                     */
    0x0000,  /* R7  row gap                       */
    0x7DF0,  /* R8  bottom panes .XXXXX.XXXXX.... */
    0x7DF0,  /* R9  bottom panes                  */
    0x7DF0,  /* R10 bottom panes                  */
    0x7DF0,  /* R11 bottom panes                  */
    0x0000,  /* R12 empty */
    0x0000,  /* R13 empty */
    0x0000,  /* R14 empty */
    0x0000,  /* R15 empty */
};

/*
 * Blit one 16×16 icon to the OLED framebuffer.
 * `ox`,`oy` = top-left corner in the rotated portrait coordinate space (32 w × 128 h).
 * Bit 15 of each row word maps to the leftmost pixel (col 0 offset from ox).
 */
static void draw_profile_icon(uint8_t ox, uint8_t oy, const uint16_t *icon) {
    for (uint8_t row = 0; row < 16; row++) {
        uint16_t bits = pgm_read_word(&icon[row]);
        for (uint8_t col = 0; col < 16; col++) {
            if (bits & (0x8000 >> col)) {
                oled_write_pixel((uint8_t)(ox + col), (uint8_t)(oy + row), true);
            }
        }
    }
}

/* Glyph cell geometry for the landscape readout. */
#define GLYPH_W 17     /* digit / letter cell width          */
#define GLYPH_H 29     /* digit / letter cell height         */
#define GLYPH_DOT_W 6  /* the decimal point gets a slim cell */
#define GLYPH_GAP 4    /* space between adjacent cells       */

/*
 * The k / m suffix is a unit marker, not another digit, so it is drawn at roughly
 * half height and sat on the baseline. At full digit size it reads as a third
 * numeral and makes the number look lopsided.
 */
#define GLYPH_SUFFIX_W 10
#define GLYPH_SUFFIX_H 15

/* Glyph codes: 0-9 are literal digits, these are the extras. */
#define GLYPH_DOT 10
#define GLYPH_K 11
#define GLYPH_M 12

static void draw_big_digit(uint8_t x, uint8_t y, uint8_t digit) {
    static const uint8_t segmask[10] = {
        /*0*/ 0b00111111, /* a b c d e f */
        /*1*/ 0b00000110, /* b c */
        /*2*/ 0b01011011, /* a b d e g */
        /*3*/ 0b01001111, /* a b c d g */
        /*4*/ 0b01100110, /* b c f g */
        /*5*/ 0b01101101, /* a c d f g */
        /*6*/ 0b01111101, /* a c d e f g */
        /*7*/ 0b00000111, /* a b c */
        /*8*/ 0b01111111, /* a b c d e f g */
        /*9*/ 0b01101111  /* a b c d f g */
    };

    if (digit > 9) {
        digit = 0;
    }

    uint8_t m = segmask[digit];

    /* Horizontal segments: 11x3 rectangles at top / middle / bottom. */
    if (m & (1 << 0)) draw_seg((uint8_t)(x + 3), (uint8_t)(y + 0), 11, 3);   /* a */
    if (m & (1 << 6)) draw_seg((uint8_t)(x + 3), (uint8_t)(y + 13), 11, 3);  /* g */
    if (m & (1 << 3)) draw_seg((uint8_t)(x + 3), (uint8_t)(y + 26), 11, 3);  /* d */

    /* Vertical segments: 3x10 rectangles filling the gaps between them. */
    if (m & (1 << 5)) draw_seg((uint8_t)(x + 0), (uint8_t)(y + 3), 3, 10);   /* f */
    if (m & (1 << 4)) draw_seg((uint8_t)(x + 0), (uint8_t)(y + 16), 3, 10);  /* e */
    if (m & (1 << 1)) draw_seg((uint8_t)(x + 14), (uint8_t)(y + 3), 3, 10);  /* b */
    if (m & (1 << 2)) draw_seg((uint8_t)(x + 14), (uint8_t)(y + 16), 3, 10); /* c */
}

/*
 * The k and m suffixes cannot be faked with 7-segment masks, so they are drawn
 * directly. Both use the same 17x29 cell so they line up with the digits beside them.
 */
/* Both letters fill a GLYPH_SUFFIX_W x GLYPH_SUFFIX_H cell with a 2 px stroke. */
static void draw_letter_k(uint8_t x, uint8_t y) {
    draw_seg(x, y, 2, GLYPH_SUFFIX_H); /* upright stem */
    /* Two staircase diagonals meeting the stem at mid height. */
    for (uint8_t i = 0; i < 7; i++) {
        draw_seg((uint8_t)(x + 7 - i), (uint8_t)(y + i), 2, 2);     /* upper arm */
        draw_seg((uint8_t)(x + 1 + i), (uint8_t)(y + 7 + i), 2, 2); /* lower arm */
    }
}

static void draw_letter_m(uint8_t x, uint8_t y) {
    draw_seg(x, y, GLYPH_SUFFIX_W, 2);                  /* top bar       */
    draw_seg(x, y, 2, GLYPH_SUFFIX_H);                  /* left upright  */
    draw_seg((uint8_t)(x + 8), y, 2, GLYPH_SUFFIX_H);   /* right upright */
    draw_seg((uint8_t)(x + 4), (uint8_t)(y + 2), 2, 6); /* centre leg    */
}

static uint8_t glyph_width(uint8_t code) {
    switch (code) {
        case GLYPH_DOT:
            return GLYPH_DOT_W;
        case GLYPH_K:
        case GLYPH_M:
            return GLYPH_SUFFIX_W;
        default:
            return GLYPH_W;
    }
}

static void draw_glyph(uint8_t x, uint8_t y, uint8_t code) {
    switch (code) {
        case GLYPH_DOT:
            /* Sits on the baseline so it reads as a decimal point, not a colon. */
            draw_seg((uint8_t)(x + 1), (uint8_t)(y + GLYPH_H - 3), 4, 3);
            break;
        case GLYPH_K:
            /* Bottom-aligned so the suffix sits on the same baseline as the digits. */
            draw_letter_k(x, (uint8_t)(y + GLYPH_H - GLYPH_SUFFIX_H));
            break;
        case GLYPH_M:
            draw_letter_m(x, (uint8_t)(y + GLYPH_H - GLYPH_SUFFIX_H));
            break;
        default:
            draw_big_digit(x, y, code);
            break;
    }
}

/*
 * Format the running total into at most 5 glyph cells:
 *   0 - 999        ->  437
 *   1k - 999k      ->  1.5k   11.3k   123k
 *   1m and above   ->  1.5m   11.3m   123m
 *
 * Three significant digits leave no room for a decimal, so 123456 renders as
 * 123k rather than 123.4k. Returns the number of glyphs written to out[].
 */
static uint8_t charcount_format(uint32_t v, uint8_t *out) {
    uint8_t n = 0;

    if (v < 1000UL) {
        if (v >= 100UL) out[n++] = (uint8_t)(v / 100UL % 10UL);
        if (v >= 10UL)  out[n++] = (uint8_t)(v / 10UL % 10UL);
        out[n++] = (uint8_t)(v % 10UL);
        return n;
    }

    uint8_t  suffix;
    uint32_t tenths; /* the value rescaled into tenths of the suffix unit */
    if (v < 1000000UL) {
        suffix = GLYPH_K;
        tenths = v / 100UL;
    } else {
        suffix = GLYPH_M;
        tenths = v / 100000UL;
    }
    /* Clamp so a uint32 near its ceiling still fits the layout (tops out at 999m). */
    if (tenths > 9999UL) {
        tenths = 9999UL;
    }

    uint32_t whole = tenths / 10UL;
    uint8_t  frac  = (uint8_t)(tenths % 10UL);

    if (whole >= 100UL) {
        out[n++] = (uint8_t)(whole / 100UL % 10UL);
        out[n++] = (uint8_t)(whole / 10UL % 10UL);
        out[n++] = (uint8_t)(whole % 10UL);
    } else {
        if (whole >= 10UL) out[n++] = (uint8_t)(whole / 10UL % 10UL);
        out[n++] = (uint8_t)(whole % 10UL);
        out[n++] = GLYPH_DOT;
        out[n++] = frac;
    }
    out[n++] = suffix;
    return n;
}

bool oled_task_user(void) {
    oled_clear();

    uint8_t glyphs[6];
    uint8_t n = charcount_format(charcount_total, glyphs);

    uint16_t total_w = (uint16_t)GLYPH_GAP * (n - 1);
    for (uint8_t i = 0; i < n; i++) {
        total_w = (uint16_t)(total_w + glyph_width(glyphs[i]));
    }

    /*
     * Landscape canvas: 128 w x 32 h.
     * The profile badge keeps its 16x16 cell flush to the right edge; the number is
     * centred in whatever width is left of it.
     */
    const uint8_t icon_x  = 128 - 16;
    const uint8_t icon_y  = (32 - 16) / 2;
    const uint8_t field_w = icon_x - GLYPH_GAP;

    uint8_t       x = (total_w < field_w) ? (uint8_t)((field_w - total_w) / 2) : 0;
    const uint8_t y = (32 - GLYPH_H) / 2;

    for (uint8_t i = 0; i < n; i++) {
        draw_glyph(x, y, glyphs[i]);
        x = (uint8_t)(x + glyph_width(glyphs[i]) + GLYPH_GAP);
    }

    if (is_windows_profile_active()) {
        draw_profile_icon(icon_x, icon_y, icon_win);
    } else {
        draw_profile_icon(icon_x, icon_y, icon_mac);
    }

    return false;
}

#endif /* OLED_ENABLE */

/*
 * Tap a US-layout key as unshifted or Shift+key, stripping held Shift first so
 * layer/thumb shift does not force the shifted symbol every time (same fix as PLU_EQ).
 */
static void tap_us_key_shifted(uint16_t keycode) {
    if (mod_config(get_mods()) & MOD_MASK_SHIFT) {
        uint8_t mods = get_mods();
        del_mods(MOD_MASK_SHIFT);
        tap_code16(S(keycode));
        set_mods(mods);
    } else {
        tap_code16(keycode);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        return true;
    }

    /*
     * Count real key presses only. With ENCODER_MAP_ENABLE the knobs dispatch through
     * here too, but as ENCODER_CW/CCW_EVENT - IS_KEYEVENT() filters those out so
     * spinning the volume knob does not inflate the total.
     */
    if (IS_KEYEVENT(record->event)) {
        charcount_total++;
        charcount_last_key = timer_read32();
    }

    switch (keycode) {
        case PLU_EQ:
            tap_us_key_shifted(KC_EQL);
            return false;
        case LBR_BRC:
            tap_us_key_shifted(KC_LBRC);
            return false;
        case RBR_BRC:
            tap_us_key_shifted(KC_RBRC);
            return false;
        case ZOOM_IN:
            /*
             * The encoder's push switch is a normal matrix key. Send one complete
             * zoom shortcut per press instead of holding the modifiers while the
             * knob remains depressed. The Windows base-layer bit is split-synced,
             * so this also behaves correctly when the left half is not USB master.
             */
            if (is_windows_profile_active()) {
                tap_code16(C(KC_PLUS));
            } else {
                tap_code16(G(KC_PLUS));
            }
            return false;
        case ZOOM_OUT:
            /*
             * The nav overlay replaces ZOOM_IN with ZOOM_OUT at the same physical
             * encoder-switch position. Minus is already an unshifted key, so only
             * the operating system's Command/Ctrl modifier needs to be added.
             */
            if (is_windows_profile_active()) {
                tap_code16(C(KC_MINS));
            } else {
                tap_code16(G(KC_MINS));
            }
            return false;
    }

    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/*
 * Layer 0 (base) - from your screenshots
 * LEFT
 * Row0: Esc  1-5
 * Row1: Tab  Q W E R T
 * Row2: LCtl A S D F G           (Lcrl on screenshot = KC_LCTL)
 * Row3: LSft Z X C V B           inner tall: mute
 * Thumbs horizontal (outer→inner): Cmd  Alt  Ctrl  Alt  ·  tall Space (matrix order below)
 *
 * BETWEEN HALVES row4: left encoder press = page zoom in;
 *                      right encoder press = media play/pause
 *
 * RIGHT
 * Row0 (number row above home): 6-0 + KC_GRV (tilde/~ key legend)
 * Row1: Y U I O P KC_BSLS
 * Row3: H J K L ; '
 * Row4: VOLU vertical inner | N M , . / RSft
 * Thumbs tall Enter · BSPC MO(1) ___ RCTL
 * - `MO(_NAV_SYM)` = hold for layer 1, release to return to base
 * - no toggles (`TG`/`TO`) so accidental persistent layer lock is removed
 */
[_BASE] = LAYOUT(
    KC_ESC,   KC_1, KC_2, KC_3, KC_4, KC_5,                                KC_6, KC_7, KC_8,    KC_9, KC_0,    KC_GRV,
    KC_TAB,   KC_Q, KC_W, KC_E, KC_R, KC_T,                                KC_Y, KC_U, KC_I,    KC_O, KC_P,    KC_BSLS,
    KC_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G,                                KC_H, KC_J, KC_K,    KC_L, KC_SCLN, KC_QUOT,
    KC_LSFT,  KC_Z, KC_X, KC_C, KC_V, KC_B, ZOOM_IN, KC_MPLY,              KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
              KC_LCTL, KC_LALT, KC_LGUI, MO(_NAV_SYM), KC_SPC,                  KC_ENT, KC_BSPC, KC_LALT, KC_LGUI, KC_RCTL
),

/*
 * Windows profile — same as _BASE except left thumb outer/inner swap:
 * Mac thumb order (outer→inner): Ctrl  Alt  Gui  |  Win: Gui  Alt  Ctrl
 * So copy/paste on Windows uses Ctrl on the thumb where Mac used Gui.
 */
[_BASE_WIN] = LAYOUT(
    KC_ESC,   KC_1, KC_2, KC_3, KC_4, KC_5,                                KC_6, KC_7, KC_8,    KC_9, KC_0,    KC_GRV,
    KC_TAB,   KC_Q, KC_W, KC_E, KC_R, KC_T,                                KC_Y, KC_U, KC_I,    KC_O, KC_P,    KC_BSLS,
    KC_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G,                                KC_H, KC_J, KC_K,    KC_L, KC_SCLN, KC_QUOT,
    KC_LSFT,  KC_Z, KC_X, KC_C, KC_V, KC_B, ZOOM_IN, KC_MPLY,              KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
              KC_LGUI, KC_LALT, KC_LCTL, MO(_NAV_SYM_WIN), KC_SPC,                  KC_ENT, KC_BSPC, KC_LALT, KC_LGUI, KC_RCTL
),

/*
 * Mac / Linux nav overlay (_NAV_SYM):
 * - TG(_BASE_WIN) top-left toggles the Windows profile (tap while holding nav).
 * - Pressing the left encoder while nav is held zooms the page out.
 * - F1–F10 on left rows 0–1 (1–5 and Q–T); arrows on right hand only (I/J/K/L).
 * - Homerow LALT on A/S so Option+arrows still work when combined with arrow keys.
 * - -/+ shifted one right to ; and ' (KC_MINS / PLU_EQ).
 * KC_NO everywhere else so accidental keys do nothing.
 */
[_NAV_SYM] = LAYOUT(
    TG(_BASE_WIN), KC_F1, KC_F2, KC_F3, KC_F4, KC_F5,               KC_NO, KC_NO, KC_NO, LBR_BRC, RBR_BRC, KC_NO,
    KC_NO, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10,                      KC_NO, KC_NO, KC_UP,   KC_NO,   KC_NO,   KC_NO,
    KC_LALT, KC_LALT, KC_NO, KC_NO, KC_NO, KC_NO,                   KC_NO, KC_LEFT, KC_DOWN, KC_RGHT, KC_MINS, PLU_EQ,
    KC_LSFT, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, ZOOM_OUT,          KC_MPLY, KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
           KC_NO, KC_NO, KC_LGUI, KC_NO, KC_SPC,                    KC_LALT, KC_BSPC, KC_NO, KC_NO, KC_NO
),

/*
 * Windows nav overlay (_NAV_SYM_WIN):
 * - Same layout as _NAV_SYM, but LCTL replaces LALT so Ctrl+arrows match Windows
 *   word/paragraph navigation (same finger positions as Mac Option+arrows).
 * - Pressing the left encoder while nav is held zooms the page out.
 * - F1–F10 on left rows 0–1 (1–5 and Q–T); F11 on `; Tab → F12.
 * - Arrows on right hand only (I/J/K/L); H → Ctrl+Alt+Del.
 * - -/+ shifted one right to ; and ' (KC_MINS / PLU_EQ).
 * - TG(_BASE_WIN) top-left switches back to Mac/Linux profile.
 */
[_NAV_SYM_WIN] = LAYOUT(
    TG(_BASE_WIN), KC_F1, KC_F2, KC_F3, KC_F4, KC_F5,               KC_NO, KC_NO, KC_NO, LBR_BRC, RBR_BRC, KC_F11,
    KC_TAB, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10,                    C(A(KC_DEL)), KC_NO, KC_UP,   KC_NO,   KC_NO,   KC_F12,
    KC_LCTL, KC_LCTL, KC_NO, KC_NO, KC_NO, KC_NO,                   KC_NO, KC_LEFT, KC_DOWN, KC_RGHT, KC_MINS, PLU_EQ,
    KC_LSFT, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, ZOOM_OUT,          KC_MPLY, KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
           KC_NO, KC_NO, KC_LCTL, KC_NO, KC_SPC,                    KC_LCTL, KC_BSPC, KC_NO, KC_NO, KC_NO
)
};

#if defined(ENCODER_MAP_ENABLE)
/*
 * Encoder 0 is the left knob and sends horizontal mouse-wheel movement. Most
 * browsers, editors, and other scrollable views interpret this as side-to-side
 * scrolling without requiring Shift to be held.
 *
 * Encoder 1 is the right knob and controls system volume.
 * ENCODER_CCW_CW(ccw, cw) lists the counter-clockwise action first. Both pairs
 * intentionally look reversed from the usual logical order because these
 * physical encoders are mounted with the opposite rotational orientation.
 * Every layer repeats the mapping so both knobs remain predictable while nav is held.
 */
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_BASE]        = { ENCODER_CCW_CW(MS_WHLR, MS_WHLL), ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_NAV_SYM]     = { ENCODER_CCW_CW(MS_WHLR, MS_WHLL), ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_NAV_SYM_WIN] = { ENCODER_CCW_CW(MS_WHLR, MS_WHLL), ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_BASE_WIN]    = { ENCODER_CCW_CW(MS_WHLR, MS_WHLL), ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
};
#endif
