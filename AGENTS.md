# Named Timers (Pebble Time 2 & Pebble Round 2)

Native C Pebble SDK 4.44 watchapp based on `coredevices/pebble-apps/timer`, targeting `emery` (Pebble Time 2, 200x228) and `gabbro` (Pebble Round 2, 260x260).

## Documentation

Pebble SDK docs: https://developer.repebble.com/llms.txt. Fetch the `.md` version of any page before using an API.

## Commands

```bash
pebble build
pebble install --emulator emery --vnc
pebble screenshot --no-open --scale 2 --emulator emery --vnc shot.png
pebble emu-button click select --emulator emery --vnc
pebble logs --emulator emery --vnc
```

Always pass `--vnc` to emulator commands in headless Linux/container environments. If the emulator gets into a stale state, run `pebble kill && pebble wipe` before reinstalling.

## Architecture

- `src/main.c` — App lifecycle, touch navigation opt-in (`app_touch_navigation_enable(true)`), Edit ActionMenu, `DictationSession` voice naming, Clay callbacks, Wakeups, App Glance.
- `src/countdown_timer.[ch]` — Timer model & persistence (including 24-byte `name` label).
- `src/presets.[ch]` — 8 customizable preset timer names persisted in watch storage and synced from Clay.
- `src/menu_window.[ch]` — Timer list with dynamic row heights and two-line named timer cells.
- `src/detail_window.[ch]` — Countdown detail view with timer name header, water/radial fill, and ActionBar (UP = Edit ActionMenu [Edit Time / Edit Name], long-press UP = quick Dictate Name, SELECT = Play/Pause, long-press SELECT = Reset Timer, DOWN = Delete).
- `src/name_window.[ch]` — Edit Name picker (`MenuLayer`) with optional top clear/remove icon, mic icon for voice dictation, and preset list below.
- `src/popup_window.[ch]` — Animated PDC alarm popup with timer name, speaker chime (emery), snooze/dismiss, and wrist-shake (`accel_tap_service`) dismiss.
- `src/phone.[ch]` & `src/pkjs/` — Offline Pebble-styled phone settings webview (`settings_page.js`) and two-way watch↔phone AppMessage sync (`index.js`) for editing, adding, and deleting all saved timers plus 8 preset names.
