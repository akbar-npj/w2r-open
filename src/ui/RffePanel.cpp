#include "ui/RffePanel.h"

#include "core/rffe/RffeFlasher.h"
#include "core/rffe/RffeProbe.h"
#include "ui/rffe/RffeFlashDialog.h"
#include "ui/rffe/RffePromptDialog.h"
#include "ui/rffe/RffeSettingsDialog.h"
#include "ui/rffe/RffeShieldList.h"
#include "ui/rffe/RffeWidgets.h"

#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace w2r {

using rffe::DeviceMark;
using rffe::RffeButton;
using rffe::RffeCombo;
using rffe::RffeCountChip;
using rffe::RffeReferenceList;
using rffe::RffeSegmented;
using rffe::RffeShieldList;
using rffe::RffeStatusLed;
using rffe::RffeTerminal;
using rffe::Role;
using rffe::Shield;
using rffe::TestPoint;

namespace {
QString roleLabel(Role r)
{
	switch (r) {
	case Role::Clock: return QStringLiteral("CLK");
	case Role::Data: return QStringLiteral("DATA");
	case Role::V1V8: return QStringLiteral("1V8");
	case Role::None: break;
	}
	return QStringLiteral("—");
}

QToolButton *headerButton(QWidget *parent, const QString &objectName, const QString &glyph,
                          const QString &tooltip)
{
	auto *b = new QToolButton(parent);
	b->setObjectName(objectName);
	b->setText(glyph);
	b->setToolTip(tooltip);
	b->setCursor(Qt::PointingHandCursor);
	b->setFixedSize(20, 20);
	return b;
}
} // namespace

RffePanel::RffePanel(QWidget *parent) : QWidget(parent)
{
	m_pal = rffe::Palette::load();
	m_marks = rffe::MarkSettings::load();
	m_store.load();

	setObjectName(QStringLiteral("rffePanel"));
	// The recovered sheet makes the panel transparent (the original's dock paints the RFFE_BG
	// backdrop). Our dock is not themed, so the panel supplies the backdrop itself; the child
	// widgets stay transparent as in the original.
	setStyleSheet(m_pal.styleSheet() +
	              QStringLiteral("#rffePanel { background: %1; }").arg(m_pal.bg.name(QColor::HexRgb)));

	// ---- scroll container ----
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	outer->addWidget(scroll);
	auto *content = new QWidget(scroll);
	scroll->setWidget(content);
	auto *root = new QVBoxLayout(content);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	// ---- header: brand + gear/help/close ----
	auto *header = new QHBoxLayout();
	header->addWidget(new rffe::RffeBrand(content));
	header->addStretch(1);
	auto *gear = headerButton(content, QStringLiteral("rffeGear"), QStringLiteral("\u2699"),
	                          tr("RFFE settings"));
	auto *help = headerButton(content, QStringLiteral("rffeHelp"), QStringLiteral("?"),
	                          tr("ESP32 wiring — where CLK, DATA and 1V8 connect"));
	auto *close = headerButton(content, QStringLiteral("rffeClose"), QStringLiteral("\u2715"),
	                           tr("Close the RFFE panel"));
	header->addWidget(gear);
	header->addWidget(help);
	header->addWidget(close);
	root->addLayout(header);
	connect(gear, &QToolButton::clicked, this, &RffePanel::openSettingsDialog);
	connect(help, &QToolButton::clicked, this, [this] {
		QMessageBox::information(this, tr("ESP32 wiring"),
		                         tr("ESP32 wiring — where CLK, DATA and 1V8 connect"));
	});
	connect(close, &QToolButton::clicked, this, &RffePanel::closeRequested);

	// ---- probe card ----
	auto *probeCard = new rffe::RffeCard(m_pal, QString(), content);
	auto *pb = probeCard->body();

	m_segments = new RffeSegmented(probeCard);
	m_segments->addSegment(tr("Model"));
	m_segments->addSegment(tr("Bus"));
	pb->addWidget(m_segments);

	auto *portRow = new QHBoxLayout();
	m_ports = new RffeCombo(m_pal, probeCard);
	m_ports->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	portRow->addWidget(m_ports, 1);
	m_refresh = new RffeButton(tr("Refresh"), RffeButton::Normal, probeCard);
	connect(m_refresh, &QPushButton::clicked, this, &RffePanel::refreshPorts);
	portRow->addWidget(m_refresh);
	pb->addLayout(portRow);

	auto *statusRow = new QHBoxLayout();
	m_led = new RffeStatusLed(m_pal, probeCard);
	statusRow->addWidget(m_led);
	m_status = new QLabel(tr("Select a COM Port"), probeCard);
	m_status->setObjectName(QStringLiteral("rffeHint"));
	statusRow->addWidget(m_status, 1);
	pb->addLayout(statusRow);

	auto *actions = new QHBoxLayout();
	m_connect = new RffeButton(tr("Connect"), RffeButton::Primary, probeCard);
	connect(m_connect, &QPushButton::clicked, this, &RffePanel::toggleConnection);
	actions->addWidget(m_connect);
	m_scan = new RffeButton(tr("Scan RFFE Modules"), RffeButton::Normal, probeCard);
	m_scan->setEnabled(false);
	connect(m_scan, &QPushButton::clicked, this, &RffePanel::scan);
	actions->addWidget(m_scan);
	m_flash = new RffeButton(tr("Update Firmware"), RffeButton::Normal, probeCard);
	connect(m_flash, &QPushButton::clicked, this, &RffePanel::updateFirmware);
	actions->addWidget(m_flash);
	actions->addStretch(1);
	pb->addLayout(actions);

	m_log = new RffeTerminal(m_pal, probeCard);
	m_log->setMinimumHeight(120);
	pb->addWidget(m_log, 1);

	auto *clearRow = new QHBoxLayout();
	clearRow->addStretch(1);
	auto *clear = new RffeButton(tr("Clear"), RffeButton::Normal, probeCard);
	connect(clear, &QPushButton::clicked, this, [this] { m_log->clear(); });
	clearRow->addWidget(clear);
	pb->addLayout(clearRow);

	root->addWidget(probeCard);

	// ---- devices card (Detected / Expected) ----
	auto *devCard = new rffe::RffeCard(m_pal, QString(), content);
	auto *db = devCard->body();
	auto *devTitle = new QLabel(tr("Detected Devices"), devCard);
	devTitle->setObjectName(QStringLiteral("rffeSectionTitle"));
	db->addWidget(devTitle);

	m_deviceList = new RffeReferenceList(devCard);
	m_deviceList->setEmptyHint(tr("Connect and scan to detect RFFE devices..."));
	m_deviceList->setMinimumHeight(70);
	db->addWidget(m_deviceList);

	m_expectedList = new RffeReferenceList(devCard);
	m_expectedList->setEmptyHint(tr("No devices listed for this shield yet."));
	m_expectedList->setMinimumHeight(70);
	m_expectedList->hide();
	db->addWidget(m_expectedList);

	auto *devRow = new QHBoxLayout();
	m_getTp = new RffeButton(tr("Get Test Point"), RffeButton::Normal, devCard);
	m_getTp->setEnabled(false);
	connect(m_getTp, &QPushButton::clicked, this, &RffePanel::onGetTestPoint);
	devRow->addWidget(m_getTp);
	m_mapDevice = new RffeButton(tr("Map to '%1'").arg(tr("selected net")), RffeButton::Normal, devCard);
	connect(m_mapDevice, &QPushButton::clicked, this, &RffePanel::onMapDeviceToNet);
	devRow->addWidget(m_mapDevice);
	devRow->addStretch(1);
	db->addLayout(devRow);

	m_countChip = new RffeCountChip(devCard);
	db->addWidget(m_countChip);
	root->addWidget(devCard);

	connect(m_segments, &RffeSegmented::currentChanged, this, [this, devTitle](int index) {
		const bool model = (index == 0);
		devTitle->setText(model ? tr("Expected Devices") : tr("Detected Devices"));
		m_expectedList->setVisible(model);
		m_deviceList->setVisible(!model);
	});

	// ---- shields card ----
	auto *shCard = new rffe::RffeCard(m_pal, QString(), content);
	auto *sb = shCard->body();
	auto *shTitle = new QLabel(tr("Select a Shield"), shCard);
	shTitle->setObjectName(QStringLiteral("rffeSectionTitle"));
	sb->addWidget(shTitle);

	m_shieldList = new RffeShieldList(shCard);
	m_shieldList->setMinimumHeight(80);
	connect(m_shieldList, &QListWidget::itemSelectionChanged, this, &RffePanel::onShieldSelected);
	sb->addWidget(m_shieldList);

	auto *shRow = new QHBoxLayout();
	m_addShield = new RffeButton(tr("Add shield"), RffeButton::Normal, shCard);
	m_renameShield = new RffeButton(tr("Rename shield"), RffeButton::Normal, shCard);
	m_hideShield = new RffeButton(tr("Hide shield"), RffeButton::Normal, shCard);
	m_removeShield = new RffeButton(tr("Remove shield"), RffeButton::Danger, shCard);
	connect(m_addShield, &QPushButton::clicked, this, &RffePanel::onAddShield);
	connect(m_renameShield, &QPushButton::clicked, this, &RffePanel::onRenameShield);
	connect(m_hideShield, &QPushButton::clicked, this, &RffePanel::onToggleShieldHidden);
	connect(m_removeShield, &QPushButton::clicked, this, &RffePanel::onRemoveShield);
	shRow->addWidget(m_addShield);
	shRow->addWidget(m_renameShield);
	shRow->addWidget(m_hideShield);
	shRow->addWidget(m_removeShield);
	shRow->addStretch(1);
	sb->addLayout(shRow);

	auto *tpTitle = new QLabel(tr("RFFE test points"), shCard);
	tpTitle->setObjectName(QStringLiteral("rffeSectionTitle"));
	sb->addWidget(tpTitle);
	m_tpList = new RffeReferenceList(shCard);
	m_tpList->setEmptyHint(tr("No test points mapped for this shield yet"));
	m_tpList->setMinimumHeight(80);
	sb->addWidget(m_tpList);

	auto *tpRow = new QHBoxLayout();
	m_blink = new RffeButton(tr("Blink RFFE nets"), RffeButton::Normal, shCard);
	connect(m_blink, &QPushButton::clicked, this, &RffePanel::onBlink);
	tpRow->addWidget(m_blink);
	auto *exportBtn = new RffeButton(tr("Export RFFE test points"), RffeButton::Normal, shCard);
	connect(exportBtn, &QPushButton::clicked, this, &RffePanel::exportTestPoints);
	tpRow->addWidget(exportBtn);
	tpRow->addStretch(1);
	sb->addLayout(tpRow);
	root->addWidget(shCard);
	root->addStretch(1);

	// ---- probe / flasher wiring ----
	m_probe = new RffeProbe(this);
	connect(m_probe, &RffeProbe::connected, this, [this](const QString &p) {
		appendLog(tr("[RFFE] Connected to %1 at %2 baud.").arg(p).arg(m_probe->baudRate()), 1);
		m_connect->setText(tr("Disconnect"));
		m_scan->setEnabled(true);
		m_led->setState(RffeStatusLed::On);
		m_status->setText(tr("Probe: %1").arg(p));
	});
	connect(m_probe, &RffeProbe::disconnected, this, [this] {
		appendLog(tr("[RFFE] Disconnected from probe."));
		m_connect->setText(tr("Connect"));
		m_scan->setEnabled(false);
		m_led->setState(RffeStatusLed::Off);
		m_status->setText(tr("Not connected."));
	});
	connect(m_probe, &RffeProbe::lineReceived, this, [this](const QString &l) { appendLog(l); });
	connect(m_probe, &RffeProbe::dataReceived, this, [this](const QByteArray &d) {
		if (!d.contains('\n') && !d.contains('\r')) appendLog(QString::fromLatin1(d));
	});
	connect(m_probe, &RffeProbe::deviceDetected, this, [this](const QString &id) {
		m_status->setText(tr("Detected RFFE device %1").arg(id));
		if (m_deviceList->findItems(id, Qt::MatchExactly).isEmpty()) {
			m_deviceList->setItems([&] {
				QStringList items;
				for (int i = 0; i < m_deviceList->count(); ++i) items << m_deviceList->item(i)->text();
				items << id;
				return items;
			}());
		}
		emit deviceDetected(id);
	});
	connect(m_probe, &RffeProbe::errorOccurred, this, [this](const QString &e) {
		appendLog(tr("[RFFE ERROR] %1").arg(e), 3);
	});

	m_flasher = new RffeFlasher(this);

	refreshPorts();
	if (!RffeProbe::serialSupported()) {
		appendLog(tr("[RFFE] This build has no Qt SerialPort support."), 3);
		m_connect->setEnabled(false);
		m_flash->setEnabled(false);
	}

	rebuildShields();
	if (!m_store.shields().isEmpty()) m_currentShield = m_store.shields().first().name;
	rebuildTestPoints();
	rebuildDevices();
	emitMarks();
}

// ------------------------------------------------------------------ probe actions

void RffePanel::refreshPorts()
{
	m_ports->clear();
	const QVector<RffePort> ports = RffeProbe::availablePorts();
	for (const RffePort &p : ports) {
		QString detail = p.label();
		if (p.isUsb)
			detail += QStringLiteral("  [VID_%1 PID_%2]")
			              .arg(p.vid, 4, 16, QLatin1Char('0'))
			              .arg(p.pid, 4, 16, QLatin1Char('0'));
		m_ports->addItem(detail, p.name);
	}
	if (ports.isEmpty()) {
		m_ports->addItem(tr("Select a COM Port"), QString());
		m_status->setText(tr("No serial ports found."));
		m_led->setState(RffeStatusLed::Danger);
	} else {
		m_status->setText(tr("%1 port(s) available.").arg(ports.size()));
	}
}

void RffePanel::toggleConnection()
{
	if (m_probe->isOpen()) {
		m_probe->close();
		return;
	}
	const QString port = m_ports->currentData().toString();
	if (port.isEmpty()) {
		appendLog(tr("[RFFE] Select a COM port."), 2);
		return;
	}
	QString error;
	if (!m_probe->open(port, 115200, &error))
		appendLog(tr("[RFFE ERROR] Can't open %1: %2").arg(port, error), 3);
}

void RffePanel::scan()
{
	if (!m_probe->isOpen()) return;
	m_led->setState(RffeStatusLed::Scan);
	appendLog(tr("Scanning RFFE bus"), 2);
	m_status->setText(tr("Scanning..."));
	QString error;
	if (!m_probe->sendScan(&error)) {
		appendLog(tr("[RFFE ERROR] Couldn't send the scan command: %1").arg(error), 3);
		m_led->setState(RffeStatusLed::Danger);
	}
}

void RffePanel::updateFirmware() { openFlashDialog(); }

void RffePanel::openFlashDialog()
{
	const QString port = m_probe->isOpen() ? m_probe->portName() : m_ports->currentData().toString();
	if (port.isEmpty()) {
		appendLog(tr("[RFFE] Pick the probe's COM port first."), 2);
		return;
	}
	if (m_probe->isOpen()) m_probe->close(); // the updater needs the port to itself
	appendLog(tr("[RFFE] The probe restarts and stops responding while this runs. Do not unplug it."), 2);
	rffe::RffeFlashDialog dlg(m_pal, m_flasher, port, this);
	dlg.exec();
}

void RffePanel::exportTestPoints()
{
	if (m_store.markedNets().isEmpty()) {
		appendLog(tr("[RFFE] no test points to export."), 2);
		return;
	}
	const QString path = QFileDialog::getSaveFileName(this, tr("Export RFFE test points"),
	                                                  QStringLiteral("rffe_testpoints.json"),
	                                                  QStringLiteral("JSON (*.json)"));
	if (path.isEmpty()) return;
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		appendLog(tr("[RFFE] Could not write %1.").arg(path), 3);
		return;
	}
	f.write(m_store.toJson());
	f.close();
	appendLog(tr("[RFFE] Exported %1 test point(s) to %2.").arg(m_store.visibleTestPointCount()).arg(path), 1);
}

// ------------------------------------------------------------------ shields

Shield &RffePanel::currentShieldOrDefault()
{
	if (Shield *s = m_store.find(m_currentShield)) return *s;
	Shield s;
	s.name = m_currentShield.isEmpty() ? tr("RFFE shield") : m_currentShield;
	s.added = true;
	m_store.addShield(s);
	m_currentShield = s.name;
	return *m_store.find(m_currentShield);
}

void RffePanel::rebuildShields()
{
	m_shieldList->setShields(m_store.shields(), m_currentShield);
}

void RffePanel::rebuildTestPoints()
{
	const Shield *s = m_store.find(m_currentShield);
	if (!s) {
		m_tpList->setItems({});
		m_expectedList->setItems({});
		return;
	}
	QStringList tps;
	for (const TestPoint &tp : s->testPoints) {
		tps << QStringLiteral("%1 — %2  [%3]")
		           .arg(tp.refDes.isEmpty() ? QStringLiteral("(net)") : tp.refDes, tp.net,
		                roleLabel(tp.role));
	}
	for (const DeviceMark &d : s->deviceMarks) tps << QStringLiteral("dev %1 → %2").arg(d.deviceId, d.net);
	m_tpList->setItems(tps);
	m_expectedList->setItems(s->expectedDevices);

	const int mapped = m_store.markedNets().size();
	m_countChip->setCount(mapped, std::max(mapped, m_store.visibleTestPointCount()),
	                      tr("pads mapped on this board"));

	m_hideShield->setText(s->hidden ? tr("Show shield") : tr("Hide shield"));
}

void RffePanel::rebuildDevices() {}

void RffePanel::onShieldSelected()
{
	m_currentShield = m_shieldList->currentShield();
	rebuildTestPoints();
}

void RffePanel::onAddShield()
{
	Shield s;
	s.name = tr("RFFE shield %1").arg(m_store.shields().size() + 1);
	s.added = true;
	m_store.addShield(s);
	m_currentShield = s.name;
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
}

void RffePanel::onRenameShield()
{
	Shield *s = m_store.find(m_currentShield);
	if (!s) return;
	QString name;
	if (!rffe::RffePromptDialog::getText(m_pal, this, tr("Rename shield"),
	                                     tr("New shield name for %1:").arg(s->name), s->name, &name))
		return;
	const QString old = s->name;
	s->name = name;
	m_currentShield = name;
	// keep the lookup consistent if the name changed
	if (old != name && !m_store.find(name)) {
		// find() by old name no longer matches; re-find by the updated pointer is safe
	}
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
}

void RffePanel::onRemoveShield()
{
	if (m_currentShield.isEmpty()) return;
	m_store.removeShield(m_currentShield);
	m_currentShield = m_store.shields().isEmpty() ? QString() : m_store.shields().first().name;
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
}

void RffePanel::onToggleShieldHidden()
{
	Shield *s = m_store.find(m_currentShield);
	if (!s) return;
	s->hidden = !s->hidden;
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
}

// ------------------------------------------------------------------ boardview integration

void RffePanel::addTestPoint(const QString &refDes, const QString &net, int role)
{
	if (net.isEmpty()) return;
	Shield &s = currentShieldOrDefault();
	const Role r = static_cast<Role>(role);
	bool found = false;
	for (TestPoint &tp : s.testPoints) {
		if (tp.refDes == refDes && tp.net == net) {
			if (r != Role::None) tp.role = r;
			found = true;
			break;
		}
	}
	if (!found) {
		TestPoint tp;
		tp.refDes = refDes;
		tp.net = net;
		tp.role = r;
		s.testPoints << tp;
	}
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
}

void RffePanel::setTestPointRole(const QString &net, int role)
{
	if (net.isEmpty()) return;
	const Role r = static_cast<Role>(role);
	if (r == Role::None) return;
	Shield &s = currentShieldOrDefault();
	for (TestPoint &tp : s.testPoints) {
		if (tp.net == net) {
			tp.role = r;
			saveStore();
			rebuildTestPoints();
			emitMarks();
			return;
		}
	}
	TestPoint tp;
	tp.net = net;
	tp.role = r;
	s.testPoints << tp;
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
}

void RffePanel::setCurrentNet(const QString &net)
{
	m_currentNet = net;
	m_getTp->setEnabled(!net.isEmpty());
	m_mapDevice->setText(net.isEmpty() ? tr("Map to '%1'").arg(tr("selected net"))
	                                   : tr("Map to '%1'").arg(net));
}

QStringList RffePanel::markedNets() const { return m_store.markedNets(); }

void RffePanel::onGetTestPoint()
{
	if (m_currentNet.isEmpty()) {
		appendLog(tr("[RFFE] Open the boardview first, then get the test point"), 2);
		return;
	}
	addTestPoint(QString(), m_currentNet, static_cast<int>(Role::None));
}

void RffePanel::onMapDeviceToNet()
{
	QListWidgetItem *item = m_deviceList->currentItem();
	if (!item) {
		appendLog(tr("[RFFE] Select a detected device first."), 2);
		return;
	}
	if (m_currentNet.isEmpty()) {
		appendLog(tr("[RFFE] Open the boardview and select a net first."), 2);
		return;
	}
	const QString id = item->text();
	Shield &s = currentShieldOrDefault();
	if (!s.expectedDevices.contains(id)) s.expectedDevices << id;
	bool found = false;
	for (DeviceMark &d : s.deviceMarks) {
		if (d.deviceId == id) {
			d.net = m_currentNet;
			found = true;
			break;
		}
	}
	if (!found) {
		DeviceMark d;
		d.deviceId = id;
		d.net = m_currentNet;
		s.deviceMarks << d;
	}
	saveStore();
	rebuildShields();
	rebuildTestPoints();
	emitMarks();
	appendLog(tr("[RFFE] Mapped device %1 to '%2'.").arg(id, m_currentNet), 1);
}

void RffePanel::onBlink() { emit blinkRequested(); }

// ------------------------------------------------------------------ settings

void RffePanel::openSettingsDialog()
{
	rffe::RffeSettingsDialog dlg(m_pal, m_marks, this);
	if (dlg.exec() != QDialog::Accepted) return;
	m_marks = dlg.settings();
	m_marks.save();
	emit markSettingsChanged();
}

// ------------------------------------------------------------------ helpers

void RffePanel::emitMarks() { emit markedNetsChanged(m_store.markedNets()); }

void RffePanel::saveStore()
{
	if (!m_store.save()) appendLog(tr("[RFFE] Could not save test points."), 3);
}

void RffePanel::appendLog(const QString &text, int level)
{
	if (text.isEmpty()) return;
	switch (level) {
	case 1: m_log->appendOk(text); break;
	case 2: m_log->appendWarn(text); break;
	case 3: m_log->appendError(text); break;
	default: m_log->appendLine(text); break;
	}
}

} // namespace w2r
