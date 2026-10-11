// RFFE probe dock: select a probe port, connect, scan a phone's RFFE bus, manage shields and
// their test points, and update the probe firmware. Mirrors the original app's RffePanel /
// RffeShieldList / RffeSettingsDialog surfaces.
#pragma once

#include "core/rffe/RffeModel.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;

namespace w2r {

class RffeProbe;
class RffeFlasher;

class RffePanel : public QWidget {
	Q_OBJECT
  public:
	explicit RffePanel(QWidget *parent = nullptr);

	// --- boardview integration ---------------------------------------------------
	// Records a test point collected from the boardview context menu ("Get Test Point").
	void addTestPoint(const QString &refDes, const QString &net, int role);
	// Sets the RFFE role (clk / 1v8 / data) for a net already collected, or adds it.
	void setTestPointRole(const QString &net, int role);
	// The net currently selected on the boardview (target for device mapping).
	void setCurrentNet(const QString &net);
	// The set of nets that should be highlighted/blinked on the boardview.
	QStringList markedNets() const;

  signals:
	// A detected RFFE device id; MainWindow may map it to a net and cross-probe.
	void deviceDetected(const QString &id);
	// The highlighted net set changed (test points / device marks edited).
	void markedNetsChanged(const QStringList &nets);
	// A scan/update should flash the marks on the boardview (pcb.anim.rffeBlink).
	void blinkRequested();
	// The persisted mark settings changed.
	void markSettingsChanged();

  private slots:
	void refreshPorts();
	void toggleConnection();
	void scan();
	void updateFirmware();

	void onShieldSelected();
	void onAddShield();
	void onRemoveShield();
	void onToggleShieldHidden();
	void onMapDeviceToNet();
	void onBlink();
	void onMarkSettingsEdited();

  private:
	void appendLog(const QString &text);
	void rebuildShields();
	void rebuildTestPoints();
	void rebuildDevices();
	void emitMarks();
	rffe::Shield &currentShieldOrDefault();
	void saveStore();
	RffeProbe *m_probe = nullptr;
	RffeFlasher *m_flasher = nullptr;
	rffe::Store m_store;
	rffe::MarkSettings m_marks;
	QString m_currentShield;
	QString m_currentNet;

	QComboBox *m_ports = nullptr;
	QPushButton *m_refresh = nullptr;
	QPushButton *m_connect = nullptr;
	QPushButton *m_scan = nullptr;
	QPushButton *m_flash = nullptr;
	QProgressBar *m_progress = nullptr;
	QPlainTextEdit *m_log = nullptr;
	QLabel *m_status = nullptr;

	QListWidget *m_shieldList = nullptr;
	QListWidget *m_tpList = nullptr;
	QListWidget *m_deviceList = nullptr;
	QPushButton *m_addShield = nullptr;
	QPushButton *m_removeShield = nullptr;
	QPushButton *m_hideShield = nullptr;
	QPushButton *m_mapDevice = nullptr;
	QPushButton *m_blink = nullptr;
	QLabel *m_tpSummary = nullptr;

	QSpinBox *m_blinkMs = nullptr;
	QSpinBox *m_holdMs = nullptr;
	QCheckBox *m_halo = nullptr;
	QCheckBox *m_esp32 = nullptr;
	QCheckBox *m_beep = nullptr;
};

} // namespace w2r
