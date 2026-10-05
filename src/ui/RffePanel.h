// RFFE probe dock: select a probe port, connect, scan a phone's RFFE bus, and update the
// probe firmware. Mirrors the original app's RffePanel.
#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace w2r {

class RffeProbe;
class RffeFlasher;

class RffePanel : public QWidget {
	Q_OBJECT
  public:
	explicit RffePanel(QWidget *parent = nullptr);

  signals:
	// A detected RFFE device id; MainWindow may map it to a net and cross-probe.
	void deviceDetected(const QString &id);

  private slots:
	void refreshPorts();
	void toggleConnection();
	void scan();
	void updateFirmware();

  private:
	void appendLog(const QString &text);

	RffeProbe *m_probe = nullptr;
	RffeFlasher *m_flasher = nullptr;

	QComboBox *m_ports = nullptr;
	QPushButton *m_refresh = nullptr;
	QPushButton *m_connect = nullptr;
	QPushButton *m_scan = nullptr;
	QPushButton *m_flash = nullptr;
	QProgressBar *m_progress = nullptr;
	QPlainTextEdit *m_log = nullptr;
	QLabel *m_status = nullptr;
};

} // namespace w2r
