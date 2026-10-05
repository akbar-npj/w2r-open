#include "ui/RffePanel.h"

#include "core/rffe/RffeFlasher.h"
#include "core/rffe/RffeProbe.h"

#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace w2r {

RffePanel::RffePanel(QWidget *parent) : QWidget(parent)
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	auto *group = new QGroupBox(tr("ESP32 RFFE Probe (USB Serial)"), this);
	auto *g = new QVBoxLayout(group);

	// --- Port selection ---
	auto *portRow = new QHBoxLayout();
	portRow->addWidget(new QLabel(tr("Port:"), this));
	m_ports = new QComboBox(this);
	m_ports->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	portRow->addWidget(m_ports, 1);
	m_refresh = new QPushButton(tr("Refresh"), this);
	connect(m_refresh, &QPushButton::clicked, this, &RffePanel::refreshPorts);
	portRow->addWidget(m_refresh);
	g->addLayout(portRow);

	// --- Connection / actions ---
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

	// --- Terminal ---
	m_log = new QPlainTextEdit(this);
	m_log->setReadOnly(true);
	m_log->setFont(QFont(QStringLiteral("Monospace"), 9));
	m_log->setStyleSheet(QStringLiteral("background:#121217;color:#10b981;"));
	g->addWidget(m_log, 1);

	root->addWidget(group, 1);

	m_status = new QLabel(this);
	m_status->setStyleSheet(QStringLiteral("color:#9ca3af;font-size:10px;"));
	root->addWidget(m_status);

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
		// Non-UTF8 / partial data that never formed a line is still shown in raw form.
		if (!d.contains('\n') && !d.contains('\r')) appendLog(QString::fromLatin1(d));
	});
	connect(m_probe, &RffeProbe::deviceDetected, this, [this](const QString &id) {
		m_status->setText(tr("Detected RFFE device %1").arg(id));
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
}

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

void RffePanel::appendLog(const QString &text)
{
	if (text.isEmpty()) return;
	m_log->appendPlainText(text);
}

} // namespace w2r
