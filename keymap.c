/* Copyright 2023 Brian Low — default header retained where applicable.
 * Key placements mirror the user's VIA/web config screenshots (layers 0 & 1).
 */
#include QMK_KEYBOARD_H

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h"
#endif

/*
 * Layer order matters: nav overlays MUST be the highest layer numbers.
 * QMK resolves keys from the highest active layer downward.
 *
 * _NAV_SYM and _NAV_SYM_WIN are separate so Windows can use LCTL (not LALT) for
 * word/line navigation and F1–F10 on the number row while Mac/Linux keep LALT.
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
 */
enum custom_keycodes { PLU_EQ = SAFE_RANGE, LBR_BRC, RBR_BRC };

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
/* Strong layer-specific color cue so you can instantly see base vs nav layer. */
static void set_layer_rgb(layer_state_t state) {
    if (!is_keyboard_master()) {
        return;
    }

    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    switch (get_highest_layer(state)) {
        case _NAV_SYM:
        case _NAV_SYM_WIN:
            /* Nav overlay: cyan so it's visually obvious you're on a nav layer. */
            rgb_matrix_sethsv_noeeprom(128, 255, 180);
            break;
        case _BASE_WIN:
            /* Windows profile base: red. HSV hue 0 + max saturation + max value.
             * Brightness is globally capped by RGB_MATRIX_MAXIMUM_BRIGHTNESS in config.h. */
            rgb_matrix_sethsv_noeeprom(0, 255, 255);
            break;
        case _BASE:
        default:
            /* Mac profile base: white (sat 0 = no color). */
            rgb_matrix_sethsv_noeeprom(0, 0, 255);
            break;
    }
}
#endif

void keyboard_post_init_user(void) {
#ifdef RGB_MATRIX_ENABLE
    if (!is_keyboard_master()) {
        return;
    }
    eeconfig_update_rgb_matrix_default();
    set_layer_rgb(layer_state);
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
 * Portrait OLED view with a large centered numeric speed readout.
 *
 * Design goal:
 * - remove labels ("WPM", layer text, modifiers, etc.)
 * - show only a big number that grows from 0 upward as you type
 * - keep portrait orientation for easier top-down glance reading
 *
 * Implementation note:
 * We draw a custom 7-segment style number with `oled_write_pixel` so we are not
 * limited by the tiny built-in 6x8 text font. This gives us a much larger number
 * without bringing in heavy bitmap assets.
 */
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    /* 270° gives a vertical read direction that is easier to see from above. */
    return OLED_ROTATION_270;
}

/*
 * Draw one thick segment rectangle (filled) for a 7-segment digit.
 * `x`,`y` are top-left pixel coordinates in the rotated (portrait) OLED space.
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
 * Size used here (kept small enough to fit two digits in 32px portrait width):
 * - total width  = 13 px
 * - total height = 23 px
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

    /* Horizontal segments: 9x2 rectangles */
    if (m & (1 << 0)) draw_seg((uint8_t)(x + 2), (uint8_t)(y + 0), 9, 2);   /* a */
    if (m & (1 << 3)) draw_seg((uint8_t)(x + 2), (uint8_t)(y + 21), 9, 2);  /* d */
    if (m & (1 << 6)) draw_seg((uint8_t)(x + 2), (uint8_t)(y + 10), 9, 2);  /* g */

    /* Vertical segments: 2x9 rectangles */
    if (m & (1 << 5)) draw_seg((uint8_t)(x + 0), (uint8_t)(y + 1), 2, 9);   /* f */
    if (m & (1 << 4)) draw_seg((uint8_t)(x + 0), (uint8_t)(y + 12), 2, 9);  /* e */
    if (m & (1 << 1)) draw_seg((uint8_t)(x + 11), (uint8_t)(y + 1), 2, 9);  /* b */
    if (m & (1 << 2)) draw_seg((uint8_t)(x + 11), (uint8_t)(y + 12), 2, 9); /* c */
}

bool oled_task_user(void) {
    oled_clear();

    /*
     * Show the typing speed as a large two-digit number centered on screen.
     * We clamp to 0..99 so the layout stays large and stable.
     */
    uint8_t val = 0;
#ifdef WPM_ENABLE
    val = get_current_wpm();
#endif
    if (val > 99) {
        val = 99;
    }

    uint8_t tens = (uint8_t)(val / 10);
    uint8_t ones = (uint8_t)(val % 10);

    /* Portrait canvas: 32 w × 128 h. Center two 13-px digits with a 2-px gap between them. */
    const uint8_t digit_w = 13;
    const uint8_t gap     = 2;
    const uint8_t total_w = (uint8_t)(digit_w * 2 + gap); /* 28 px total */
    const uint8_t start_x = (uint8_t)((32 - total_w) / 2); /* x = 2, horizontally centered */
    const uint8_t start_y = (uint8_t)((128 - 23) / 2);     /* y = 52, vertically centered */

    /*
     * OS profile badge: flush to the bottom-right corner of the 32×128 canvas.
     * Icon is 16×16 px → top-left at (16, 112) so it sits at pixel columns 16-31
     * and pixel rows 112-127.
     */
    const uint8_t icon_x = 32 - 16; /* = 16 */
    const uint8_t icon_y = 128 - 16; /* = 112 */
    if (is_windows_profile_active()) {
        draw_profile_icon(icon_x, icon_y, icon_win);
    } else {
        draw_profile_icon(icon_x, icon_y, icon_mac);
    }

    /*
     * Keep the leading zero for readability at a glance (e.g. "07").
     * If you prefer blank leading zero, we can conditionally skip tens.
     */
    draw_big_digit(start_x, start_y, tens);
    draw_big_digit((uint8_t)(start_x + digit_w + gap), start_y, ones);

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
 * BETWEEN HALVES row4: mute (left) | vol up (right) — matches Sofle dual-inner slots
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
    KC_LSFT,  KC_Z, KC_X, KC_C, KC_V, KC_B, KC_VOLU, KC_VOLU,              KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
              KC_LCTL, KC_LALT, KC_LGUI, MO(_NAV_SYM), KC_SPC,                  KC_ENT, KC_BSPC, KC_LGUI, KC_LALT, KC_RCTL
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
    KC_LSFT,  KC_Z, KC_X, KC_C, KC_V, KC_B, KC_VOLU, KC_VOLU,              KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
              KC_LGUI, KC_LALT, KC_LCTL, MO(_NAV_SYM_WIN), KC_SPC,                  KC_ENT, KC_BSPC, KC_LGUI, KC_LALT, KC_RCTL
),

/*
 * Mac / Linux nav overlay (_NAV_SYM):
 * - TG(_BASE_WIN) top-left toggles the Windows profile (tap while holding nav).
 * - Homerow nav uses LALT so Option+arrows match macOS word/line movement.
 * - D/F = Up/Down, J/K = Left/Right; L/; = -/+
 * KC_NO everywhere else so accidental keys do nothing.
 */
[_NAV_SYM] = LAYOUT(
    TG(_BASE_WIN), KC_NO, KC_NO, KC_UP,   KC_NO, KC_NO,               KC_LEFT, KC_RIGHT, KC_DOWN, LBR_BRC, RBR_BRC, KC_NO,
    KC_NO, KC_NO, KC_NO, KC_NO,   KC_NO, KC_NO,                       KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
    KC_LALT, KC_LALT, KC_NO, KC_UP,   KC_DOWN, KC_NO,                 KC_NO,   KC_LEFT,  KC_RGHT, KC_MINS, PLU_EQ,  KC_NO,
    KC_LSFT, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_MUTE,           KC_NO,  KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
           KC_NO, KC_NO, KC_LGUI, KC_NO, KC_SPC,                    KC_LALT, KC_BSPC, KC_NO, KC_NO, KC_NO
),

/*
 * Windows nav overlay (_NAV_SYM_WIN):
 * - Same homerow arrow positions as _NAV_SYM, but LCTL replaces LALT so Ctrl+arrows
 *   match Windows word/paragraph navigation (same finger positions as Mac Option+arrows).
 * - Number row 1–8 → F1–F8; 9/0 slots → [ ] / { } (LBR_BRC/RBR_BRC); ` → F11; Tab → F12.
 * - Row 1 right: F9, F10 (moved down from the old number-row positions).
 * - H → Ctrl+Alt+Del.
 * - TG(_BASE_WIN) top-left switches back to Mac/Linux profile.
 */
[_NAV_SYM_WIN] = LAYOUT(
    TG(_BASE_WIN), KC_F1, KC_F2, KC_F3, KC_F4, KC_F5,               KC_F6, KC_F7, KC_F8, LBR_BRC, RBR_BRC, KC_F11,
    KC_F12, KC_NO, KC_NO, KC_NO,   KC_NO, KC_NO,                   C(A(KC_DEL)), KC_NO,    KC_NO,   KC_F9,   KC_F10,  KC_NO,
    KC_LCTL, KC_LCTL, KC_NO, KC_UP,   KC_DOWN, KC_NO,                 KC_NO,   KC_LEFT,  KC_RGHT, KC_MINS, PLU_EQ,  KC_NO,
    KC_LSFT, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_MUTE,           KC_NO,  KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
           KC_NO, KC_NO, KC_LCTL, KC_NO, KC_SPC,                    KC_LCTL, KC_BSPC, KC_NO, KC_NO, KC_NO
)
};

#if defined(ENCODER_MAP_ENABLE)
/*
 * Both rotary encoders control system volume only (no scroll / page keys).
 * ENCODER_CCW_CW(ccw, cw): turn one way = quieter, the other = louder.
 * Same mapping on every layer so knobs always behave the same.
 */
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_BASE]        = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [_NAV_SYM]     = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [_NAV_SYM_WIN] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [_BASE_WIN]    = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif
