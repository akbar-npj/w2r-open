#include "core/rffe/RffeTheme.h"

#include "core/Settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace w2r {
namespace rffe {

namespace {

// Colour as a QSS literal: plain #rrggbb when opaque, rgba() when it carries alpha.
QString qssColor(const QColor &c)
{
	if (c.alpha() == 255) return c.name(QColor::HexRgb);
	return QStringLiteral("rgba(%1,%2,%3,%4)")
	    .arg(c.red())
	    .arg(c.green())
	    .arg(c.blue())
	    .arg(c.alpha());
}

// The stylesheet template. Every @RFFE_* token is substituted by styleSheet().
const char *kStyleTemplate = R"QSS(
RffePanel, RffePanel QWidget { background: transparent; }
QScrollArea { background: transparent; border: none; }
RffeCard { background: @RFFE_SURFACE; border: 1px solid @RFFE_BORDER; border-radius: 10px; }
RffeCard[raised="true"] { background: @RFFE_SURFACE_RAISED; }
RffeBrand { background: transparent; }
RffeBrand QLabel#brandMark { color: @RFFE_BRAND_RED; font-weight: 800; letter-spacing: 1px; }
RffeBrand QLabel#brandName { color: @RFFE_TEXT; font-weight: 700; letter-spacing: 2px; }
QLabel#rffeSectionTitle { color: @RFFE_TEXT; font-weight: 700; }
QLabel#rffeHint { color: @RFFE_TEXT_FAINT; font-size: 10px; }
QToolButton#rffeGear { background: transparent; border: none; color: @RFFE_TEXT_DIM; font-size: 13px; padding: 0; }
QToolButton#rffeGear:hover { color: @RFFE_ACCENT; }
QToolButton#rffeHelp { background: transparent; border: 1px solid @RFFE_BORDER; border-radius: 9px; color: @RFFE_TEXT_DIM; font-size: 11px; font-weight: 600; padding: 0; }
QToolButton#rffeHelp:hover { color: @RFFE_ACCENT; border-color: @RFFE_ACCENT; }
QToolButton#rffeClose { background: transparent; border: none; color: @RFFE_TEXT_DIM; font-size: 12px; padding: 0; }
QToolButton#rffeClose:hover { color: @RFFE_LED_DANGER; }
RffeButton { background: @RFFE_SURFACE_RAISED; border: 1px solid @RFFE_BORDER; border-radius: 8px; color: @RFFE_TEXT; padding: 6px 12px; font-weight: 600; }
RffeButton:hover { border-color: @RFFE_ACCENT; }
RffeButton[variant="primary"] { background: @RFFE_ACCENT; border-color: @RFFE_ACCENT_STRONG; color: @RFFE_ON_INK; }
RffeButton[variant="primary"]:hover { background: @RFFE_ACCENT_STRONG; }
RffeButton[variant="danger"] { color: @RFFE_LED_DANGER; border-color: @RFFE_LED_DANGER; }
RffeButton:disabled { color: @RFFE_TEXT_FAINT; border-color: @RFFE_BORDER_SOFT; background: @RFFE_SURFACE_SUNKEN; }
RffeCombo { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER; border-radius: 8px; color: @RFFE_TEXT; padding: 4px 8px; }
RffeCombo::drop-down { border: none; width: 18px; }
#rffeComboPopup { background: @RFFE_SURFACE_RAISED; border: 1px solid @RFFE_BORDER; border-radius: 6px; color: @RFFE_TEXT; }
#rffeComboPopup::item:selected { background: @RFFE_ACCENT_INK; color: @RFFE_TEXT; }
RffeCountChip { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER_SOFT; border-radius: 9px; color: @RFFE_TEXT_DIM; padding: 2px 8px; font-size: 10px; font-weight: 600; }
RffeSegmented { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER_SOFT; border-radius: 8px; }
RffeSegmented QPushButton { background: transparent; border: none; color: @RFFE_TEXT_DIM; padding: 4px 12px; }
RffeSegmented QPushButton:checked { background: @RFFE_SURFACE_RAISED; color: @RFFE_TEXT; border-radius: 6px; }
RffeTerminal { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER_SOFT; border-radius: 8px; color: @RFFE_TEXT_DIM; }
RffeReferenceList, RffeShieldList { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER_SOFT; border-radius: 8px; color: @RFFE_TEXT; }
RffeReferenceList::item, RffeShieldList::item { padding: 3px 6px; }
RffeReferenceList::item:selected, RffeShieldList::item:selected { background: @RFFE_ACCENT_INK; color: @RFFE_TEXT; }
QCheckBox { color: @RFFE_TEXT; }
QSpinBox { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER; border-radius: 6px; color: @RFFE_TEXT; padding: 2px 6px; }
QProgressBar { background: @RFFE_SURFACE_SUNKEN; border: 1px solid @RFFE_BORDER_SOFT; border-radius: 6px; text-align: center; color: @RFFE_TEXT_DIM; }
QProgressBar::chunk { background: @RFFE_ACCENT; border-radius: 5px; }
)QSS";

// The dialog sheets below are recovered verbatim from the original binary (the app builds them
// by substituting positional %1..%N colour arguments). They are applied on top of the panel
// sheet so the shared RffeButton / RffeCombo rules still reach the dialog's own widgets.
const char *kFlashSheet = R"QSS(QDialog, QDialog QWidget { background: transparent; }QWidget#rffeFlashCard { background:%1; border:1px solid %2; border-left:3px solid %3;  border-radius:10px; }QLabel { color:%4; font-family:'Segoe UI'; font-size:12px; }QLabel#rffeFlashTitle { color:%4; font-size:13px; font-weight:600; }QLabel#rffeNote { color:%5; font-size:10.5px; }QLabel#rffeStage { color:%4; font-size:11.5px; }QLabel#rffeWarn { color:%7; font-size:10.5px; }QProgressBar { background:%6; border:none; border-radius:3px; }QProgressBar::chunk { background:%3; border-radius:3px; }QPlainTextEdit#rffeFlashLog { background:%6; color:%8; border:1px solid %2;  border-radius:6px; padding:6px; selection-background-color:%3;  selection-color:%1; }QPlainTextEdit#rffeFlashLog > QWidget { background:%6; })QSS";

const char *kSettingsSheet = R"QSS(QDialog, QDialog QWidget { background: transparent; }QWidget#rffeSettingsCard { background:%1; border:1px solid %2; border-left:3px solid %3;  border-radius:10px; }QLabel { color:%4; font-family:'Segoe UI'; font-size:12px; }QLabel#rffeSettingsTitle { color:%4; font-size:13px; font-weight:600; }QLabel#rffeNote { color:%5; font-size:10.5px; }QCheckBox, QRadioButton { color:%4; font-family:'Segoe UI'; font-size:12px;  spacing:7px; background:transparent; }QCheckBox::indicator, QRadioButton::indicator { width:13px; height:13px;  border:1px solid %2; background:%6; }QCheckBox::indicator { border-radius:3px; }QRadioButton::indicator { border-radius:7px; }QCheckBox::indicator:checked, QRadioButton::indicator:checked {  background:%3; border-color:%3; }QSlider::groove:horizontal { height:4px; border-radius:2px; background:%6; }QSlider::sub-page:horizontal { height:4px; border-radius:2px; background:%3; }QSlider::handle:horizontal { width:13px; margin:-5px 0; border-radius:7px;  background:%3; border:2px solid %1; })QSS";

const char *kPromptSheet = R"QSS(QDialog, QDialog QWidget { background: transparent; }QWidget#rffePromptCard { background:%1; border:1px solid %2; border-left:3px solid %3;  border-radius:10px; }QLabel { color:%4; font-family:'Segoe UI'; font-size:12px; }QLabel#rffePromptTitle { color:%4; font-size:13px; font-weight:600; }QLabel#rffePromptText { color:%5; font-size:11.5px; }QLineEdit#rffePromptEdit { background:%6; border:1px solid %2; border-radius:6px;  padding:7px 9px; color:%4; font-family:'Segoe UI'; font-size:12px;  selection-background-color:%3; selection-color:%7; }QLineEdit#rffePromptEdit:focus { border-color:%3; })QSS";

// The flash log's vertical scrollbar (object name rffeFlashLogBar).
const char *kLogBarSheet = R"QSS(QScrollBar#rffeFlashLogBar:vertical { width:6px; background:transparent; margin:2px 0; }QScrollBar#rffeFlashLogBar::handle:vertical { background:%1; border-radius:3px;  min-height:24px; }QScrollBar#rffeFlashLogBar::handle:vertical:hover { background:%2; }QScrollBar#rffeFlashLogBar::add-line:vertical,QScrollBar#rffeFlashLogBar::sub-line:vertical { height:0; border:none; background:none; }QScrollBar#rffeFlashLogBar::add-page:vertical,QScrollBar#rffeFlashLogBar::sub-page:vertical { background:transparent; })QSS";

// Substitutes positional %1..%N arguments (the recovered sheets' placeholder scheme).
QString applyArgs(QString s, const QStringList &args)
{
	for (int i = args.size(); i >= 1; --i)
		s.replace(QLatin1Char('%') + QString::number(i), args.at(i - 1));
	return s;
}

const char *kTokens[] = {
    "RFFE_BG",           "RFFE_BACKDROP_DOT",   "RFFE_SURFACE",        "RFFE_SURFACE_RAISED",
    "RFFE_SURFACE_SUNKEN", "RFFE_BORDER",       "RFFE_BORDER_SOFT",    "RFFE_TEXT",
    "RFFE_TEXT_DIM",     "RFFE_TEXT_FAINT",     "RFFE_BRAND_RED",      "RFFE_ACCENT",
    "RFFE_ACCENT_STRONG", "RFFE_ACCENT_INK",    "RFFE_ON_INK",         "RFFE_LED_ON",
    "RFFE_LED_ON_GLOW",  "RFFE_LED_OFF",        "RFFE_LED_DANGER",     "RFFE_LED_DANGER_GLOW",
    "RFFE_LED_SCAN",     "RFFE_LED_SCAN_GLOW",  "RFFE_FOCUS_RING",     "RFFE_SHADOW_INSET",
    "RFFE_SHADOW_AMBIENT", "RFFE_SHADOW_CONTACT",
};

// Qt matches type selectors against QObject::metaObject()->className(). The recovered sheet
// uses the original app's bare class names ("RffeCard"), but our classes live in namespaces,
// so their class names are "w2r::rffe::RffeCard" / "w2r::RffePanel". Qt spells a namespace in
// a type selector with "--" instead of "::", so the bare names are rewritten to the qualified
// form here. Keeping the template verbatim above documents the recovered ground truth.
struct ClassRewrite {
	const char *bare;
	const char *qualified;
};
const ClassRewrite kClassRewrites[] = {
    {"RffePanel", "w2r--RffePanel"},
    {"RffeCard", "w2r--rffe--RffeCard"},
    {"RffeBrand", "w2r--rffe--RffeBrand"},
    {"RffeButton", "w2r--rffe--RffeButton"},
    {"RffeCombo", "w2r--rffe--RffeCombo"},
    {"RffeCountChip", "w2r--rffe--RffeCountChip"},
    {"RffeSegmented", "w2r--rffe--RffeSegmented"},
    {"RffeTerminal", "w2r--rffe--RffeTerminal"},
    {"RffeReferenceList", "w2r--rffe--RffeReferenceList"},
    {"RffeShieldList", "w2r--rffe--RffeShieldList"},
};

} // namespace

Palette Palette::defaultPalette()
{
	Palette p;
	p.bg = QColor(0xff0f1116);
	p.backdropDot = QColor(0xff1b1f28);
	p.surface = QColor(0xff161a22);
	p.surfaceRaised = QColor(0xff1d222c);
	p.surfaceSunken = QColor(0xff10131a);
	p.border = QColor(0xff2a3140);
	p.borderSoft = QColor(0xff232936);
	p.text = QColor(0xffe6e9ef);
	p.textDim = QColor(0xff9aa3b2);
	p.textFaint = QColor(0xff6b7280);
	p.brandRed = QColor(0xffe11d2e);
	p.accent = QColor(0xff3b82f6);
	p.accentStrong = QColor(0xff2563eb);
	p.accentInk = QColor(0xff152a4d);
	p.onInk = QColor(0xffffffff);
	p.ledOn = QColor(0xff22c55e);
	p.ledOnGlow = QColor(0x5522c55e);
	p.ledOff = QColor(0xff3a4150);
	p.ledDanger = QColor(0xffef4444);
	p.ledDangerGlow = QColor(0x55ef4444);
	p.ledScan = QColor(0xff3b82f6);
	p.ledScanGlow = QColor(0x553b82f6);
	p.focusRing = QColor(0xff3b82f6);
	p.shadowInset = QColor(0x66000000);
	p.shadowAmbient = QColor(0x40000000);
	p.shadowContact = QColor(0x22000000);
	return p;
}

QColor Palette::color(const QString &token) const
{
	if (token == QLatin1String("RFFE_BG")) return bg;
	if (token == QLatin1String("RFFE_BACKDROP_DOT")) return backdropDot;
	if (token == QLatin1String("RFFE_SURFACE")) return surface;
	if (token == QLatin1String("RFFE_SURFACE_RAISED")) return surfaceRaised;
	if (token == QLatin1String("RFFE_SURFACE_SUNKEN")) return surfaceSunken;
	if (token == QLatin1String("RFFE_BORDER")) return border;
	if (token == QLatin1String("RFFE_BORDER_SOFT")) return borderSoft;
	if (token == QLatin1String("RFFE_TEXT")) return text;
	if (token == QLatin1String("RFFE_TEXT_DIM")) return textDim;
	if (token == QLatin1String("RFFE_TEXT_FAINT")) return textFaint;
	if (token == QLatin1String("RFFE_BRAND_RED")) return brandRed;
	if (token == QLatin1String("RFFE_ACCENT")) return accent;
	if (token == QLatin1String("RFFE_ACCENT_STRONG")) return accentStrong;
	if (token == QLatin1String("RFFE_ACCENT_INK")) return accentInk;
	if (token == QLatin1String("RFFE_ON_INK")) return onInk;
	if (token == QLatin1String("RFFE_LED_ON")) return ledOn;
	if (token == QLatin1String("RFFE_LED_ON_GLOW")) return ledOnGlow;
	if (token == QLatin1String("RFFE_LED_OFF")) return ledOff;
	if (token == QLatin1String("RFFE_LED_DANGER")) return ledDanger;
	if (token == QLatin1String("RFFE_LED_DANGER_GLOW")) return ledDangerGlow;
	if (token == QLatin1String("RFFE_LED_SCAN")) return ledScan;
	if (token == QLatin1String("RFFE_LED_SCAN_GLOW")) return ledScanGlow;
	if (token == QLatin1String("RFFE_FOCUS_RING")) return focusRing;
	if (token == QLatin1String("RFFE_SHADOW_INSET")) return shadowInset;
	if (token == QLatin1String("RFFE_SHADOW_AMBIENT")) return shadowAmbient;
	if (token == QLatin1String("RFFE_SHADOW_CONTACT")) return shadowContact;
	return QColor();
}

QString Palette::styleSheet() const
{
	QString qss = QString::fromUtf8(kStyleTemplate);

	// Substitute the longest token names first: "RFFE_SURFACE_SUNKEN" must not be matched by
	// the shorter "RFFE_SURFACE" prefix, which would leave a stray "_SUNKEN" in the value
	// (an invalid QSS colour that Qt would drop, silently un-styling the rule).
	QVector<QString> tokens;
	tokens.reserve(int(sizeof(kTokens) / sizeof(kTokens[0])));
	for (const char *tok : kTokens) tokens.append(QLatin1String(tok));
	std::sort(tokens.begin(), tokens.end(),
	          [](const QString &a, const QString &b) { return a.size() > b.size(); });
	for (const QString &token : tokens)
		qss.replace(QLatin1Char('@') + token, qssColor(color(token)));

	// Rewrite bare class selectors to the namespaced form Qt expects (see kClassRewrites).
	for (const ClassRewrite &r : kClassRewrites) {
		const QRegularExpression re(QStringLiteral("\\b%1\\b").arg(QLatin1String(r.bare)));
		qss.replace(re, QLatin1String(r.qualified));
	}
	return qss;
}

QString Palette::flashDialogStyleSheet() const
{
	const QString card = applyArgs(QString::fromUtf8(kFlashSheet),
	                               {qssColor(surface), qssColor(border), qssColor(accent), qssColor(text),
	                                qssColor(textDim), qssColor(surfaceSunken), qssColor(ledDanger),
	                                qssColor(textDim)});
	const QString bar = applyArgs(QString::fromUtf8(kLogBarSheet),
	                              {qssColor(textFaint), qssColor(accent)});
	return card + bar;
}

QString Palette::settingsDialogStyleSheet() const
{
	return applyArgs(QString::fromUtf8(kSettingsSheet),
	                 {qssColor(surface), qssColor(border), qssColor(accent), qssColor(text),
	                  qssColor(textFaint), qssColor(surfaceSunken)});
}

QString Palette::promptDialogStyleSheet() const
{
	return applyArgs(QString::fromUtf8(kPromptSheet),
	                 {qssColor(surface), qssColor(border), qssColor(accent), qssColor(text),
	                  qssColor(textDim), qssColor(surfaceSunken), qssColor(onInk)});
}

QByteArray Palette::toJson() const
{
	QJsonObject o;
	for (const char *tok : kTokens) {
		const QString token = QLatin1String(tok);
		o.insert(token, color(token).name(QColor::HexArgb));
	}
	return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

Palette Palette::fromJson(const QByteArray &json)
{
	Palette p = defaultPalette();
	const QJsonObject o = QJsonDocument::fromJson(json).object();
	if (o.isEmpty()) return p;
	auto pick = [&o](const char *tok, QColor &dst) {
		const QJsonValue v = o.value(QLatin1String(tok));
		if (v.isString()) {
			const QColor c(v.toString());
			if (c.isValid()) dst = c;
		}
	};
	pick("RFFE_BG", p.bg);
	pick("RFFE_BACKDROP_DOT", p.backdropDot);
	pick("RFFE_SURFACE", p.surface);
	pick("RFFE_SURFACE_RAISED", p.surfaceRaised);
	pick("RFFE_SURFACE_SUNKEN", p.surfaceSunken);
	pick("RFFE_BORDER", p.border);
	pick("RFFE_BORDER_SOFT", p.borderSoft);
	pick("RFFE_TEXT", p.text);
	pick("RFFE_TEXT_DIM", p.textDim);
	pick("RFFE_TEXT_FAINT", p.textFaint);
	pick("RFFE_BRAND_RED", p.brandRed);
	pick("RFFE_ACCENT", p.accent);
	pick("RFFE_ACCENT_STRONG", p.accentStrong);
	pick("RFFE_ACCENT_INK", p.accentInk);
	pick("RFFE_ON_INK", p.onInk);
	pick("RFFE_LED_ON", p.ledOn);
	pick("RFFE_LED_ON_GLOW", p.ledOnGlow);
	pick("RFFE_LED_OFF", p.ledOff);
	pick("RFFE_LED_DANGER", p.ledDanger);
	pick("RFFE_LED_DANGER_GLOW", p.ledDangerGlow);
	pick("RFFE_LED_SCAN", p.ledScan);
	pick("RFFE_LED_SCAN_GLOW", p.ledScanGlow);
	pick("RFFE_FOCUS_RING", p.focusRing);
	pick("RFFE_SHADOW_INSET", p.shadowInset);
	pick("RFFE_SHADOW_AMBIENT", p.shadowAmbient);
	pick("RFFE_SHADOW_CONTACT", p.shadowContact);
	return p;
}

Palette Palette::load()
{
	const QByteArray json = Settings::rffePalette();
	if (json.isEmpty()) return defaultPalette();
	return fromJson(json);
}

void Palette::save() const { Settings::setRffePalette(toJson()); }

} // namespace rffe
} // namespace w2r
