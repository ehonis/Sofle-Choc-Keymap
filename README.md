# Sofle Choc — ethan_keymap

## Mac vs Windows profile

Two base layers share the same layout; only the **left thumb** Ctrl/Gui keys swap on Windows so copy/paste shortcuts feel like macOS (Ctrl on the outer thumb).

| Profile | RGB (base layer) | OLED (above WPM) |
|---------|------------------|------------------|
| Mac (default) | Neutral white | Apple icon |
| Windows | Cool white | Windows icon |

### Switch profiles

1. **Hold** the nav-layer key (inner left thumb).
2. **Tap** the top-left key on the left half (where `Esc` lives on the base layer). That key is `TG(_BASE_WIN)` on both nav overlays.

Tap again with the same gesture to switch back. Profile resets to Mac after a full power cycle (not stored in EEPROM).

Both OLEDs read the synced layer state (not a master-only variable), so the logo updates on the left and right half together.

### Nav overlays (Mac vs Windows)

| Profile | Nav layer | Modifier for word nav | Number row (while nav held) |
|---------|-----------|----------------------|-----------------------------|
| Mac / Linux | `_NAV_SYM` | `LALT` (Option+arrows) | unused (`KC_NO`) |
| Windows | `_NAV_SYM_WIN` | `LCTL` (Ctrl+arrows) | `1`–`0` → F1–F10, `` ` `` → F11, Tab → F12, H → Ctrl+Alt+Del |

Windows base uses `MO(_NAV_SYM_WIN)` so homerow arrows and modifiers match Windows shortcuts with the same finger positions as on Mac.
