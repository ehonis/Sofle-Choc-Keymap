/* Sofle Choc — handedness / split tuning for this keymap.
 *
 * RGB caps were very low (38) when this ran on AVR Pro Micro + OLED to avoid brownout reboots.
 * On RP2040 there is much more headroom — brightness is raised toward stock levels below.
 * If the board ever resets under full white + OLED, nudge RGB_MATRIX_MAXIMUM_BRIGHTNESS down.
 */
#pragma once

#define SPLIT_USB_DETECT
/*
 * EE_HANDS — identical PCBs; each half must know if it is left or right.
 *
 * AVR Pro Micro / Caterina (personal keyboard, TRRS unplugged):
 *   Left half:  ./sofle flash:left
 *   Right half: ./sofle flash:right
 * These use QMK's avrdude-split-left/right targets to store handedness in EEPROM.
 * A plain `qmk flash` does not assign the side; if both halves identify as left,
 * both physical encoders are reported as encoder 0 and receive the left mapping.
 *
 * RP2040 (UF2): flash EACH half with a different bootloader target (TRRS unplugged):
 *   Left half:  qmk flash -kb sofle_choc -km ethan_keymap -bl uf2-split-left
 *   Right half: qmk flash -kb sofle_choc -km ethan_keymap -bl uf2-split-right
 * (CONVERT_TO in rules.mk is picked up automatically.)
 *
 * Dragging the same .uf2 onto both halves leaves handedness wrong → keys feel swapped.
 * If hands are still reversed, you flashed the wrong side with left/right — swap the commands.
 */
#define EE_HANDS
#define RGB_MATRIX_SLEEP
#define SPLIT_TRANSPORT_MIRROR

/*
 * Lifetime keypress counter (replaces the old WPM readout).
 *
 * EECONFIG_USER_DATA_SIZE reserves user EEPROM bytes for the running total so the
 * number survives unplugging. 4 bytes = one uint32_t (max 4,294,967,295 keypresses).
 *
 * Only the USB master runs process_record_user(), so it owns the count. The slave half
 * needs the value for its own OLED, which is what the CHARCOUNT_SYNC split RPC carries.
 */
#define EECONFIG_USER_DATA_SIZE 4
#define SPLIT_TRANSACTION_IDS_USER CHARCOUNT_SYNC

/* Keep Mac/Win base layer and nav overlay identical on both halves. */
#define SPLIT_LAYER_STATE_ENABLE

/*
 * OLED pulls meaningful current at default “255” brightness. QMK defaults to 255 unless overridden;
 * 255 is brightest; lowering this saves a little panel current — change this if readability suffers.
 */
#define OLED_BRIGHTNESS 68

/* Slightly slower refresh on splits — tiny savings vs hammering I2C every 50 ms on both halves. */
#define OLED_UPDATE_INTERVAL 80

/* Per-key RGB ceiling (0–255). Stock QMK default is 255; 200 is bright without pegging every LED. */
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 200

/*
 * “Clear EEPROM” (QMK Toolbox, etc.) does not mean “reset LEDs to off” — it wipes stored settings, then
 * on the next boot QMK re-seeds RGB from compile-time defaults. Globally, QMK prefers
 * RGB_MATRIX_CYCLE_LEFT_RIGHT (moving rainbow) when that animation is compiled in unless you override
 * RGB_MATRIX_DEFAULT_MODE here — exactly why a toolbox clear often LOOKS like it “made it rainbow”.
 * This keymap forces SOLID_* below and re-applies them in keyboard_post_init_user().
 */
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR
#define RGB_MATRIX_DEFAULT_VAL 200
