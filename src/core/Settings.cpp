#include "core/Settings.h"

#include <QDir>
#include <QSettings>

namespace w2r {
namespace Settings {

namespace {
QSettings store()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("w2r-open"),
	                 QStringLiteral("w2r-open"));
}

// Test/headless harnesses set W2R_NO_SETTINGS so they never clobber real user preferences.
bool writable()
{
	return qEnvironmentVariableIsEmpty("W2R_NO_SETTINGS");
}
} // namespace

QStringList libraryRoots()
{
	const QByteArray env = qgetenv("W2R_LIBRARY_ROOTS");
	if (!env.isEmpty())
		return QString::fromLocal8Bit(env).split(QLatin1Char(':'), Qt::SkipEmptyParts);

	const QStringList saved = store().value(QStringLiteral("library/roots")).toStringList();
	if (!saved.isEmpty()) return saved;

	QStringList roots;
	const QString guess = QDir::homePath() + QStringLiteral("/Projects/Schematics");
	if (QDir(guess).exists()) roots << guess;
	return roots;
}

void setLibraryRoots(const QStringList &roots)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("library/roots"), roots);
}

QString themeName()
{
	return store().value(QStringLiteral("view/theme")).toString();
}

void setThemeName(const QString &name)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("view/theme"), name);
}

QByteArray userThemes()
{
	return store().value(QStringLiteral("themes/user")).toByteArray();
}

void setUserThemes(const QByteArray &json)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("themes/user"), json);
}

QByteArray rffePalette()
{
	return store().value(QStringLiteral("rffe/palette")).toByteArray();
}

void setRffePalette(const QByteArray &json)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("rffe/palette"), json);
}

bool showDiodeReadings()
{
	return store().value(QStringLiteral("view/showDiode"), true).toBool();
}

void setShowDiodeReadings(bool on)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("view/showDiode"), on);
}

bool showRatsnest()
{
	return store().value(QStringLiteral("view/showRatsnest"), true).toBool();
}

void setShowRatsnest(bool on)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("view/showRatsnest"), on);
}

int treeFontPointSize()
{
	return store().value(QStringLiteral("view/treeFontPointSize"), 0).toInt();
}

void setTreeFontPointSize(int pointSize)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("view/treeFontPointSize"), pointSize);
}

QString language()
{
	return store().value(QStringLiteral("ui/language")).toString();
}

void setLanguage(const QString &code)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("ui/language"), code);
}

bool animationsEnabled()
{
	return store().value(QStringLiteral("anim/master"), true).toBool();
}
void setAnimationsEnabled(bool on)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("anim/master"), on);
}

bool spotlightDim()
{
	return store().value(QStringLiteral("anim/spotlightDim"), false).toBool();
}
void setSpotlightDim(bool on)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("anim/spotlightDim"), on);
}

bool netPulse()
{
	return store().value(QStringLiteral("anim/netPulse"), false).toBool();
}
void setNetPulse(bool on)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("anim/netPulse"), on);
}

bool traceReveal()
{
	return store().value(QStringLiteral("anim/traceReveal"), false).toBool();
}
void setTraceReveal(bool on)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("anim/traceReveal"), on);
}

QByteArray windowGeometry()
{
	return store().value(QStringLiteral("window/geometry")).toByteArray();
}

void setWindowGeometry(const QByteArray &geometry)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("window/geometry"), geometry);
}

QByteArray windowState()
{
	return store().value(QStringLiteral("window/state")).toByteArray();
}

void setWindowState(const QByteArray &state)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("window/state"), state);
}

QStringList recentFiles()
{
	return store().value(QStringLiteral("files/recent")).toStringList();
}

void setRecentFiles(const QStringList &files)
{
	if (!writable()) return;
	QSettings s = store();
	s.setValue(QStringLiteral("files/recent"), files);
}

} // namespace Settings
} // namespace w2r
