# W2R Open

An offline Linux viewer for PCB boardviews and schematic PDFs, aimed at board-level repair
work. It is a clean-room C++/Qt6 reimplementation of the proprietary Windows app
*W2R Solutions (Way2Repair)*, with the remote-account and cloud features removed: everything
runs locally, with no accounts, no telemetry and no network access.

## Get the source

```sh
git clone https://github.com/akbar-npj/w2r-open.git
cd w2r-open
./build.sh --clean --test
```

Requires a C++17 toolchain, CMake 3.21+ and Qt 6.5+; see **[BUILDING.md](BUILDING.md)** for
per-distro prerequisites and options. The probe firmware and its source are included, so no
extra downloads are needed.

## Features

- **Boardview rendering** — parses obfuscated `.brd` and `BVRAW_FORMAT_3` `.bvr` files.
  Pins, parts, nets, outline and ratsnest, with top/bottom side flip, rotation, zoom and
  viewport culling.
- **Pin-name recovery** — the `.brd` format carries no pin numbers; when a sibling `.bvr`
  exists, pin names are recovered by coordinate correlation, so net/pin lookups and diode
  readings work on BRD boards too.
- **Schematic viewing** — PDF rendering and full-text net search via QtPdf.
- **Cross-probing** — select a net on the PCB and jump to it in the schematic, and vice versa.
- **Net Inspector** — browse every net and its pins; selecting one highlights it on both panes.
- **Diode readings** — import a JSON or CSV diode sheet and overlay the readings on the pads.
- **Device library** — indexes a folder tree of boards and schematics (including files inside
  `.rar`/`.zip`/`.7z` archives) and opens a board and its schematic in one click.
- **RFFE probe** — talk to an ESP32 RFFE probe over USB serial, scan a phone's RF front end,
  and update the probe firmware in-app. The probe firmware is open source (see
  `firmware/rffe-probe/`).
- **Themes, preferences, recent files, i18n scaffolding.**

## Install

Build `.rpm` and `.deb` packages:

```sh
./build.sh --package      # artifacts land in ./dist
```

Or install directly:

```sh
./build.sh --install --prefix "$HOME/.local"
```

## Build from source

```sh
./build.sh --clean --test
```

See **[BUILDING.md](BUILDING.md)** for prerequisites, options and troubleshooting. The build
works on 4 KiB, 16 KiB and 64 KiB page-size kernels (including Asahi Linux on Apple Silicon).

## Pointing at your library

By default the app indexes `~/Projects/Schematics`. Change the roots in **File → Preferences**,
or set them for a single run:

```sh
W2R_LIBRARY_ROOTS=/path/to/boards:/another/path ./build/w2r-open
```

## Command-line tools

```sh
w2r-open --stats board.brd            # parser statistics
w2r-open --library "820-02443"        # index and search the library
w2r-open --resolve "820-02443"        # resolve, extract and parse a board
w2r-open --diode board.brd sheet.csv  # apply a diode sheet
w2r-open --rffe-info                  # RFFE probe environment
```

## Licence and provenance

W2R Open is MIT-licensed (see [LICENSE](LICENSE)). Boardview parsing is adapted from
[OpenBoardView](https://github.com/OpenBoardView/OpenBoardView) (also MIT); see
`third_party/openboardview/`.

This project is an independent reimplementation for interoperability and repair work. It does
not include or redistribute any proprietary Way2Repair binaries. The RFFE probe firmware in
`firmware/rffe-probe/` is an original, clean-room implementation.
