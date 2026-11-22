# Named Timers (Pebble Time 2 & Pebble Round 2)

A countdown timer watchapp for Pebble Time 2 (`emery`) and Pebble Round 2 (`gabbro`) with support for **naming your saved timers** (e.g., *Coffee*, *Laundry*, *Tea*, *Cooking*, *Workout*).

> Based on the official open-source [Pebble Timer](https://github.com/coredevices/pebble-apps/tree/main/timer) watchapp by Pebble / Core Devices, with a fresh UUID so it installs cleanly alongside the official Timer app.

**Pebble Appstore:** https://apps.repebble.com/2c61c0e6622c46f9b4945843

## Features

- **Authentic Pebble Timer flow**: Add timers via `+` at the top of the list, set `HH:MM:SS`, view live countdown progress bars, MRU sorting (running timers on top), background Wakeups when the app is closed, speaker chime toggle on Pebble Time 2, and App Glance countdown.
- **Name & restart any saved timer**:
  - From the Detail screen, **long-press SELECT** to reset a timer back to its saved duration (restarts if running, stays stopped if paused), or **long-press UP** to jump straight into voice dictation.
  - Press **UP (Edit)** to open the Edit menu:
    - **Edit Time** — change the timer duration
    - **Edit Name** — opens the name picker with an optional remove/clear icon at the top (when a name is already set), a mic icon for voice dictation (`DictationSession`), and customizable preset names below (*Coffee*, *Laundry*, *Tea*, *Cooking*, *Workout*, *Focus*, *Eggs*, *Nap* by default)
- **Phone Settings & Two-Way Sync**: Open **App Settings** in the Pebble mobile app to view, rename, re-time (`HH:MM:SS`), delete, or add up to 8 saved timers directly from your phone (without interrupting running timers), plus customize the 8 quick preset names. Changes sync live when the watchapp is open or automatically the next time you open it.
- **System Touch Navigation**: Opts into PebbleOS touch navigation (`app_touch_navigation_enable(true)`) alongside full physical button support.
- **Hands-Free Alarm Dismiss**: When a timer expires, the alarm popup shows the timer's name (*e.g., "Coffee — Time's Up!"*), vibrates/chimes, and can be snoozed (+1 min), dismissed by button, or dismissed with a wrist shake (`accel_tap_service`).

## Build & Test

```bash
pebble build
pebble install --emulator emery --vnc
pebble screenshot --no-open --scale 2 --emulator emery --vnc screenshot_emery.png
```

To install on a physical watch via the Pebble mobile app:
```bash
pebble install --cloudpebble
# or on the same Wi-Fi network:
pebble install --phone <phone-ip>
```

## Credits & License

This watchapp is derived from the official [Pebble Timer](https://github.com/coredevices/pebble-apps/tree/main/timer) app in `coredevices/pebble-apps` (originally authored by Eric Phillips at Pebble, with Emery/Gabbro and PebbleOS updates by Thomas Buchheit and Eric Migicovsky / Core Devices).

- Full upstream git history and original source author headers are preserved.
- The upstream `coredevices/pebble-apps` repository does not currently include an explicit `LICENSE` file; accordingly, no conflicting license is asserted over upstream files, which remain subject to their original authors' rights and any license published upstream by Core Devices.
