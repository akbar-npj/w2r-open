// Application settings, persisted with QSettings.
//
// A single place for user-configurable state (library roots, default theme, view toggles)
// so the GUI and the headless CLI modes agree. Explicit org/app names are used because the
// headless modes run without a QApplication that would otherwise supply them.
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace w2r {
namespace Settings {

// Library search roots. Precedence: W2R_LIBRARY_ROOTS env var, then saved settings, then the
// ~/Projects/Schematics default when it exists.
QStringList libraryRoots();
void setLibraryRoots(const QStringList &roots);

QString themeName();
void setThemeName(const QString &name);

// User-created PCB themes, stored as the original pcb_themes.json document ({"themes":[...]}).
QByteArray userThemes();
void setUserThemes(const QByteArray &json);

bool showDiodeReadings();
void setShowDiodeReadings(bool on);

bool showRatsnest();
void setShowRatsnest(bool on);

// Font size (points) for the Net Inspector and Library trees; 0 = application default.
int treeFontPointSize();
void setTreeFontPointSize(int pointSize);

// UI language code (e.g. "en", "ar", "ru", "ta", "th"); empty = follow the system locale.
QString language();
void setLanguage(const QString &code);

// PCB canvas animations (original app: pcb.anim.*).
bool animationsEnabled();
void setAnimationsEnabled(bool on);
bool spotlightDim();
void setSpotlightDim(bool on);
bool netPulse();
void setNetPulse(bool on);
bool traceReveal();
void setTraceReveal(bool on);

// Window geometry / dock layout, stored as QWidget::saveGeometry()/saveState() blobs.
QByteArray windowGeometry();
void setWindowGeometry(const QByteArray &geometry);
QByteArray windowState();
void setWindowState(const QByteArray &state);

// Most-recently-opened board files (newest first), capped at 10.
QStringList recentFiles();
void setRecentFiles(const QStringList &files);

} // namespace Settings
} // namespace w2r
