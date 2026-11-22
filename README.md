# Named Timer (Pebble Time 2 & Pebble Round 2)

A countdown timer watchapp for Pebble Time 2 (`emery`) and Pebble Round 2 (`gabbro`), based on the official open-source [Pebble Timer](https://github.com/coredevices/pebble-apps/tree/main/timer) app, with support for **naming your saved timers** (e.g., *Coffee*, *Laundry*, *Tea*, *Cooking*, *Workout*).

## Features

- **Authentic Pebble Timer flow**: Add timers via `+` at the top of the list, set `HH:MM:SS`, view live countdown progress bars, MRU sorting (running timers on top), background Wakeups when the app is closed, speaker chime toggle on Pebble Time 2, and App Glance countdown.
- **Name any saved timer**: From the Detail screen, press **UP (Edit)** to open the Edit ActionMenu:
  - **Edit Time** — change the timer duration
  - **Dictate Name** — speak a custom name using the Pebble Time 2 / Round 2 microphone (`DictationSession`); or **long-press UP** from the Detail screen as a quick shortcut
  - **Preset Name** — pick from customizable preset names (*Coffee*, *Laundry*, *Tea*, *Cooking*, *Workout*, *Focus*, *Eggs*, *Nap* by default)
  - **Clear Name** — revert to an unnamed timer
- **Phone Settings (Clay)**: Customize up to 8 preset timer names from the Pebble mobile app settings page, and optionally push a new named timer straight to your watch.
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
