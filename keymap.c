/* Copyright 2023 Brian Low — default header retained where applicable.
 * Key placements mirror the user's VIA/web config screenshots (layers 0 & 1).
 */
#include QMK_KEYBOARD_H

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h"
#endif

/* Layer aliases keep the keymap readable and match your labels. */
enum sofle_layers { _BASE = 0, _NAV_SYM = 1 };

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
            /* Layer 1: cyan so nav layer is visually distinct. */
            rgb_matrix_sethsv_noeeprom(128, 255, 180);
            break;
        case _BASE:
        default:
            /* Layer 0: white as the normal typing layer. */
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

    /* Portrait canvas here is 32w x 128h. Center two 13px digits with 2px gap. */
    const uint8_t digit_w = 13;
    const uint8_t gap     = 2;
    const uint8_t total_w = (uint8_t)(digit_w * 2 + gap); /* 28 */
    const uint8_t start_x = (uint8_t)((32 - total_w) / 2); /* centered -> 2 */
    const uint8_t start_y = (uint8_t)((128 - 23) / 2);     /* vertically centered -> 52 */

    /*
     * Keep the leading zero for readability at a glance (e.g. "07").
     * If you prefer blank leading zero, we can conditionally skip tens.
     */
    draw_big_digit(start_x, start_y, tens);
    draw_big_digit((uint8_t)(start_x + digit_w + gap), start_y, ones);

    return false;
}

#endif /* OLED_ENABLE */

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
              KC_LGUI, KC_LALT, KC_LCTL, MO(_NAV_SYM), KC_SPC,                  KC_ENT, KC_BSPC, KC_LGUI, KC_NO, KC_RCTL
),

/*
 * Layer 1:
 * - Homerow navigation cluster:
 *   - D = Up, F = Down
 *   - J = Left, K = Right
 * - L = '-', ; = '+'
 * KC_NO disables all other keys so this layer only does what you mapped intentionally.
 * Left Shift is kept on this layer so shifted arrows/symbols still work while held.
 */
[_NAV_SYM] = LAYOUT(
    KC_NO, KC_NO, KC_NO, KC_UP,   KC_NO, KC_NO,                   KC_LEFT, KC_RIGHT, KC_DOWN, KC_LCBR, KC_RCBR, KC_NO,
    KC_NO, KC_NO, KC_NO, KC_NO,   KC_NO, KC_NO,                   KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
    KC_LALT, KC_LALT, KC_NO, KC_UP,   KC_DOWN, KC_NO,                 KC_NO,   KC_LEFT,  KC_RGHT, KC_MINS, KC_PLUS, KC_NO,
    KC_LSFT, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_MUTE, KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
           KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,                  KC_LALT, KC_BSPC, KC_NO, KC_NO, KC_NO
)
};

#ifdef ENCODER_ENABLE
/*
 * Split encoder handling:
 * - index 0 => left encoder
 * - index 1 => right encoder
 *
 * This is routed by QMK split transport, so behavior stays consistent regardless
 * of which half is currently USB master.
 */
bool encoder_update_user(uint8_t index, bool clockwise) {
    switch (index) {
        case 0:
            /* Left knob: sideways scroll. */
            tap_code(clockwise ? MS_WHLR : MS_WHLL);
            break;
        case 1:
            /* Right knob: volume. */
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;
        default:
            return true;
    }
    return false;
}
#endif
