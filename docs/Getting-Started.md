# Getting Started

## Requirements

- **OS:** Linux (aarch64 or x86-64).
- **Kernel page size:** 4 KiB, 16 KiB and 64 KiB are all supported (the binary is linked with
  64 KiB maximum page alignment and makes no page-size assumptions — including Asahi Linux on
  Apple Silicon, which uses 16 KiB).
- **Toolchain:** a C++17 compiler, CMake 3.21+.
- **Qt:** 6.5 or newer. **Core, Gui, Widgets, Concurrent** are required. **Pdf, Svg** and
  **SerialPort** are optional and only enable extra features (schematic viewing, SVG icons,
  the RFFE probe).

Per-distro package commands (Fedora, Debian/Ubuntu, Arch) are in **[BUILDING.md](../BUILDING.md)**.

## Build

The easy path uses the bundled script:

```sh
./build.sh --clean --test     # configure, build, run the test suite
```

`./build.sh --help` lists every option (`--clean`, `--test`, `--package`, `--install`,
`--prefix PATH`, `--debug`, …).

Manual CMake build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Install and package

```sh
./build.sh --package              # build .rpm and .deb into ./dist
./build.sh --install --prefix "$HOME/.local"
```

## First run

```sh
./build/w2r-open                       # empty viewer
./build/w2r-open board.brd             # open a boardview
./build/w2r-open board.brd sheet.pdf   # open a board and a schematic together
```

The window opens with a toolbar, a central PCB/PDF area, and dockable panels on the sides. The
first positional file is opened as a board, the second as a schematic PDF.

## Pointing at your library

By default the app indexes `~/Projects/Schematics`. Change the roots in
**File → Preferences**, or for a single run:

```sh
W2R_LIBRARY_ROOTS=/path/to/boards:/another/path ./build/w2r-open
```

See [Device Library](Device-Library.md).

## Command-line tools

Every mode is headless-friendly and exits with a non-zero status on failure.

```sh
w2r-open --stats <board>                 # parser statistics (no display needed)
w2r-open --render <board> <out.png> [w h]        # render the PCB canvas to a PNG
w2r-open --render-window <out.png> [w h ms]      # render the whole window to a PNG
w2r-open --library [query]               # index and search the library
w2r-open --resolve <query>               # resolve a query to a board, extract and parse it
w2r-open --diode <board> <sheet>         # apply a diode sheet and report matches
w2r-open --rffe-info                     # RFFE probe environment (serial, firmware, esptool, ports)
w2r-open --theme-selftest                # theme serialisation round-trip check
w2r-open --rffe-selftest                 # RFFE shields / test-points store round-trip
w2r-open --rffe-palette-selftest         # RFFE palette round-trip + stylesheet substitution
w2r-open --rffe-dialog-selftest [dir]    # render the RFFE dialogs + panel to PNGs
```

`--render`, `--render-window` and the self-tests use the Qt **offscreen** platform, so they work
over SSH with no display. Useful environment variables:

| Variable | Effect |
|----------|--------|
| `W2R_LIBRARY_ROOTS` | Colon-separated library roots (overrides saved settings) |
| `W2R_NO_SETTINGS` | Disable reading/writing user settings (used by tests) |
| `W2R_BOARD`, `W2R_PDF`, `W2R_NET`, `W2R_THEME`, `W2R_ANIM`, `W2R_RFFE` | Preload state for `--render-window` |
| `W2R_DIODE` | Diode sheet to overlay in `--render` |
| `W2R_RFFE_FIRMWARE` | Override the bundled RFFE probe firmware image |

## Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+O` | Open board |
| `Ctrl+P` | Preferences |
| `Ctrl+1` | PCB only |
| `Ctrl+2` | PDF only |
| `Ctrl+3` | Split PDF \| PCB |
| `Ctrl+4` | Split PDF \| PDF |
| `Home` | Fit board to view |
| `Space` | Flip board side |
| `R` | Rotate 90° |
| `Ctrl++` / `Ctrl+-` | Zoom in / out |
| `F11` | Full screen |
| `Ctrl+Shift+R` | Toggle the RFFE panel |
| `Ctrl+Q` | Quit |

## Next

- [Boardview Viewer](Boardview-Viewer.md) — start with a board.
- [RFFE Probe](RFFE-Probe.md) — set up the RF probe.
- [Omitted Features](Omitted-Features.md) — why some original features are absent.
