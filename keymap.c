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
 * Landscape status (fits 128 px wide OLEDs). Avoids snprintf to save AVR flash.
 * SPLIT_WPM_ENABLE mirrors WPM onto the slave half so both OLEDs stay in sync.
 */

bool oled_task_user(void) {
    oled_clear();

    uint8_t layer_now = get_highest_layer(layer_state);
    uint8_t mods      = get_mods();

#ifdef WPM_ENABLE
    oled_write_P(PSTR("WPM "), false);
    /* get_u8_str uses a tiny static buffer inside QMK (RAM ok for short-lived draw). */
    oled_write(get_u8_str(get_current_wpm(), ' '), false);
    oled_write_P(PSTR(" "), false);
#else
    oled_write_P(PSTR("Lyr "), false);
#endif

    switch (layer_now) {
        case _BASE:
            oled_write_ln_P(PSTR("Base"), false);
            break;
        case _NAV_SYM:
            oled_write_ln_P(PSTR("Nav"), false);
            break;
        default:
            oled_write_ln_P(PSTR("?"), false);
            break;
    }

    oled_write_char((mods & MOD_MASK_CTRL) ? 'C' : '-', false);
    oled_write_char((mods & MOD_MASK_SHIFT) ? 'S' : '-', false);
    oled_write_char((mods & MOD_MASK_ALT) ? 'A' : '-', false);
    oled_write_char((mods & MOD_MASK_GUI) ? 'G' : '-', false);
    oled_write_P(PSTR(" "), false);

    led_t led_state = host_keyboard_led_state();
    oled_write_P(led_state.caps_lock ? PSTR("CAP ") : PSTR("--- "), false);
    /* M = USB half, lowercase s = I2C / serial slave split (split link OK). */
    oled_write_char(is_keyboard_master() ? 'M' : 's', false);

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
    KC_NO, KC_NO, KC_NO, KC_UP,   KC_DOWN, KC_NO,                 KC_NO,   KC_LEFT,  KC_RGHT, KC_MINS, KC_PLUS, KC_NO,
    KC_LSFT, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_MUTE, KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,
           KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,                  KC_LALT, KC_BSPC, KC_NO, KC_NO, KC_NO
)
};

#if defined(ENCODER_MAP_ENABLE)
/*
 * Encoder behavior (both layers):
 * - Left knob turn: horizontal scroll (inverted direction for left-hand use).
 * - Right knob turn: volume control.
 * This keeps media volume on the right hand and navigation scrolling on the left.
 */
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_BASE]     = { ENCODER_CCW_CW(MS_WHLR, MS_WHLL), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [_NAV_SYM]  = { ENCODER_CCW_CW(MS_WHLR, MS_WHLL), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif
