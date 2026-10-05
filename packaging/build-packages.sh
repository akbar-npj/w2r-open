#!/usr/bin/env bash
#
# Builds installable .rpm and .deb packages for W2R Open (aarch64).
#
# One CMake build + install is staged into build/pkgroot, then:
#   - the .deb is produced directly from that tree with dpkg-deb
#   - the .rpm is produced from a tarball of the same tree via rpmbuild
# so both packages carry an identical payload.
#
# Usage: packaging/build-packages.sh [--no-build]
set -euo pipefail

VERSION="0.1.0"
NAME="w2r-open"
HOST_ARCH="$(uname -m)"                       # aarch64
DEB_ARCH="arm64"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Honour a build directory chosen by build.sh; default to ./build.
BUILD="${W2R_BUILD_DIR:-$ROOT/build}"
STAGE="$BUILD/pkgroot"
# rpmbuild's buildroot sanity checks break on paths containing spaces, and this project's
# path has them, so the RPM topdir lives under a space-free cache location.
RPMDIR="${XDG_CACHE_HOME:-$HOME/.cache}/w2r-open/rpmbuild"
OUT="$ROOT/dist"

NO_BUILD=0
[[ "${1:-}" == "--no-build" ]] && NO_BUILD=1

say() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }

# --- 1. Build + stage -------------------------------------------------------------
if [[ $NO_BUILD -eq 0 ]]; then
    say "Configuring and building ($HOST_ARCH)"
    cmake -S "$ROOT" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "$BUILD"
fi

say "Staging into $STAGE"
rm -rf "$STAGE"
mkdir -p "$STAGE"
# Packages always target /usr, regardless of the build's configured install prefix.
DESTDIR="$STAGE" cmake --install "$BUILD" --prefix /usr

install -Dm644 "$ROOT/packaging/w2r-open.desktop"      "$STAGE/usr/share/applications/w2r-open.desktop"
install -Dm644 "$ROOT/packaging/w2r-open.metainfo.xml" "$STAGE/usr/share/metainfo/w2r-open.metainfo.xml"
install -Dm644 "$ROOT/packaging/icons/w2r-open.svg"    "$STAGE/usr/share/icons/hicolor/scalable/apps/w2r-open.svg"
install -Dm644 "$ROOT/LICENSE"                         "$STAGE/usr/share/licenses/$NAME/LICENSE"

mkdir -p "$OUT"

# --- 2. .deb ----------------------------------------------------------------------
say "Building .deb"
DEBIAN="$STAGE/DEBIAN"
rm -rf "$DEBIAN"
mkdir -p "$DEBIAN"

INSTALLED_KB="$(du -sk "$STAGE/usr" | cut -f1)"

# Derive Qt6 runtime dependencies from the actual binary so the list never drifts as optional
# modules (Pdf, Svg, SerialPort) come and go.
qt_deb_package() {
    case "$1" in
        libQt6Core.so.6)       echo libqt6core6 ;;
        libQt6Gui.so.6)        echo libqt6gui6 ;;
        libQt6Widgets.so.6)    echo libqt6widgets6 ;;
        libQt6Concurrent.so.6) echo libqt6concurrent6 ;;
        libQt6DBus.so.6)       echo libqt6dbus6 ;;
        libQt6Network.so.6)    echo libqt6network6 ;;
        libQt6Pdf.so.6)        echo libqt6pdf6 ;;
        libQt6PdfWidgets.so.6) echo libqt6pdfwidgets6 ;;
        libQt6Svg.so.6)        echo libqt6svg6 ;;
        libQt6SerialPort.so.6) echo libqt6serialport6 ;;
        *)                     echo "" ;;
    esac
}

DEB_DEPS=""
while read -r lib; do
    pkg="$(qt_deb_package "$lib")"
    [[ -n "$pkg" ]] && DEB_DEPS="${DEB_DEPS:+$DEB_DEPS, }$pkg"
done < <(ldd "$STAGE/usr/bin/$NAME" | grep -oP 'libQt6\w+\.so\.6' | sort -u)
[[ -z "$DEB_DEPS" ]] && DEB_DEPS="libqt6widgets6, libqt6gui6, libqt6core6"

cat > "$DEBIAN/control" <<EOF
Package: $NAME
Version: $VERSION
Section: electronics
Priority: optional
Architecture: $DEB_ARCH
Installed-Size: $INSTALLED_KB
Depends: $DEB_DEPS
Recommends: unar
Maintainer: W2R Open contributors <noreply@example.invalid>
Homepage: https://github.com/OpenBoardView/OpenBoardView
Description: Offline schematic and PCB boardview viewer
 W2R Open is an offline viewer for PCB boardviews and schematic PDFs, aimed at
 board-level repair work. It renders boardviews with pins, nets and diode
 readings, cross-probes between the PCB and schematic views, and browses a local
 library of boards and schematics, including files inside .rar/.zip/.7z archives.
 It runs entirely locally: no accounts, no cloud, no telemetry.
EOF

dpkg-deb --build --root-owner-group "$STAGE" "$OUT/${NAME}_${VERSION}_${DEB_ARCH}.deb"

# --- 3. .rpm ----------------------------------------------------------------------
if command -v rpmbuild >/dev/null 2>&1; then
    say "Building .rpm"
    rm -rf "$RPMDIR"
    mkdir -p "$RPMDIR"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

    # Payload tarball (top-level entries are ./usr/...).
    tar -C "$STAGE" --exclude=./DEBIAN -czf "$RPMDIR/SOURCES/${NAME}-${VERSION}-stage.tar.gz" .

    # Substitute the version into the spec.
    sed "s/@VERSION@/${VERSION}/g" "$ROOT/packaging/rpm/${NAME}.spec" > "$RPMDIR/SPECS/${NAME}.spec"

    rpmbuild --define "_topdir $RPMDIR" -bb "$RPMDIR/SPECS/${NAME}.spec"
    cp "$RPMDIR"/RPMS/*/*.rpm "$OUT/" 2>/dev/null || true
else
    say "rpmbuild not found — skipping .rpm"
fi

say "Done. Artifacts in $OUT:"
ls -la "$OUT"
