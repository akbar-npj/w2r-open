#include "core/Theme.h"

#include "core/Settings.h"

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

QJsonObject Theme::toJson() const
{
	QJsonObject o = raw; // keep unknown keys (base, baseLayer, componentFill, refEdge, ...)
	o.insert(QStringLiteral("name"), name);

	auto put = [&o](const char *key, const QColor &c) {
		o.insert(QLatin1String(key), c.name(QColor::HexArgb));
	};
	put("background", background);
	put("boardFill", boardFill);
	put("outline", outline);
	put("partOutline", partOutline);
	put("componentNameText", componentNameText);
	put("pin", pin);
	put("pinOutline", pinOutline);
	put("pinText", pinText);
	put("groundPin", groundPin);
	put("ncPin", ncPin);
	put("netText", netText);
	put("silkscreen", silkscreen);
	put("selectedPin", selectedPin);
	put("sameNetPin", sameNetPin);
	put("ratsnet", ratsnest); // original key spelling is "ratsnet"
	put("partHighlightFill", partHighlightFill);
	put("partHighlightBorder", partHighlightBorder);
	for (int i = 0; i < layers.size(); ++i)
		o.insert(QStringLiteral("layer%1").arg(i + 1), layers[i].name(QColor::HexArgb));
	return o;
}

QVector<Theme> ThemeManager::parseThemes(const QByteArray &json)
{
	QVector<Theme> out;
	const QJsonDocument doc = QJsonDocument::fromJson(json);
	if (!doc.isObject()) return out;
	const QJsonArray arr = doc.object().value(QStringLiteral("themes")).toArray();

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
		t.raw = o;
		out.append(t);
	}
	return out;
}

ThemeManager::ThemeManager()
{
	loadBuiltin();
	if (m_themes.isEmpty()) m_themes.append(defaultTheme());
	m_builtinCount = m_themes.size();
	loadUserThemes();
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
	const QVector<Theme> parsed = parseThemes(json);
	if (parsed.isEmpty()) return;
	m_themes = parsed;
	m_builtinCount = m_themes.size();
}

bool ThemeManager::loadUserThemes()
{
	const QByteArray json = Settings::userThemes();
	if (json.isEmpty()) return false;
	const QVector<Theme> users = parseThemes(json);
	for (const Theme &t : users) m_themes.append(t);
	return !users.isEmpty();
}

bool ThemeManager::saveUserThemes() const
{
	QJsonArray arr;
	for (int i = m_builtinCount; i < m_themes.size(); ++i) arr.append(m_themes[i].toJson());
	QJsonObject root;
	root.insert(QStringLiteral("themes"), arr);
	Settings::setUserThemes(QJsonDocument(root).toJson(QJsonDocument::Compact));
	return true;
}

int ThemeManager::addOrUpdate(const Theme &theme, int index)
{
	if (isUserTheme(index)) {
		m_themes[index] = theme;
		return index;
	}
	m_themes.append(theme);
	return m_themes.size() - 1;
}

bool ThemeManager::removeTheme(int index)
{
	if (!isUserTheme(index)) return false;
	m_themes.remove(index);
	return true;
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
