#include "ui/RffePanel.h"

#include "core/rffe/RffeFlasher.h"
#include "core/rffe/RffeProbe.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

namespace w2r {

using rffe::DeviceMark;
using rffe::Role;
using rffe::Shield;
using rffe::TestPoint;

namespace {
QString roleLabel(rffe::Role r)
{
	switch (r) {
	case rffe::Role::Clock: return QStringLiteral("CLK");
	case rffe::Role::Data: return QStringLiteral("DATA");
	case rffe::Role::V1V8: return QStringLiteral("1V8");
	case rffe::Role::None: break;
	}
	return QStringLiteral("—");
}
} // namespace

RffePanel::RffePanel(QWidget *parent) : QWidget(parent)
{
	m_marks = rffe::MarkSettings::load();
	m_store.load(); // best-effort; a missing file is fine

	// A single scrollable column: the dock is narrow and can be resized freely.
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	outer->addWidget(scroll);

	auto *content = new QWidget(scroll);
	scroll->setWidget(content);
	auto *root = new QVBoxLayout(content);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(8);

	// ============================ probe ============================
	auto *group = new QGroupBox(tr("ESP32 RFFE Probe (USB Serial)"), content);
	auto *g = new QVBoxLayout(group);

	auto *portRow = new QHBoxLayout();
	portRow->addWidget(new QLabel(tr("Port:"), this));
	m_ports = new QComboBox(this);
	m_ports->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	portRow->addWidget(m_ports, 1);
	m_refresh = new QPushButton(tr("Refresh"), this);
	connect(m_refresh, &QPushButton::clicked, this, &RffePanel::refreshPorts);
	portRow->addWidget(m_refresh);
	g->addLayout(portRow);

	auto *actions = new QHBoxLayout();
	m_connect = new QPushButton(tr("Connect"), this);
	connect(m_connect, &QPushButton::clicked, this, &RffePanel::toggleConnection);
	actions->addWidget(m_connect);

	m_scan = new QPushButton(tr("Scan RFFE Modules"), this);
	m_scan->setEnabled(false);
	connect(m_scan, &QPushButton::clicked, this, &RffePanel::scan);
	actions->addWidget(m_scan);

	m_flash = new QPushButton(tr("Update Probe Firmware..."), this);
	connect(m_flash, &QPushButton::clicked, this, &RffePanel::updateFirmware);
	actions->addWidget(m_flash);
	actions->addStretch(1);
	g->addLayout(actions);

	m_progress = new QProgressBar(this);
	m_progress->setRange(0, 100);
	m_progress->setVisible(false);
	g->addWidget(m_progress);

	m_log = new QPlainTextEdit(this);
	m_log->setReadOnly(true);
	m_log->setFont(QFont(QStringLiteral("Monospace"), 9));
	m_log->setStyleSheet(QStringLiteral("background:#121217;color:#10b981;"));
	g->addWidget(m_log, 1);

	root->addWidget(group, 1);
	m_status = new QLabel(this);
	m_status->setStyleSheet(QStringLiteral("color:#9ca3af;font-size:10px;"));
	root->addWidget(m_status);

	// ============================ shields ============================
	// --- Detected devices ---
	auto *devGroup = new QGroupBox(tr("Detected Devices"), this);
	auto *dv = new QVBoxLayout(devGroup);
	m_deviceList = new QListWidget(this);
	m_deviceList->setMaximumHeight(110);
	dv->addWidget(m_deviceList);
	auto *devRow = new QHBoxLayout();
	m_mapDevice = new QPushButton(tr("Map to '%1'").arg(tr("selected net")), this);
	connect(m_mapDevice, &QPushButton::clicked, this, &RffePanel::onMapDeviceToNet);
	devRow->addWidget(m_mapDevice);
	devRow->addStretch(1);
	dv->addLayout(devRow);
	root->addWidget(devGroup);

	// --- Shields + test points ---
	auto *shGroup = new QGroupBox(tr("RFFE Shields & Test Points"), this);
	auto *sv = new QVBoxLayout(shGroup);
	auto *split = new QSplitter(Qt::Vertical, this);
	m_shieldList = new QListWidget(this);
	split->addWidget(m_shieldList);
	m_tpList = new QListWidget(this);
	split->addWidget(m_tpList);
	split->setStretchFactor(0, 1);
	split->setStretchFactor(1, 1);
	sv->addWidget(split, 1);

	m_tpSummary = new QLabel(this);
	m_tpSummary->setStyleSheet(QStringLiteral("color:#9ca3af;font-size:10px;"));
	sv->addWidget(m_tpSummary);

	auto *shRow = new QHBoxLayout();
	m_addShield = new QPushButton(tr("Add shield"), this);
	m_removeShield = new QPushButton(tr("Remove shield"), this);
	m_hideShield = new QPushButton(tr("Hide shield"), this);
	m_blink = new QPushButton(tr("Blink RFFE nets"), this);
	connect(m_addShield, &QPushButton::clicked, this, &RffePanel::onAddShield);
	connect(m_removeShield, &QPushButton::clicked, this, &RffePanel::onRemoveShield);
	connect(m_hideShield, &QPushButton::clicked, this, &RffePanel::onToggleShieldHidden);
	connect(m_blink, &QPushButton::clicked, this, &RffePanel::onBlink);
	connect(m_shieldList, &QListWidget::itemSelectionChanged, this, &RffePanel::onShieldSelected);
	shRow->addWidget(m_addShield);
	shRow->addWidget(m_removeShield);
	shRow->addWidget(m_hideShield);
	shRow->addWidget(m_blink);
	shRow->addStretch(1);
	sv->addLayout(shRow);
	root->addWidget(shGroup, 1);

	// --- Marking settings ---
	auto *setGroup = new QGroupBox(tr("RFFE settings"), this);
	auto *form = new QFormLayout(setGroup);
	m_blinkMs = new QSpinBox(this);
	m_blinkMs->setRange(0, 1400);
	m_blinkMs->setSingleStep(50);
	m_blinkMs->setSuffix(QStringLiteral(" ms"));
	connect(m_blinkMs, &QSpinBox::valueChanged, this, &RffePanel::onMarkSettingsEdited);
	form->addRow(tr("Net & component highlight blink"), m_blinkMs);

	m_holdMs = new QSpinBox(this);
	m_holdMs->setRange(0, 120000);
	m_holdMs->setSingleStep(500);
	m_holdMs->setSuffix(QStringLiteral(" ms"));
	connect(m_holdMs, &QSpinBox::valueChanged, this, &RffePanel::onMarkSettingsEdited);
	form->addRow(tr("Hold mark for"), m_holdMs);

	m_halo = new QCheckBox(tr("Halo ring around marked pads"), this);
	connect(m_halo, &QCheckBox::toggled, this, &RffePanel::onMarkSettingsEdited);
	form->addRow(QString(), m_halo);

	m_esp32 = new QCheckBox(tr("Probe is an ESP32"), this);
	connect(m_esp32, &QCheckBox::toggled, this, &RffePanel::onMarkSettingsEdited);
	form->addRow(QString(), m_esp32);

	m_beep = new QCheckBox(tr("Beep when a scan or update starts and ends"), this);
	connect(m_beep, &QCheckBox::toggled, this, &RffePanel::onMarkSettingsEdited);
	form->addRow(QString(), m_beep);

	root->addWidget(setGroup);
	root->addStretch(1);

	// --- Wire the probe / flasher ---
	m_probe = new RffeProbe(this);
	connect(m_probe, &RffeProbe::connected, this, [this](const QString &p) {
		appendLog(tr("[RFFE] Connected to %1 at %2 baud.").arg(p).arg(m_probe->baudRate()));
		m_connect->setText(tr("Disconnect"));
		m_scan->setEnabled(true);
		m_status->setText(tr("Probe: %1").arg(p));
	});
	connect(m_probe, &RffeProbe::disconnected, this, [this] {
		appendLog(tr("[RFFE] Disconnected from probe."));
		m_connect->setText(tr("Connect"));
		m_scan->setEnabled(false);
		m_status->setText(tr("Not connected."));
	});
	connect(m_probe, &RffeProbe::lineReceived, this, [this](const QString &l) { appendLog(l); });
	connect(m_probe, &RffeProbe::dataReceived, this, [this](const QByteArray &d) {
		if (!d.contains('\n') && !d.contains('\r')) appendLog(QString::fromLatin1(d));
	});
	connect(m_probe, &RffeProbe::deviceDetected, this, [this](const QString &id) {
		m_status->setText(tr("Detected RFFE device %1").arg(id));
		if (m_deviceList->findItems(id, Qt::MatchExactly).isEmpty()) m_deviceList->addItem(id);
		emit deviceDetected(id);
	});
	connect(m_probe, &RffeProbe::errorOccurred, this, [this](const QString &e) {
		appendLog(tr("[RFFE ERROR] %1").arg(e));
	});

	m_flasher = new RffeFlasher(this);
	connect(m_flasher, &RffeFlasher::outputLine, this, [this](const QString &l) { appendLog(l); });
	connect(m_flasher, &RffeFlasher::progress, this, [this](int p) { m_progress->setValue(p); });
	connect(m_flasher, &RffeFlasher::finished, this, [this](bool ok, const QString &msg) {
		m_progress->setVisible(false);
		m_flash->setEnabled(true);
		m_connect->setEnabled(true);
		appendLog(ok ? tr("[RFFE] %1").arg(msg) : tr("[RFFE ERROR] %1").arg(msg));
		m_status->setText(msg);
	});

	refreshPorts();
	if (!RffeProbe::serialSupported()) {
		appendLog(tr("[RFFE] This build has no Qt SerialPort support."));
		m_connect->setEnabled(false);
		m_flash->setEnabled(false);
	}

	// Populate widgets from the loaded settings/store.
	m_blinkMs->setValue(m_marks.markBlinkMs);
	m_holdMs->setValue(m_marks.markHoldMs);
	m_halo->setChecked(m_marks.markHalo);
	m_esp32->setChecked(m_marks.esp32);
	m_beep->setChecked(m_marks.beep);
	rebuildShields();
	if (!m_store.shields().isEmpty()) m_currentShield = m_store.shields().first().name;
	rebuildTestPoints();
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
		m_ports->addItem(tr("No serial ports found"), QString());
		m_status->setText(tr("No serial ports found."));
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
		appendLog(tr("[RFFE] Select a COM port."));
		return;
	}
	QString error;
	if (!m_probe->open(port, 115200, &error))
		appendLog(tr("[RFFE ERROR] Can't open %1: %2").arg(port, error));
}

void RffePanel::scan()
{
	if (!m_probe->isOpen()) return;
	appendLog(tr("[RFFE] Scanning RFFE bus..."));
	QString error;
	if (!m_probe->sendScan(&error))
		appendLog(tr("[RFFE ERROR] Couldn't send the scan command: %1").arg(error));
}

void RffePanel::updateFirmware()
{
	const QString port = m_probe->isOpen() ? m_probe->portName() : m_ports->currentData().toString();
	if (port.isEmpty()) {
		appendLog(tr("[RFFE] Pick the probe's COM port first."));
		return;
	}
	if (m_probe->isOpen()) m_probe->close(); // the updater needs the port to itself
	m_progress->setValue(0);
	m_progress->setVisible(true);
	m_flash->setEnabled(false);
	m_connect->setEnabled(false);
	appendLog(tr("[RFFE] The probe restarts and stops responding while this runs. Do not unplug it."));
	m_flasher->start(port);
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
	m_shieldList->clear();
	for (const Shield &s : m_store.shields()) {
		QString label = s.name;
		if (s.hidden) label += QStringLiteral("  (hidden)");
		auto *item = new QListWidgetItem(label, m_shieldList);
		item->setData(Qt::UserRole, s.name);
	}
	for (int i = 0; i < m_shieldList->count(); ++i) {
		if (m_shieldList->item(i)->data(Qt::UserRole).toString() == m_currentShield) {
			m_shieldList->setCurrentRow(i);
			break;
		}
	}
}

void RffePanel::rebuildTestPoints()
{
	m_tpList->clear();
	Shield *s = m_store.find(m_currentShield);
	if (!s) {
		m_tpSummary->setText(tr("No shields for this model."));
		return;
	}
	for (const TestPoint &tp : s->testPoints) {
		const QString label = QStringLiteral("%1 — %2  [%3]")
		                          .arg(tp.refDes.isEmpty() ? QStringLiteral("(net)") : tp.refDes,
		                               tp.net, roleLabel(tp.role));
		m_tpList->addItem(label);
	}
	for (const DeviceMark &d : s->deviceMarks) {
		m_tpList->addItem(QStringLiteral("dev %1 → %2").arg(d.deviceId, d.net));
	}
	m_tpSummary->setText(tr("%1 test point(s), %2 expected device(s) in %3")
	                         .arg(s->testPoints.size())
	                         .arg(s->expectedDevices.size())
	                         .arg(s->name));
	m_hideShield->setText(s->hidden ? tr("Show shield") : tr("Hide shield"));
}

void RffePanel::rebuildDevices()
{
	// Detected ids are kept in the list widget itself; nothing to rebuild from the store.
}

void RffePanel::onShieldSelected()
{
	auto *item = m_shieldList->currentItem();
	if (!item) return;
	m_currentShield = item->data(Qt::UserRole).toString();
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
	const rffe::Role r = static_cast<rffe::Role>(role);
	bool found = false;
	for (TestPoint &tp : s.testPoints) {
		if (tp.refDes == refDes && tp.net == net) {
			if (r != rffe::Role::None) tp.role = r;
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
	const rffe::Role r = static_cast<rffe::Role>(role);
	if (r == rffe::Role::None) return;
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
	m_mapDevice->setText(net.isEmpty() ? tr("Map to '%1'").arg(tr("selected net"))
	                                   : tr("Map to '%1'").arg(net));
}

QStringList RffePanel::markedNets() const { return m_store.markedNets(); }

void RffePanel::onMapDeviceToNet()
{
	auto *item = m_deviceList->currentItem();
	if (!item) {
		appendLog(tr("[RFFE] Select a detected device first."));
		return;
	}
	if (m_currentNet.isEmpty()) {
		appendLog(tr("[RFFE] Open the boardview and select a net first."));
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
	appendLog(tr("[RFFE] Mapped device %1 to '%2'.").arg(id, m_currentNet));
}

void RffePanel::onBlink() { emit blinkRequested(); }

void RffePanel::onMarkSettingsEdited()
{
	m_marks.markBlinkMs = m_blinkMs->value();
	m_marks.markHoldMs = m_holdMs->value();
	m_marks.markHalo = m_halo->isChecked();
	m_marks.esp32 = m_esp32->isChecked();
	m_marks.beep = m_beep->isChecked();
	m_marks.save();
	emit markSettingsChanged();
}

// ------------------------------------------------------------------ helpers

void RffePanel::emitMarks() { emit markedNetsChanged(m_store.markedNets()); }

void RffePanel::saveStore()
{
	QString err;
	if (!m_store.save()) appendLog(tr("[RFFE] Could not save test points."));
	Q_UNUSED(err);
}

void RffePanel::appendLog(const QString &text)
{
	if (text.isEmpty()) return;
	m_log->appendPlainText(text);
}

} // namespace w2r
