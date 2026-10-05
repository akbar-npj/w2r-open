# Building W2R Open

W2R Open is a C++/Qt6 desktop application. This guide covers prerequisites, the automated
build script, a manual CMake build, tests, packaging and troubleshooting.

## Supported platforms

- **Architecture:** aarch64 and x86-64 Linux.
- **Kernel page size:** 4 KiB, 16 KiB and 64 KiB are all supported. The binary is linked with
  64 KiB maximum page alignment and makes no page-size assumptions, so the same build runs on
  every page-size kernel (including Asahi Linux on Apple Silicon, which uses 16 KiB).
- **Qt:** 6.5 or newer. Qt6 **Core**, **Gui**, **Widgets** and **Concurrent** are required.
  **Pdf**, **Svg** and **SerialPort** are optional and only enable extra features.

## Prerequisites

Install a C++ toolchain, CMake (3.21+) and the Qt6 development packages.

### Fedora / RHEL

```sh
sudo dnf install cmake ninja-build gcc-c++ \
                 qt6-qtbase-devel \
                 qt6-qtpdf-devel qt6-qtsvg-devel qt6-qtserialport-devel
```

### Debian / Ubuntu

```sh
sudo apt install cmake ninja-build g++ \
                 qt6-base-dev \
                 qt6-svg-dev libqt6pdf6-dev libqt6serialport6-dev
```

### Arch

```sh
sudo pacman -S cmake ninja base-devel \
               qt6-base qt6-svg qt6-pdf qt6-serialport
```

The optional packages map to features as follows:

| Package        | Enables                                            |
|----------------|----------------------------------------------------|
| `qt6-qtpdf`    | Schematic (PDF) viewing via QtPdf/PDFium           |
| `qt6-qtsvg`    | SVG icons and themes                               |
| `qt6-qtserialport` | ESP32 RFFE probe (scan + firmware update)      |

Without them the app still builds; the corresponding UI is disabled or shows a placeholder.

## Quick start

```sh
git clone <repository-url> w2r-open
cd w2r-open
./build.sh --clean --test
```

`build.sh` checks the toolchain, configures with CMake, compiles, and (with `--test`) runs the
test suite. The resulting binary is `build/w2r-open`.

### Common invocations

```sh
./build.sh                       # release build into ./build
./build.sh --clean --test        # wipe, rebuild, run tests
./build.sh -j 8 --package        # parallel build, then .rpm/.deb into ./dist
./build.sh --install --prefix "$HOME/.local"
./build.sh --debug -b build-dbg  # debug build in a separate directory
./build.sh --help                # all options
```

> **Paths with spaces:** the project directory may contain spaces. `build.sh` quotes every
> path, and the packaging script uses a space-free RPM top directory to work around
> `rpmbuild`'s sanity checks.

## Manual CMake build

If you prefer to drive CMake yourself:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --parallel
ctest --test-dir build --output-on-failure   # optional
sudo cmake --install build                   # optional
```

Useful CMake options:

| Option | Default | Meaning |
|--------|---------|---------|
| `CMAKE_BUILD_TYPE` | `Release` | `Release` or `Debug` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Install prefix |
| `BUILD_TESTING` | `ON` | Build the CTest suite |
| `W2R_SAMPLE_DIR` | `/home/shaanair/Projects/Schematics/Apple` | Sample boardviews used by tests |

### Optional Qt modules

CMake enables features automatically when the modules are found:

```sh
# Force-disable an optional module:
cmake -S . -B build -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Pdf=ON
```

The build reports which optional modules were found at configure time.

## Running the application

```sh
./build/w2r-open                       # GUI
./build/w2r-open board.brd             # open a boardview
./build/w2r-open board.brd schem.pdf   # board + schematic
```

Headless / diagnostic modes (no display required):

```sh
./build/w2r-open --stats board.brd            # parser statistics
./build/w2r-open --library "820-02443"        # index the local library
./build/w2r-open --resolve "820-02443"        # resolve + parse a board
./build/w2r-open --diode board.brd sheet.csv  # apply diode readings
./build/w2r-open --rffe-info                  # RFFE probe environment
```

## Tests

```sh
ctest --test-dir build --output-on-failure
```

The suite covers BRD/BVR parsing, diode-sheet import (including BRD pin-name recovery from a
sibling `.bvr`) and the RFFE environment check. Tests read real sample boardviews from
`W2R_SAMPLE_DIR`; point it elsewhere with `-DW2R_SAMPLE_DIR=/path/to/boards` if the default
does not exist.

## Packaging

```sh
./build.sh --package          # or: packaging/build-packages.sh
```

This stages the install tree and produces both packages in `dist/`:

- `w2r-open-<version>-1.<dist>.aarch64.rpm` (requires `rpmbuild`)
- `w2r-open_<version>_arm64.deb` (requires `dpkg-deb`)

Runtime dependencies for the `.deb` are derived from `ldd` so they stay correct as optional
modules change. The `.rpm` declares the equivalent `qt6-qtbase`/`qt6-qtpdf`/`qt6-qtsvg`/
`qt6-qtserialport` requirements.

## Probe firmware

The ESP32 RFFE probe firmware is open source (MIT) and lives in `firmware/rffe-probe/`. A
prebuilt app-partition image (`firmware/rffe-open-1.0.0.bin`) is bundled and flashed at
`0x10000` by the app's **Update Probe Firmware** button.

To rebuild it you need [PlatformIO](https://platformio.org/):

```sh
cd firmware/rffe-probe
pio run                       # produces .pio/build/esp32dev/firmware.bin
cp .pio/build/esp32dev/firmware.bin ../rffe-open-1.0.0.bin
```

See `firmware/rffe-probe/README.md` for the host protocol and wiring. Note that the RFFE
timing has not been bench-validated against real hardware.

## Troubleshooting

**CMake cannot find Qt6.**
Install the Qt6 base development package (see above). `find_package(Qt6 ...)` looks for the
`Qt6Config.cmake` that ships with the `-devel`/`-dev` packages, not the runtime libraries.

**`rpmbuild` fails with "Path does not start with RPM_BUILD_ROOT".**
This happens when the project path contains spaces. The packaging script already redirects the
RPM top directory to `~/.cache/w2r-open/rpmbuild`; if you invoke `rpmbuild` by hand, do the same.

**The schematic pane is empty.**
QtPdf was not found at configure time. Install `qt6-qtpdf-devel` (Fedora) or `libqt6pdf6-dev`
(Debian) and reconfigure.

**The RFFE probe is unavailable.**
Qt SerialPort was not found. Install `qt6-qtserialport-devel` (Fedora) or
`libqt6serialport6-dev` (Debian), or check `./build/w2r-open --rffe-info`.

**Out-of-memory during the link step.**
Reduce parallelism: `./build.sh -j 2`.
