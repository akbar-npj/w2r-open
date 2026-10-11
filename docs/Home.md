# W2R Open — Wiki

W2R Open is an **offline Linux viewer for PCB boardviews and schematic PDFs**, aimed at
board-level repair work. It is a clean-room C++/Qt6 reimplementation of the proprietary
Windows app *W2R Solutions (Way2Repair)*, with the remote-account and cloud features removed:
everything runs locally, with **no accounts, no telemetry and no network access**.

This wiki documents everything the software can do and how to use it.

## Contents

| Page | What it covers |
|------|----------------|
| [Getting Started](Getting-Started.md) | Install, build, first run, command-line tools, keyboard shortcuts |
| [Boardview Viewer](Boardview-Viewer.md) | Supported board formats, rendering, pin-name recovery, the Net Inspector |
| [Schematics](Schematics.md) | PDF viewing, full-text net search, occurrences, cross-probing |
| [Diode Readings](Diode-Readings.md) | Importing diode/continuity sheets (JSON/CSV) and overlaying them on pads |
| [Device Library](Device-Library.md) | Indexing a folder tree of boards and schematics, archives, search |
| [RFFE Probe](RFFE-Probe.md) | The ESP32 USB-serial RF-front-end probe: scan, shields, test points, firmware |
| [Themes](Themes.md) | Built-in themes and the create/edit/delete colour editor (`pcb_themes.json`) |
| [Views and Animations](Views-and-Animations.md) | Split modes, pane swap, full screen, canvas animations, tree font size |
| [Preferences and Language](Preferences-and-Language.md) | Preferences dialog, recent files, the language menu, where settings live |
| [Omitted Features](Omitted-Features.md) | The cloud/account/DRM features that are **intentionally not implemented** |
| [Architecture](Architecture.md) | Source layout, formats, tests, build system, packaging |
| [Troubleshooting](Troubleshooting.md) | Common problems and fixes |

## At a glance

- **Boardviews** — obfuscated `.brd` and `BVRAW_FORMAT_3` `.bvr`; pins, parts, nets, outline,
  ratsnest; flip, rotate, zoom.
- **Schematics** — PDF rendering with full-text net search.
- **Cross-probing** — select a net on the PCB and jump to it in the schematic, and vice versa.
- **Diode readings** — overlay an imported JSON/CSV diode sheet on the pads.
- **Device library** — index a tree of boards + schematics (including inside archives) and open
  a board and its schematic in one click.
- **RFFE probe** — talk to an ESP32 RFFE probe over USB serial, scan a phone's RF front end,
  mark test points and update the probe firmware in-app.
- **Themes, animations, split views, language** — the presentation features of the original.

## Licence and provenance

W2R Open is MIT-licensed. Boardview parsing is adapted from
[OpenBoardView](https://github.com/OpenBoardView/OpenBoardView) (also MIT); see
`third_party/openboardview/`. It does not include or redistribute any proprietary Way2Repair
binaries; the RFFE probe firmware in `firmware/rffe-probe/` is an original clean-room
implementation.
