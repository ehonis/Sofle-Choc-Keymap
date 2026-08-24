# Sofle Choc build options for this keymap.
# encoder_map in keymap.c gives each knob its own behavior.
ENCODER_MAP_ENABLE = yes

# The left encoder emits horizontal mouse-wheel reports for side-to-side
# scrolling. QMK omits all mouse keycodes unless this feature is compiled in.
MOUSEKEY_ENABLE = yes

# RP2040 work builds: use ./sofle compile:work (sets CONVERT_TO on the command line).
# Home AVR builds: use ./sofle compile (no converter).

# The OLED shows a lifetime keypress total (persisted in EEPROM), not WPM, so
# WPM_ENABLE is intentionally off - it only costs flash on the AVR build.
