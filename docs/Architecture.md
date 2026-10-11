# Architecture

## Language and frameworks

C++17, CMake 3.21+, Qt 6.5+. Required Qt modules: **Core, Gui, Widgets, Concurrent**. Optional
modules enable extra features when present:

| Module | Enables | Compile define |
|--------|---------|----------------|
| Qt6::Pdf / PdfWidgets | Schematic PDF viewing | `W2R_HAVE_PDF` |
| Qt6::Svg | SVG icons | `W2R_HAVE_SVG` |
| Qt6::SerialPort | The RFFE probe | `W2R_HAVE_SERIALPORT` |

When an optional module is missing the app still builds; the affected feature shows a placeholder
or is disabled.

## Source layout

```
src/
  main.cpp                 entry point: GUI + all headless CLI modes
  core/
    Board.{h,cpp}          board model (parts, pins, nets, outline)
    BoardLoader.{h,cpp}    format detection + dispatch
    PinNameRecovery.*      recover .brd pin names from a sibling .bvr
    DiodeSheet.*           JSON/CSV diode-reading import
    Theme.*                PCB theme model + pcb_themes.json (de)serialisation
    Settings.*             QSettings-backed user state
    formats/ObAdapter.*    OpenBoardView parser adapter
    library/
      LibraryIndex.*       folder-tree indexer + search + board-number extraction
      ArchiveBackend.*     .rar/.zip/.7z access via lsar/unar (XAD)
    rffe/
      RffeModel.*          shields / test points / MarkSettings + rffe_testpoints.json
      RffeProbe.*          serial port discovery + probe protocol
      RffeFlasher.*        esptool driver + firmware manifest validation
      RffeTheme.*          RFFE_* palette + QSS builder (panel + dialog sheets)
  ui/
    MainWindow.*           toolbar, menus, docks, cross-probe wiring
    PcbView.*              boardview canvas + animations
    PdfView.*              QtPdf viewer + search
    NetInspector.*         net/pin tree
    LibraryPanel.*         device library dock
    OccurrencesPanel.*     schematic search hits
    ThemeEditorDialog.*    theme colour editor
    PreferencesDialog.*
    RffePanel.*            RFFE dock
    rffe/                  RFFE widget kit + dialogs (Flash/Settings/Prompt/lists)
  resources/themes.json    built-in PCB themes
third_party/openboardview/ vendored OpenBoardView parsers (MIT)
firmware/rffe-probe/       clean-room ESP32 probe firmware (PlatformIO)
firmware/rffe-open-1.0.0.bin  built firmware + manifest.json
docs/                      this wiki
tests/                     CTest registration + diode test data
```

## Board formats

- **BRD** (obfuscated `.brd`) — via the OpenBoardView-derived parser.
- **BVR3** (`BVRAW_FORMAT_3` `.bvr`).
- Detection is by **content sniffing first** (magic/format verification), then by extension.

Pin names are not stored in `.brd`; `PinNameRecovery` correlates pads by coordinate with a
sibling `.bvr` to recover them.

## RFFE firmware

`firmware/rffe-probe/` is an original ESP32 firmware built with PlatformIO; the compiled image is
`firmware/rffe-open-1.0.0.bin` and is described by `firmware/manifest.json` (version, file,
sha256, flash offset, chip, flash size). The flasher validates the image against the manifest
before writing. The vendor's firmware and binaries are **not** included.

## Tests

CTest cases (see `tests/CMakeLists.txt`):

- **Sample-based** (only registered when `-DW2R_SAMPLE_DIR=/path` exists and points at real
  boardviews): `stats_brd`, `stats_bvr3`, `diode_csv`, `diode_json`.
- **Always run:** `rffe_info`, `theme_selftest`, `rffe_selftest`, `rffe_palette_selftest`,
  `rffe_dialog_selftest`.

The self-tests are implemented as CLI flags in `main.cpp` (e.g. `--theme-selftest`) and run
headless via the Qt offscreen platform.

```sh
ctest --test-dir build --output-on-failure
```

## Packaging

`./build.sh --package` produces `.rpm` and `.deb` artifacts in `dist/`. Packaging files live in
`packaging/`. The build works on 4/16/64 KiB page-size kernels (64 KiB max page alignment, no
page-size assumptions).

## Provenance

W2R Open is MIT-licensed. Boardview parsing is adapted from OpenBoardView (MIT), vendored under
`third_party/openboardview/`. It is an independent reimplementation for interoperability and
repair work and does not include or redistribute any proprietary Way2Repair binaries. The RFFE
probe firmware is original and clean-room. See
[Omitted Features](Omitted-Features.md) for the cloud/DRM parts that are intentionally absent.
