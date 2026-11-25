# Changelog

## [1.1.0] - 2026-11-24

### Added
- Per-timer snooze duration: new **Edit Snooze** option, synced with the phone settings page, with repeating reminders at each timer's snooze interval.
- Cumulative `+M:SS` overtime since the original timer end, shown on the timer screen while snoozing and on the alarm popup after each snooze.
- Custom Edit menu (Edit Time / Edit Name / Edit Snooze).

### Changed
- Lists wrap around: UP on the first item goes to the last, DOWN on the last goes to the first (timer list, Edit menu, Edit Name picker).
- Touchscreen input is disabled on the timer detail and alarm popup screens to prevent accidental stops (wrist-shake dismiss still works).
- Paused named timers are vertically centered in the timer list.

### Fixed
- Stale selection highlight in the timer list after timers reorder.
- Saved timers from earlier versions migrate automatically.

## [1.0.0]

- Initial release: named timers, voice dictation, customizable presets, phone settings sync, and timer reset.
