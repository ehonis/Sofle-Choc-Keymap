/* Sofle Choc — handedness / split tuning for this keymap.
 *
 * If the board “works briefly” then freezes with Pro Micro bootloader LEDs flashing, that is usually
 * a brownout reboot: powering ~58 WS2812-style LEDs plus OLED spikes current past what the onboard
 * 3.3V regulator (or skinny USB/TRRS cabling) can keep stable. Repo default warns that high RGB_MATRIX
 * limits can outright crash AVR controllers — we cap brightness and prefer a calm default effect.
 *
 * You confirmed the keyboard stays stable with OLED ribbons unplugged — so the tipping point is OLED
 * plus per-key RGB + split on the same skinny 3.3V regulator. We cap OLED contrast and RGB together;
 * if it still browns out, lower OLED_BRIGHTNESS / RGB_MATRIX_* a few more steps or use a short good
 * USB cable / powered hub (hardware path, not firmware).
 */
#pragma once

#define SPLIT_USB_DETECT
#define EE_HANDS
#define RGB_MATRIX_SLEEP
#define SPLIT_TRANSPORT_MIRROR

/*
 * Mirror WPM from the USB “master” half to the slave so both OLEDs can show typing speed while
 * you type across the whole board (requires WPM_ENABLE in rules.mk).
 */
#define SPLIT_WPM_ENABLE

/*
 * OLED pulls meaningful current at default “255” brightness. QMK defaults to 255 unless overridden;
 * 255 is brightest; lowering this saves a little panel current — change this if readability suffers.
 */
#define OLED_BRIGHTNESS 68

/* Slightly slower refresh on splits — tiny savings vs hammering I2C every 50 ms on both halves. */
#define OLED_UPDATE_INTERVAL 80

/* Tight RGB budget while OLED is connected (see RGB_MATRIX_DEFAULT_* below). */
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 38

/*
 * “Clear EEPROM” (QMK Toolbox, etc.) does not mean “reset LEDs to off” — it wipes stored settings, then
 * on the next boot QMK re-seeds RGB from compile-time defaults. Globally, QMK prefers
 * RGB_MATRIX_CYCLE_LEFT_RIGHT (moving rainbow) when that animation is compiled in unless you override
 * RGB_MATRIX_DEFAULT_MODE here — exactly why a toolbox clear often LOOKS like it “made it rainbow”.
 * This keymap forces SOLID_* below and re-applies them in keyboard_post_init_user().
 */
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR
#define RGB_MATRIX_DEFAULT_VAL 22
