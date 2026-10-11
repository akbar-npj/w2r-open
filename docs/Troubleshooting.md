# Troubleshooting

## Build

**Qt6 not found.** Install the development packages for your distro (see
[BUILDING.md](../BUILDING.md)) and point CMake at Qt if it lives off the default path:
`cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64`.

**Schematic pane is a placeholder.** Qt6::Pdf was not found at configure time. Install
`qt6-qtpdf-devel` (Fedora) / `libqt6pdf6-dev` (Debian) and reconfigure.

**RFFE probe tab is missing / no serial support.** Qt6::SerialPort was not found. Install the
SerialPort dev package and reconfigure. `w2r-open --rffe-info` reports `serial support: no` in
that case.

## Boardviews

**Pins/nets have no names on a `.brd`.** The `.brd` format carries no pin names. Put the sibling
`.bvr` for the same board in the same folder so names can be recovered by coordinate correlation
(see [Boardview Viewer](Boardview-Viewer.md)).

**A board fails to load.** `w2r-open --stats <file>` prints the parser error. Detection is by
content first, then extension, so a mis-named file usually still loads — if it does not, the file
may be a different/unsupported variant.

## Diode readings

**"0 readings matched."** The sheet did not line up with the board. Check the component designator
and pin spelling — on `.brd` boards the pin names must have been recovered first (sibling `.bvr`).
`--diode` exits non-zero when nothing matches.

## Device library

**Archives are disabled.** The XAD tools are missing. Install `unar`/`lsar` for your distro; the
`--library` output shows the reason and whether the backend is `enabled` or `DISABLED`.

**Nothing indexed / empty library.** Check the roots: `W2R_LIBRARY_ROOTS` overrides saved
settings, which override the default `~/Projects/Schematics`. Set roots in **File →
Preferences**.

## RFFE probe

See the [RFFE Probe troubleshooting table](RFFE-Probe.md#troubleshooting). Quick checks:

```sh
w2r-open --rffe-info          # is serial support on? is esptool found? which ports exist?
```

- *"Pick the probe's COM port first."* — select a port.
- **esptool not found** — `pip install esptool`.
- **Firmware rejected** — the message names the reason (missing manifest, hash mismatch, missing
  `0xE9` marker, wrong flash size); nothing was written.

## Settings / UI

**A setting won't stick.** Make sure `W2R_NO_SETTINGS` is not set. Settings live in
`~/.config/w2r-open/w2r-open.ini` (see
[Preferences and Language](Preferences-and-Language.md)).

**Reset the layout.** Delete the geometry/state keys from the INI, or remove the INI file to
start fresh.

## Headless / remote

Rendering and the self-tests use the Qt **offscreen** platform, so they run without a display:

```sh
w2r-open --render board.brd out.png
w2r-open --theme-selftest
```

If a GUI run fails over SSH, set `QT_QPA_PLATFORM=offscreen` or use a virtual X server.

## Reporting a problem

Include the output of `w2r-open --stats <file>` (or `--rffe-info` for probe issues), your distro,
Qt version and whether Pdf/Svg/SerialPort were detected. Remember the app makes **no network
calls** — nothing is sent anywhere automatically.
