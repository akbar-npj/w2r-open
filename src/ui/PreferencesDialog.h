// Preferences dialog: library roots, default theme and view toggles.
//
// Values are read from / written to w2r::Settings. The dialog reports whether the library
// roots changed so the caller can trigger a rescan.
#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QCheckBox;
class QListWidget;

namespace w2r {

class PreferencesDialog : public QDialog {
	Q_OBJECT
  public:
	explicit PreferencesDialog(const QStringList &themeNames, QWidget *parent = nullptr);

	QStringList libraryRoots() const;
	QString themeName() const;
	bool showDiodeReadings() const;
	bool showRatsnest() const;

  private slots:
	void addRoot();
	void removeRoot();

  private:
	QListWidget *m_roots = nullptr;
	QComboBox *m_theme = nullptr;
	QCheckBox *m_diode = nullptr;
	QCheckBox *m_ratsnest = nullptr;
};

} // namespace w2r
