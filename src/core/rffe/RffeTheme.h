// RFFE panel palette + stylesheet.
//
// The original app themes its RFFE surfaces from a set of named colour tokens (RFFE_BG,
// RFFE_SURFACE*, RFFE_TEXT*, RFFE_LED_*, RFFE_SHADOW_*, ...) that it substitutes into a Qt
// stylesheet written with "@RFFE_*" placeholders. This mirrors that scheme: a Palette holds the
// 26 token colours and builds the panel's QSS. It is deliberately separate from the PCB Theme
// (which round-trips the original pcb_themes.json schema) so that schema stays untouched.
#pragma once

#include <QByteArray>
#include <QColor>
#include <QString>

namespace w2r {
namespace rffe {

struct Palette {
	// Surface / structure.
	QColor bg;             // RFFE_BG
	QColor backdropDot;    // RFFE_BACKDROP_DOT
	QColor surface;        // RFFE_SURFACE
	QColor surfaceRaised;  // RFFE_SURFACE_RAISED
	QColor surfaceSunken;  // RFFE_SURFACE_SUNKEN
	QColor border;         // RFFE_BORDER
	QColor borderSoft;     // RFFE_BORDER_SOFT
	// Text.
	QColor text;           // RFFE_TEXT
	QColor textDim;        // RFFE_TEXT_DIM
	QColor textFaint;      // RFFE_TEXT_FAINT
	// Brand / accent.
	QColor brandRed;       // RFFE_BRAND_RED
	QColor accent;         // RFFE_ACCENT
	QColor accentStrong;   // RFFE_ACCENT_STRONG
	QColor accentInk;      // RFFE_ACCENT_INK
	QColor onInk;          // RFFE_ON_INK
	// Status LEDs.
	QColor ledOn;          // RFFE_LED_ON
	QColor ledOnGlow;      // RFFE_LED_ON_GLOW
	QColor ledOff;         // RFFE_LED_OFF
	QColor ledDanger;      // RFFE_LED_DANGER
	QColor ledDangerGlow;  // RFFE_LED_DANGER_GLOW
	QColor ledScan;        // RFFE_LED_SCAN
	QColor ledScanGlow;    // RFFE_LED_SCAN_GLOW
	// Focus + shadows.
	QColor focusRing;      // RFFE_FOCUS_RING
	QColor shadowInset;    // RFFE_SHADOW_INSET
	QColor shadowAmbient;  // RFFE_SHADOW_AMBIENT
	QColor shadowContact;  // RFFE_SHADOW_CONTACT

	static Palette defaultPalette();

	// Token name (without '@', e.g. "RFFE_BG") -> colour; invalid when unknown.
	QColor color(const QString &token) const;

	// The full panel stylesheet, with every @RFFE_* placeholder resolved to a colour.
	QString styleSheet() const;

	// The per-dialog stylesheets, recovered verbatim from the original app (which builds them
	// by substituting positional %1..%N colour args). Applied on top of styleSheet() so the
	// shared RffeButton / RffeCombo rules still cover the dialog's own widgets.
	QString flashDialogStyleSheet() const;
	QString settingsDialogStyleSheet() const;
	QString promptDialogStyleSheet() const;

	QByteArray toJson() const;
	static Palette fromJson(const QByteArray &json);

	// Loads from Settings (key rffe/palette); falls back to defaultPalette().
	static Palette load();
	void save() const;
};

} // namespace rffe
} // namespace w2r
