# Thunderbird-Termine für die Plasma-Uhr

Plasma-Kalender-Plugin, das Termine aus Thunderbirds `calendar-data/cache.sqlite` liest
(nur lesend, auf einer Kopie) und in der Digitaluhr anzeigt. Kein Akonadi, kein Login.
Laden läuft in einem eigenen Thread, Plasmashell wartet nie darauf.

- Stand der Termine = letzte Synchronisierung von Thunderbird. Läuft Thunderbird nicht, bleibt er stehen.
- Kalender namens "*Feiertage*" werden übersprungen (macht schon das Plasma-Plugin "Feiertage").
- Voraussetzung in Thunderbird: `calendar.registry.<id>.cache.enabled = true` (steht in `user.js` des Profils).

## Bauen und installieren
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
    cmake --build build && cmake --install build
Das Plugin liegt dann in `~/.local/lib/qt6/plugins/plasmacalendarplugins/`. Plasma findet es über
`~/.config/plasma-workspace/env/tb-calendar-plugin.sh` (setzt QT_PLUGIN_PATH, wirkt ab dem nächsten Login).

## Testen ohne Plasma
    build/tbdump 2026-11-01 2026-11-30

## Entfernen
Plugin-Datei, `tb-calendar-plugin.sh` und "thunderbirdevents" aus den Kalender-Plugins der Uhr entfernen.
