// Theme editor: create or edit a PCB colour theme with per-field colour pickers.
// Mirrors the original app's theme.create / theme.edit dialogs.
#pragma once

#include "core/Theme.h"

#include <QDialog>
#include <QVector>

#include <functional>

class QFormLayout;
class QLineEdit;
class QPushButton;

namespace w2r {

class ThemeEditorDialog : public QDialog {
	Q_OBJECT
  public:
	// Pass a copy of the theme to edit; for a new theme, pass a default-constructed one.
	explicit ThemeEditorDialog(const Theme &theme, QWidget *parent = nullptr);

	// The edited theme (valid after accept()).
	Theme theme() const { return m_theme; }

  signals:
	// Emitted whenever a colour changes so the caller can live-preview.
	void themeChanged(const Theme &theme);

  protected:
	void accept() override;

  private:
	// Adds a labelled colour-swatch row whose button opens a colour picker.
	void addColorRow(QFormLayout *form, const QString &label, const std::function<QColor()> &get,
	                 const std::function<void(const QColor &)> &set);
	void refreshSwatches();

	Theme m_theme;
	QLineEdit *m_nameEdit = nullptr;
	QVector<QPushButton *> m_swatches;
	QVector<std::function<QColor()>> m_getters;
};

} // namespace w2r
