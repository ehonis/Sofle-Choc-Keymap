# Sofle Choc build options for this keymap.
# encoder_map in keymap.c gives each knob its own behavior.
ENCODER_MAP_ENABLE = yes

# The left encoder emits horizontal mouse-wheel reports for side-to-side
# scrolling. QMK omits all mouse keycodes unless this feature is compiled in.
MOUSEKEY_ENABLE = yes

# RP2040 work builds: use ./sofle compile:work (sets CONVERT_TO on the command line).
# Home AVR builds: use ./sofle compile (no converter).

# OLED “words per minute” readout (`get_current_wpm`) — weighted recent typing speed.
WPM_ENABLE = yes
