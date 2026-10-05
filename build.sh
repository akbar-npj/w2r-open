#!/usr/bin/env bash
#
# build.sh — configure, compile and (optionally) test/package W2R Open.
#
# Works on aarch64 and x86-64 Linux, on 4K, 16K and 64K page-size kernels.
#
# Usage:
#   ./build.sh                 # release build into ./build
#   ./build.sh --clean --test  # wipe, rebuild, run the test suite
#   ./build.sh --package       # also produce .rpm / .deb in ./dist
#   ./build.sh --install --prefix /usr/local
#
# Run ./build.sh --help for all options.
set -euo pipefail

# --- defaults ---------------------------------------------------------------------
BUILD_DIR="build"
BUILD_TYPE="Release"
PREFIX="/usr/local"
GENERATOR="Ninja"
JOBS="$( (command -v nproc >/dev/null && nproc) || echo 2 )"
DO_CLEAN=0
DO_TEST=0
DO_PACKAGE=0
DO_INSTALL=0

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- pretty output ----------------------------------------------------------------
if [[ -t 1 ]]; then
    BOLD=$'\033[1m'; BLUE=$'\033[1;34m'; RED=$'\033[1;31m'; GREEN=$'\033[1;32m'; OFF=$'\033[0m'
else
    BOLD=""; BLUE=""; RED=""; GREEN=""; OFF=""
fi
say()  { printf '%s==>%s %s\n' "$BLUE" "$OFF" "$*"; }
ok()   { printf '%s ok %s %s\n' "$GREEN" "$OFF" "$*"; }
die()  { printf '%serror:%s %s\n' "$RED" "$OFF" "$*" >&2; exit 1; }

usage() {
    cat <<EOF
${BOLD}W2R Open — build script${OFF}

Usage: ./build.sh [options]

Options:
  -h, --help            Show this help and exit
  -c, --clean           Remove the build directory first
  -t, --test            Run the test suite (ctest) after building
  -p, --package         Build .rpm/.deb packages into ./dist after building
      --install         Install to --prefix after building
      --prefix PATH     Install prefix (default: $PREFIX)
  -d, --debug           Build with Debug symbols instead of Release
  -j N                  Parallel build jobs (default: $JOBS)
  -b, --build-dir DIR   Build directory (default: $BUILD_DIR)
  -G, --generator GEN   CMake generator (default: $GENERATOR)

Examples:
  ./build.sh --clean --test
  ./build.sh -j 8 --package
  ./build.sh --install --prefix "\$HOME/.local"

Notes:
  * The project path may contain spaces; this script quotes everything for that case.
  * Qt6 Core/Gui/Widgets/Concurrent are required. Pdf, Svg and SerialPort are optional:
    without them the app builds, but the schematic viewer, SVG icons or the RFFE probe
    are disabled respectively.
EOF
}

# --- argument parsing -------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)        usage; exit 0 ;;
        -c|--clean)       DO_CLEAN=1 ;;
        -t|--test)        DO_TEST=1 ;;
        -p|--package)     DO_PACKAGE=1 ;;
        --install)        DO_INSTALL=1 ;;
        --prefix)         PREFIX="${2:?--prefix needs a path}"; shift ;;
        -d|--debug)       BUILD_TYPE="Debug" ;;
        -j)               JOBS="${2:?-j needs a number}"; shift ;;
        -b|--build-dir)   BUILD_DIR="${2:?--build-dir needs a path}"; shift ;;
        -G|--generator)   GENERATOR="${2:?--generator needs a name}"; shift ;;
        --)               shift; break ;;
        *)                die "unknown option: $1 (try --help)" ;;
    esac
    shift
done

# Resolve the build dir relative to the project root unless absolute.
[[ "$BUILD_DIR" = /* ]] || BUILD_DIR="$ROOT/$BUILD_DIR"

# --- tool checks ------------------------------------------------------------------
say "Checking build tools"
command -v cmake >/dev/null || die "cmake not found. Install it (Fedora: 'dnf install cmake'; Debian: 'apt install cmake')."
cmake_ver="$(cmake --version | head -1 | awk '{print $3}')"
cmake_major="${cmake_ver%%.*}"
[[ "${cmake_major:-0}" -ge 3 ]] || die "cmake 3.21+ required, found $cmake_ver"
ok "cmake $cmake_ver"

if [[ "$GENERATOR" == "Ninja" ]] && ! command -v ninja >/dev/null; then
    say "ninja not found — falling back to Unix Makefiles"
    GENERATOR="Unix Makefiles"
fi
ok "generator: $GENERATOR"

if ! command -v cc >/dev/null && ! command -v gcc >/dev/null && ! command -v clang >/dev/null; then
    die "no C/C++ compiler found. Install a toolchain (Fedora: 'dnf install gcc-c++'; Debian: 'apt install build-essential')."
fi

# --- configure --------------------------------------------------------------------
if [[ $DO_CLEAN -eq 1 && -d "$BUILD_DIR" ]]; then
    say "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

say "Configuring ($BUILD_TYPE, prefix $PREFIX)"
if ! cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$PREFIX"; then
    cat >&2 <<EOF

${RED}Configuration failed.${OFF} The most common cause is missing Qt6 development files.

  Fedora / RHEL:
    sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel \\
                     qt6-qtpdf-devel qt6-qtsvg-devel qt6-qtserialport-devel

  Debian / Ubuntu:
    sudo apt install cmake ninja-build g++ qt6-base-dev \\
                     qt6-svg-dev libqt6pdf6-dev libqt6serialport6-dev

  Arch:
    sudo pacman -S cmake ninja base-devel qt6-base qt6-svg qt6-pdf qt6-serialport
EOF
    exit 1
fi

# --- build ------------------------------------------------------------------------
say "Building with $JOBS job(s)"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
ok "built $BUILD_DIR/w2r-open"

# --- test -------------------------------------------------------------------------
if [[ $DO_TEST -eq 1 ]]; then
    say "Running tests"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

# --- package ----------------------------------------------------------------------
if [[ $DO_PACKAGE -eq 1 ]]; then
    say "Building packages"
    W2R_BUILD_DIR="$BUILD_DIR" "$ROOT/packaging/build-packages.sh" --no-build
    ok "artifacts in $ROOT/dist"
fi

# --- install ----------------------------------------------------------------------
if [[ $DO_INSTALL -eq 1 ]]; then
    say "Installing to $PREFIX"
    cmake --install "$BUILD_DIR"
fi

say "${GREEN}Done.${OFF}"
