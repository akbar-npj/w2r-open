// RFFE firmware-update dialog.
//
// Drives RffeFlasher (esptool) while showing the original app's staged progress: a card with a
// stage label, a progress bar, a scrolling log, an activity bar and a warning line. The stage
// names and messages mirror the strings recovered from W2RSolutions.exe.
#pragma once

#include "core/rffe/RffeTheme.h"

#include <QDialog>
#include <QString>

class QLabel;
class QProgressBar;
class QPushButton;

namespace w2r {

class RffeFlasher;

namespace rffe {

class RffeTerminal;

class RffeFlashDialog : public QDialog {
	Q_OBJECT
  public:
	explicit RffeFlashDialog(const Palette &pal, RffeFlasher *flasher, const QString &port,
	                         QWidget *parent = nullptr);

  private slots:
	void onOutput(const QString &line);
	void onProgress(int percent);
	void onFinished(bool ok, const QString &message);
	void onCancel();

  private:
	void setStage(const QString &stage);

	Palette m_pal;
	RffeFlasher *m_flasher = nullptr;
	QString m_port;
	RffeTerminal *m_log = nullptr;
	QLabel *m_stage = nullptr;
	QLabel *m_warn = nullptr;
	QProgressBar *m_progress = nullptr;
	QProgressBar *m_logBar = nullptr;
	QPushButton *m_cancel = nullptr;
	bool m_running = true;
};

} // namespace rffe
} // namespace w2r
