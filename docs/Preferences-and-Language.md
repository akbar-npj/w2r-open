# Preferences and Language

## Preferences

**File → Preferences…** (`Ctrl+P`) opens a dialog with:

- **Library roots** — the folders scanned for boardviews and schematics (see
  [Device Library](Device-Library.md)).
- **Theme:** — the active PCB theme (see [Themes](Themes.md)).
- **Show diode readings on pads** — toggles the diode overlay (see
  [Diode Readings](Diode-Readings.md)).
- **Show ratsnest for the selected net** — toggles the ratsnest lines.

Most view options also appear as toolbar buttons and/or in the **View** menu, and are kept in
sync with the preferences.

## Recent files

**File → Open Recent** lists the last boards you opened (newest first, up to 10), plus
**Clear Recent**. Clicking an entry re-opens that board.

## Language

**Help → Language** switches the UI language. The available languages mirror the original app:

- English (`en`)
- العربية — Arabic (`ar`)
- Русский — Russian (`ru`)
- தமிழ் — Tamil (`ta`)
- ไทย — Thai (`th`)

The chosen language is saved. If a translation file (`w2r-open_<code>.qm`) is not found, the UI
stays in its source language — a missing translation is not an error. The app also loads Qt's own
translations for the selected locale. An empty setting follows the system locale.

## Where settings live

Settings are stored with `QSettings` in **INI format, user scope**, under the organisation and
application name `w2r-open` — on Linux that is:

```
~/.config/w2r-open/w2r-open.ini
```

Persisted state includes:

- library roots, active theme, user themes (the full `pcb_themes.json` document),
- the RFFE panel palette,
- diode/ratsnest visibility, tree font size, language,
- animation toggles, window geometry and dock layout,
- recent files.

Two other data locations are used:

| Data | Location |
|------|----------|
| RFFE shields / test points | `rffe_testpoints.json` under `AppLocalDataLocation` |
| Extracted archive members | `~/.cache/w2r-open/extract/<hash>/` |

Set `W2R_NO_SETTINGS=1` to run without reading or writing user settings (the test suite does
this).
