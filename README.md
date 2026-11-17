# Sofle Choc — ethan_keymap

## Mac vs Windows profile

Two base layers share the same layout; only the **left thumb** Ctrl/Gui keys swap on Windows so copy/paste shortcuts feel like macOS (Ctrl on the outer thumb).

| Profile | RGB (base layer) | OLED (beside the counter) |
|---------|------------------|------------------|
| Mac (default) | Neon light blue | Pine tree |
| Windows | Deep amethyst purple | Snowy pine tree |
| Nav (either profile) | White | Same counter + profile icon |

### Switch profiles

1. **Hold** the nav-layer key (inner left thumb).
2. **Tap** the top-left key on the left half (where `Esc` lives on the base layer). That key is `TG(_BASE_WIN)` on both nav overlays.

Tap again with the same gesture to switch back. The active profile is **remembered across
power cycles** - it is mirrored into user EEPROM whenever it changes and re-applied at boot,
so the board comes back up on whichever profile you left it on.

Both OLEDs read the synced layer state (not a master-only variable), so the tree updates on the left and right half together.

## OLED keypress counter

The OLED shows a **lifetime keypress total** rather than WPM. It counts every key
press (encoder turns are excluded) and abbreviates as it grows. The whole part
is drawn big; every digit after the decimal point is drawn **small** (shown as
subscripts below):

| Total | Shown | Exact to |
|-------|-------|----------|
| 0 - 999 | `437` | 1 |
| 1,000 - 99,999 | `1.₂₃₄k`, `12.₃₄₅k` | 1 |
| 100,000 - 999,999 | `123.₄₅k` | 10 |
| 1,000,000 - 99,999,999 | `1.₂₃₄m`, `12.₃₄₅m` | 1,000 |
| 100,000,000 and up | `123.₄₅m` | 10,000 |

Values are truncated rather than rounded, so the readout ticks like an odometer.

The number **survives unplugging** - it is stored in user EEPROM
(`EECONFIG_USER_DATA_SIZE`, which also holds the active OS profile: bytes 0-3 are the
count, byte 4 is the profile flag). It is deliberately *not* written on every keystroke:
EEPROM cells endure only ~100k writes, so a per-keystroke write would wear the
controller out within hours. Instead the total is flushed 30 s after you stop
typing, or every 2,000 presses during a long session. A hard unplug can therefore
lose at most the presses since the last flush, which in normal use is zero.

Only the USB-connected half runs `process_record_user()`, so the master owns the
count and pushes it to the other half over the `CHARCOUNT_SYNC` split transaction -
that is what keeps both OLEDs showing the same number.

The digits use the **Oswald** typeface (SIL Open Font License), pre-rendered to
1-bit bitmaps in `oled_font.h`. To try another font or weight, regenerate the header
with `tools/gen_oled_font.py`; its docstring has the exact commands.

The readout is drawn in landscape (`OLED_ROTATION_180`) along the panel's long
axis; the 32 px short axis only ever had room for two digits. If the display reads
upside down after a rebuild, switch that to `OLED_ROTATION_0` in `oled_init_user`.

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

## Flashing

One command does both halves. From this folder (PowerShell or cmd, via `sofle.cmd`):

```powershell
sofle flash:work    # work board (RP2040)
sofle flash:home    # home board (AVR Pro Micro)
```

`flash:work` builds a left and a right image first (each half stores its side, so
they differ), then walks you through it:

1. Unplug USB and the TRRS cable from both halves.
2. Plug USB into the **left** half and double-tap its reset button. An `RPI-RP2`
   drive appears and the script copies the left image on its own.
3. When it asks, move USB to the **right** half and double-tap reset again.
4. Unplug USB, reconnect TRRS, plug USB back in.

Do the left half first. If the halves come out swapped, you plugged in the right
half first, so run it again in the right order.

### Build machine setup (Windows)

- [QMK MSYS](https://msys.qmk.fm/) installed at `C:\QMK_MSYS` (`sofle.cmd` uses it),
  with `qmk setup` cloning `qmk_firmware` to `%USERPROFILE%\qmk_firmware`.
- This repo linked into the QMK tree as the keymap folder:

  ```powershell
  New-Item -ItemType Junction -Path $HOME\qmk_firmware\keyboards\sofle_choc\keymaps\ethan_keymap -Target $HOME\Sofle-Choc-Keymap
  ```
- Optionally add this folder to your user `PATH` so `sofle` works from any directory.
