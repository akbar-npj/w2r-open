#include "core/Theme.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace w2r {

namespace {

QColor col(const QJsonObject &o, const QString &key, const QColor &fallback)
{
	const QString s = o.value(key).toString();
	if (s.isEmpty()) return fallback;
	const QColor c(s);
	return c.isValid() ? c : fallback;
}

Theme defaultTheme()
{
	Theme t;
	t.name = QStringLiteral("Default");
	t.background = QColor(0xff232323);
	t.boardFill = QColor(0xff272727);
	t.outline = QColor(0xff3e7597);
	t.partOutline = QColor(0xff3e7597);
	t.componentNameText = QColor(0xffffffff);
	t.pin = QColor(0xffdea769);
	t.pinOutline = QColor(0xff041018);
	t.pinText = QColor(0xff06202f);
	t.groundPin = QColor(0xff9aa4ae);
	t.ncPin = QColor(0xff1a989b);
	t.netText = QColor(0xff123246);
	t.silkscreen = QColor(0xc0dddddd);
	t.selectedPin = QColor(0xffc21ac3);
	t.sameNetPin = QColor(0xffc21ac3);
	t.ratsnest = QColor(0xffc21ac3);
	t.partHighlightFill = QColor(0xff00a2e8);
	t.partHighlightBorder = QColor(0xffffffff);
	for (int i = 0; i < 16; ++i) t.layers.append(QColor(0xa75b8fd6));
	return t;
}

} // namespace

ThemeManager::ThemeManager()
{
	loadBuiltin();
	if (m_themes.isEmpty()) m_themes.append(defaultTheme());
}

bool ThemeManager::loadBuiltin()
{
	QFile f(QStringLiteral(":/resources/themes.json"));
	if (!f.open(QIODevice::ReadOnly)) return false;
	parse(f.readAll());
	return !m_themes.isEmpty();
}

bool ThemeManager::loadFromFile(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return false;
	parse(f.readAll());
	return !m_themes.isEmpty();
}

void ThemeManager::parse(const QByteArray &json)
{
	const QJsonDocument doc = QJsonDocument::fromJson(json);
	if (!doc.isObject()) return;
	const QJsonArray arr = doc.object().value(QStringLiteral("themes")).toArray();
	if (arr.isEmpty()) return;

	m_themes.clear();
	for (const QJsonValue &v : arr) {
		const QJsonObject o = v.toObject();
		Theme t = defaultTheme();
		t.name = o.value(QStringLiteral("name")).toString(QStringLiteral("Theme"));
		t.background = col(o, QStringLiteral("background"), t.background);
		t.boardFill = col(o, QStringLiteral("boardFill"), t.boardFill);
		t.outline = col(o, QStringLiteral("outline"), t.outline);
		t.partOutline = col(o, QStringLiteral("partOutline"), t.partOutline);
		t.componentNameText = col(o, QStringLiteral("componentNameText"), t.componentNameText);
		t.pin = col(o, QStringLiteral("pin"), t.pin);
		t.pinOutline = col(o, QStringLiteral("pinOutline"), t.pinOutline);
		t.pinText = col(o, QStringLiteral("pinText"), t.pinText);
		t.groundPin = col(o, QStringLiteral("groundPin"), t.groundPin);
		t.ncPin = col(o, QStringLiteral("ncPin"), t.ncPin);
		t.netText = col(o, QStringLiteral("netText"), t.netText);
		t.silkscreen = col(o, QStringLiteral("silkscreen"), t.silkscreen);
		t.selectedPin = col(o, QStringLiteral("selectedPin"), t.selectedPin);
		t.sameNetPin = col(o, QStringLiteral("sameNetPin"), t.sameNetPin);
		t.ratsnest = col(o, QStringLiteral("ratsnet"), t.ratsnest);
		t.partHighlightFill = col(o, QStringLiteral("partHighlightFill"), t.partHighlightFill);
		t.partHighlightBorder = col(o, QStringLiteral("partHighlightBorder"), t.partHighlightBorder);

		t.layers.clear();
		for (int i = 1; i <= 16; ++i) {
			const QColor c = col(o, QStringLiteral("layer%1").arg(i), QColor(0xa75b8fd6));
			t.layers.append(c);
		}
		m_themes.append(t);
	}
}

QStringList ThemeManager::names() const
{
	QStringList out;
	for (const Theme &t : m_themes) out << t.name;
	return out;
}

const Theme &ThemeManager::theme(int index) const
{
	static Theme fallback = defaultTheme();
	if (index < 0 || index >= m_themes.size()) return fallback;
	return m_themes[index];
}

int ThemeManager::indexOfName(const QString &name) const
{
	for (int i = 0; i < m_themes.size(); ++i)
		if (m_themes[i].name == name) return i;
	return 0;
}

} // namespace w2r
