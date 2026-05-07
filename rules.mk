# Sofle Choc build options for this keymap.
# Use custom encoder_update_user() logic (handedness-aware), not encoder_map.
ENCODER_MAP_ENABLE = no
#
# Horizontal wheel keycodes (`KC_WH_L` / `KC_WH_R` aliases) are part of mousekey.
# Without this, the compiler won't know those keycodes and encoder-side scroll fails.
MOUSEKEY_ENABLE = yes

# OLED “words per minute” readout (`get_current_wpm`) — weighted recent typing speed.
WPM_ENABLE = yes
