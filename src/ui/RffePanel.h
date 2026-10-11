// RFFE probe dock panel.
//
// The original app's RffePanel: a card-styled dock that selects a probe port, connects, scans a
// phone's RFFE bus, manages shields and their expected devices / test points, and updates the
// probe firmware. Colours come from the rffe::Palette token stylesheet; the dialogs
// (RffeFlashDialog / RffeSettingsDialog / RffePromptDialog) are opened from here.
#pragma once

#include "core/rffe/RffeModel.h"
#include "core/rffe/RffeTheme.h"

#include <QWidget>

class QLabel;

namespace w2r {

class RffeProbe;
class RffeFlasher;

namespace rffe {
class RffeButton;
class RffeCombo;
class RffeCountChip;
class RffeReferenceList;
class RffeSegmented;
class RffeShieldList;
class RffeStatusLed;
class RffeTerminal;
} // namespace rffe

class RffePanel : public QWidget {
	Q_OBJECT
  public:
	explicit RffePanel(QWidget *parent = nullptr);

	// --- boardview integration ---------------------------------------------------
	void addTestPoint(const QString &refDes, const QString &net, int role);
	void setTestPointRole(const QString &net, int role);
	void setCurrentNet(const QString &net);
	QStringList markedNets() const;

	// --- menu entry points -------------------------------------------------------
	void openSettingsDialog();
	void openFlashDialog();
	void exportTestPoints();

  signals:
	void deviceDetected(const QString &id);
	void markedNetsChanged(const QStringList &nets);
	void blinkRequested();
	void markSettingsChanged();
	void closeRequested();

  private slots:
	void refreshPorts();
	void toggleConnection();
	void scan();
	void updateFirmware();

	void onShieldSelected();
	void onAddShield();
	void onRenameShield();
	void onRemoveShield();
	void onToggleShieldHidden();
	void onGetTestPoint();
	void onMapDeviceToNet();
	void onBlink();

  private:
	void appendLog(const QString &text, int level = 0); // 0 dim, 1 ok, 2 warn, 3 error
	void rebuildShields();
	void rebuildTestPoints();
	void rebuildDevices();
	void emitMarks();
	rffe::Shield &currentShieldOrDefault();
	void saveStore();

	rffe::Palette m_pal;
	RffeProbe *m_probe = nullptr;
	RffeFlasher *m_flasher = nullptr;
	rffe::Store m_store;
	rffe::MarkSettings m_marks;
	QString m_currentShield;
	QString m_currentNet;

	rffe::RffeCombo *m_ports = nullptr;
	rffe::RffeButton *m_refresh = nullptr;
	rffe::RffeButton *m_connect = nullptr;
	rffe::RffeButton *m_scan = nullptr;
	rffe::RffeButton *m_flash = nullptr;
	rffe::RffeTerminal *m_log = nullptr;
	rffe::RffeStatusLed *m_led = nullptr;
	QLabel *m_status = nullptr;
	rffe::RffeSegmented *m_segments = nullptr;

	rffe::RffeShieldList *m_shieldList = nullptr;
	rffe::RffeReferenceList *m_tpList = nullptr;
	rffe::RffeReferenceList *m_expectedList = nullptr;
	rffe::RffeReferenceList *m_deviceList = nullptr;
	rffe::RffeCountChip *m_countChip = nullptr;
	rffe::RffeButton *m_getTp = nullptr;
	rffe::RffeButton *m_mapDevice = nullptr;
	rffe::RffeButton *m_addShield = nullptr;
	rffe::RffeButton *m_renameShield = nullptr;
	rffe::RffeButton *m_removeShield = nullptr;
	rffe::RffeButton *m_hideShield = nullptr;
	rffe::RffeButton *m_blink = nullptr;
};

} // namespace w2r
