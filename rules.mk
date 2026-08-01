# Sofle Choc build options for this keymap.
# encoder_map in keymap.c drives both knobs (volume only).
ENCODER_MAP_ENABLE = yes

# RP2040 work builds: use ./sofle compile:work (sets CONVERT_TO on the command line).
# Home AVR builds: use ./sofle compile (no converter).

# OLED “words per minute” readout (`get_current_wpm`) — weighted recent typing speed.
WPM_ENABLE = yes
