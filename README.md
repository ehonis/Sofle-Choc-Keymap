# Sofle Choc — ethan_keymap

## Mac vs Windows profile

Two base layers share the same layout; only the **left thumb** Ctrl/Gui keys swap on Windows so copy/paste shortcuts feel like macOS (Ctrl on the outer thumb).

| Profile | RGB (base layer) | OLED (above WPM) |
|---------|------------------|------------------|
| Mac (default) | Neutral white | Apple icon |
| Windows | Cool white | Windows icon |

### Switch profiles

1. **Hold** the nav-layer key (inner left thumb, same as layer 1).
2. **Tap** the top-left key on the left half (where `Esc` lives on the base layer). That key is `OS_SWAP` on layer 1.

Tap again with the same gesture to switch back. Profile resets to Mac after a full power cycle (not stored in EEPROM).

Both OLEDs read the synced layer state (not a master-only variable), so the logo updates on the left and right half together.

To move the toggle elsewhere, change `OS_SWAP` in `[_NAV_SYM]` or bind it on the base layer in `keymap.c`.
