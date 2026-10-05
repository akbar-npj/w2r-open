# Binary RPM spec for W2R Open.
#
# The payload is pre-built by packaging/build-packages.sh and shipped as Source0 (a tar of the
# staged /usr tree). This avoids rebuilding inside rpmbuild and keeps rpm/deb payloads identical.
# @VERSION@ is substituted by the build script.
%global debug_package %{nil}

Name:           w2r-open
Version:        @VERSION@
Release:        1%{?dist}
Summary:        Offline schematic and PCB boardview viewer
License:        MIT
URL:            https://github.com/OpenBoardView/OpenBoardView
Source0:        %{name}-%{version}-stage.tar.gz
BuildArch:      aarch64

Requires:       qt6-qtbase
Requires:       qt6-qtpdf
Requires:       qt6-qtsvg
Requires:       qt6-qtserialport
Recommends:     unar

%description
W2R Open is an offline viewer for PCB boardviews and schematic PDFs, aimed at board-level
repair work. It renders boardviews with pins, nets and diode readings, cross-probes between
the PCB and schematic views, and browses a local library of boards and schematics, including
files stored inside .rar/.zip/.7z archives. It runs entirely locally: no accounts, no cloud,
no telemetry.

%prep
# No sources to unpack; the payload is a pre-built tarball extracted in %install.

%build
# Binary package: payload is pre-built into Source0.

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
tar -xzf "%{_sourcedir}/%{name}-%{version}-stage.tar.gz" -C "%{buildroot}"

%files
%license usr/share/licenses/%{name}/LICENSE
/usr/bin/w2r-open
/usr/share/applications/w2r-open.desktop
/usr/share/icons/hicolor/scalable/apps/w2r-open.svg
/usr/share/metainfo/w2r-open.metainfo.xml
/usr/share/w2r-open/firmware

%changelog
* Mon Oct 05 2026 W2R Open contributors - @VERSION@-1
- Initial package
