# Sofle Choc — ethan_keymap

## Mac vs Windows profile

Two base layers share the same layout; only the **left thumb** Ctrl/Gui keys swap on Windows so copy/paste shortcuts feel like macOS (Ctrl on the outer thumb).

| Profile | RGB (base layer) | OLED (above WPM) |
|---------|------------------|------------------|
| Mac (default) | Neon light blue | Apple icon |
| Windows | Deep amethyst purple | Windows icon |
| Nav (either profile) | White | Same WPM + profile icon |

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

## Rotary encoders

The encoder behavior stays the same on every layer, including while a nav layer is held:

- Turn the **left encoder** to scroll horizontally (side to side).
- Press the **left encoder** to zoom the current page in. It sends Command+Plus
  on the Mac profile and Ctrl+Plus on the Windows profile.
- Hold the **nav-layer key** and press the **left encoder** to zoom out instead.
  It sends Command+Minus on Mac or Ctrl+Minus on Windows.
- Turn the **right encoder** counter-clockwise for volume up or clockwise for volume down.
- Press the **right encoder** to play or pause media.

### AVR handedness is required

QMK numbers split encoders left-first: the left knob is encoder 0 and the right
knob is encoder 1. Because this keymap uses `EE_HANDS`, each personal-keyboard
Pro Micro must have its physical side stored in EEPROM. Unplug TRRS, connect and
reset one half at a time, then run:

```bash
./sofle flash:left
./sofle flash:right
```

Use `flash:left` only while the physical left half is connected and
`flash:right` only for the physical right half. If both halves are flashed as
left, both knobs use encoder 0's horizontal-scroll mapping.
