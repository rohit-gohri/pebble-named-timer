# Named Timers (Pebble Time 2 & Pebble Round 2)

Native C Pebble SDK 4.44 watchapp derived from `coredevices/pebble-apps/timer`, targeting `emery` (Pebble Time 2, 200x228) and `gabbro` (Pebble Round 2, 260x260).

- **UUID:** `bdae1e1e-2828-4e9b-9e6d-071ee568ed8f`
- **Package / Display Name:** `named-timers` / `Named Timers` (`Boring Downloads`)
- **Pebble Appstore:** https://apps.repebble.com/2c61c0e6622c46f9b4945843 (App ID: `2c61c0e6622c46f9b4945843`)
- **Repository:** https://github.com/rohit-gohri/pebble-named-timer

## Documentation

Pebble SDK docs: https://developer.repebble.com/llms.txt. Fetch the `.md` version of any page before using an unfamiliar Pebble API.

## Commands

```bash
pebble build
pebble install --emulator emery --vnc
pebble install --emulator gabbro --vnc
pebble install --phone <phone-ip>
pebble screenshot --no-open --scale 2 --emulator emery --vnc shot.png
pebble emu-button click select --emulator emery --vnc
pebble logs --emulator emery --vnc
```

- Always pass `--vnc` to emulator commands in headless Linux/container environments.
- If the emulator gets into a stale state, run `pebble kill && pebble wipe` before reinstalling.
- Validate Phone Settings HTML generation without a device:
  ```bash
  node -e "const sp = require('./src/pkjs/settings_page'); console.log(sp.buildSettingsHtml({live:true, timers:[], presets:[]}).length)"
  ```

## Architecture

- `src/main.c` — App lifecycle, touch navigation opt-in (`app_touch_navigation_enable(true)`), Edit ActionMenu (`Edit Time` / `Edit Name` / `Edit Snooze`), `DictationSession` voice naming, two-way phone sync callbacks, Wakeups, and App Glance.
- `src/countdown_timer.[ch]` — Timer model & persistent storage (packed struct with 24-byte `name`, `snooze_ms` defaulting to `min(1 min, floor(duration / 2))` clamped to `>= 5s`, and v1→v2 struct migration on load).
- `src/presets.[ch]` — 8 customizable preset timer names persisted in watch storage and synced with the phone settings page.
- `src/menu_window.[ch]` — Timer list with dynamic row heights, two-line named timer cells, live progress bars, and `emery` speaker chime toggle.
- `src/detail_window.[ch]` — Countdown detail view with timer name header, water/radial fill, and ActionBar (`UP` = Edit ActionMenu, long-press `UP` = quick Dictate Name, `SELECT` = Play/Pause, long-press `SELECT` = Reset Timer preserving running/paused state, `DOWN` = Delete).
- `src/setting_window.[ch]` & `src/selection_layer.[ch]` — `HH:MM:SS` picker supporting both `SETTING_WINDOW_MODE_DURATION` (`Set Timer`) and `SETTING_WINDOW_MODE_SNOOZE` (`Set Snooze`).
- `src/name_window.[ch]` — Edit Name picker (`MenuLayer`) with optional top clear/remove icon (when named), vector mic icon for voice dictation, and active preset list below.
- `src/popup_window.[ch]` — Animated PDC alarm popup with timer name, live overtime counter (`+0:00`, `+0:01`, ...), repeating reminder vibes/chime at each timer's snooze interval while open, snooze (`UP`), dismiss (`DOWN`/`BACK`), and wrist-shake (`accel_tap_service`) dismiss.
- `src/phone.[ch]` & `src/pkjs/` — Offline Pebble-styled phone settings webview (`src/pkjs/settings_page.js`) and two-way watch↔phone AppMessage sync (`src/pkjs/index.js`). Wire format for `TIMERS_DATA` is newline-delimited `<id>|<duration_sec>|<flags>|<snooze_sec>|<name>` (backward-compatible with 4-field lines).

## Key Conventions & Gotchas

- **Persistence Compatibility:** `countdown_timer_list_load()` validates blob size. Whenever changing `struct CountdownTimer`, preserve backward-compatible migration from prior struct layouts so upgrades do not wipe saved timers.
- **Pebble C APIs:**
  - `MESSAGE_KEY_*` symbols in `message_keys.auto.h` are `extern uint32_t`, not C compile-time constants — do not use them in file-scope static array initializers.
  - `ActionMenu` items are text-only; use a custom `MenuLayer` window (like `NameWindow`) when rows need icons.
- **Upstream Attribution & Licensing:** Preserve original `Eric Phillips` author headers and upstream git history from `coredevices/pebble-apps/timer`. Do not add a conflicting `LICENSE` file over upstream files unless Core Devices publishes one upstream.
- **Store Assets:** Icons (`144x144`, `80x80`, `48x48`), banner (`720x320`), and native-resolution screenshots live under `store/`. Build artifacts (`build/`, `node_modules/`) stay untracked.
