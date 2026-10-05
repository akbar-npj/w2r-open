// PCB viewer colour themes, imported from the original application's
// pcb_themes_builtin.json (Graphite / Navy / Plum). Colours are Qt #AARRGGBB strings.
#pragma once

#include <QColor>
#include <QString>
#include <QVector>

namespace w2r {

struct Theme {
	QString name = QStringLiteral("Default");
	QColor background = QColor(0xff232323);
	QColor boardFill = QColor(0xff272727);
	QColor outline = QColor(0xff3e7597);
	QColor partOutline = QColor(0xff3e7597);
	QColor componentNameText = QColor(0xffffffff);
	QColor pin = QColor(0xffdea769);
	QColor pinOutline = QColor(0xff041018);
	QColor pinText = QColor(0xff06202f);
	QColor groundPin = QColor(0xff9aa4ae);
	QColor ncPin = QColor(0xff1a989b);
	QColor netText = QColor(0xff123246);
	QColor silkscreen = QColor(0xc0dddddd);
	QColor selectedPin = QColor(0xffc21ac3);
	QColor sameNetPin = QColor(0xffc21ac3);
	QColor ratsnest = QColor(0xffc21ac3);
	QColor partHighlightFill = QColor(0xff00a2e8);
	QColor partHighlightBorder = QColor(0xffffffff);
	QVector<QColor> layers; // 16 copper layers
};

class ThemeManager {
  public:
	ThemeManager();

	// Loads themes from a JSON file shaped like the original pcb_themes_builtin.json.
	bool loadFromFile(const QString &path);
	// Loads the themes embedded in the executable (:/resources/themes.json).
	bool loadBuiltin();

	int count() const { return m_themes.size(); }
	QStringList names() const;
	const Theme &theme(int index) const;
	int indexOfName(const QString &name) const;

  private:
	void parse(const QByteArray &json);
	QVector<Theme> m_themes;
};

} // namespace w2r
