# Thunderbird events for the Plasma clock

A Plasma 6 calendar plugin that reads the events Thunderbird keeps in
`calendar-data/cache.sqlite` (read-only, on a copy) and shows them in the digital clock's
calendar popup. No Akonadi, no login. Events are delivered synchronously, because Plasma does not
remove duplicates when a plugin answers late (the events would show up twice).

Supports recurring events and exceptions, time zones (including DST), all-day events,
locations and the calendar colours from Thunderbird.

## Notes

- Events are as fresh as Thunderbird's last sync. If Thunderbird is not running, the clock keeps
  showing the old state.
- Calendars with "holiday" or "Feiertage" in their name are skipped (the Plasma "Holidays"
  plugin already covers them).
- Google's auto-generated descriptions are dropped, other descriptions are cut to 200 characters.
- Thunderbird only writes the cache if it is enabled per calendar. Put this into the profile's
  `user.js` (one line per CalDAV calendar, the id is in `prefs.js` under `calendar.registry.<id>`):

      user_pref("calendar.registry.<id>.cache.enabled", true);

## Build and install

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
    cmake --build build && cmake --install build

Requires Qt 6 (Core, Sql, Concurrent), KF6 CalendarCore and KF6 Declarative (CalendarEvents).
The plugin lands in `~/.local/lib/qt6/plugins/plasmacalendarplugins/`. Make Plasma find it by
putting this into `~/.config/plasma-workspace/env/tb-calendar-plugin.sh` (takes effect at the next login):

    export QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

Then enable "Thunderbird events" in the digital clock's settings under Calendar.

## Test without Plasma

    build/tbdump 2026-11-01 2026-11-30

## Uninstall

Delete the plugin file and `tb-calendar-plugin.sh`, and untick the plugin in the clock's settings.

## License

GPL-3.0-only, see `LICENSE`.
