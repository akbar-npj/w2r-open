#include "core/rffe/RffeProbe.h"

#include <QRegularExpression>

#ifdef W2R_HAVE_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

namespace w2r {

namespace {
// Device identifiers are formatted by the firmware with "%05d"; surface runs of digits as
// candidate IDs. This is deliberately permissive because the wire format is not documented.
const QRegularExpression &idPattern()
{
	static const QRegularExpression re(QStringLiteral("\\b(\\d{5})\\b"));
	return re;
}
} // namespace

const char *RffeProbe::scanCommand() { return "way2_rffe_"; }

QString RffePort::label() const
{
	QString l = name;
	if (!description.isEmpty()) l += QStringLiteral(" (%1)").arg(description);
	return l;
}

RffeProbe::RffeProbe(QObject *parent) : QObject(parent)
{
#ifdef W2R_HAVE_SERIALPORT
	m_serial = new QSerialPort(this);
	connect(m_serial, &QSerialPort::readyRead, this, &RffeProbe::onReadyRead);
#endif
}

RffeProbe::~RffeProbe() { close(); }

bool RffeProbe::serialSupported()
{
#ifdef W2R_HAVE_SERIALPORT
	return true;
#else
	return false;
#endif
}

QVector<RffePort> RffeProbe::availablePorts()
{
	QVector<RffePort> out;
#ifdef W2R_HAVE_SERIALPORT
	for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
		RffePort p;
		p.name = info.portName();
		p.description = info.description();
		p.manufacturer = info.manufacturer();
		p.serial = info.serialNumber();
		p.isUsb = info.hasVendorIdentifier() && info.hasProductIdentifier();
		if (p.isUsb) {
			p.vid = info.vendorIdentifier();
			p.pid = info.productIdentifier();
		}
		out.append(p);
	}
#endif
	return out;
}

bool RffeProbe::open(const QString &portName, int baud, QString *error)
{
#ifdef W2R_HAVE_SERIALPORT
	if (m_open) close();
	m_serial->setPortName(portName);
	m_serial->setBaudRate(baud);
	m_serial->setDataBits(QSerialPort::Data8);
	m_serial->setParity(QSerialPort::NoParity);
	m_serial->setStopBits(QSerialPort::OneStop);
	m_serial->setFlowControl(QSerialPort::NoFlowControl);
	if (!m_serial->open(QIODevice::ReadWrite)) {
		if (error) *error = m_serial->errorString();
		return false;
	}
	m_open = true;
	m_portName = portName;
	m_baud = baud;
	m_buffer.clear();
	emit connected(portName);
	return true;
#else
	Q_UNUSED(portName);
	Q_UNUSED(baud);
	if (error) *error = QStringLiteral("Qt SerialPort support was not compiled in.");
	return false;
#endif
}

void RffeProbe::close()
{
#ifdef W2R_HAVE_SERIALPORT
	if (m_open) {
		m_serial->close();
		m_open = false;
		m_portName.clear();
		emit disconnected();
	}
#endif
}

bool RffeProbe::sendScan(QString *error)
{
	if (!m_open) {
		if (error) *error = QStringLiteral("No probe connected.");
		return false;
	}
	write(QByteArray(scanCommand()));
	return true;
}

void RffeProbe::write(const QByteArray &data)
{
#ifdef W2R_HAVE_SERIALPORT
	if (!m_open) return;
	m_serial->write(data);
	m_serial->flush();
#else
	Q_UNUSED(data);
#endif
}

void RffeProbe::onReadyRead()
{
#ifdef W2R_HAVE_SERIALPORT
	consume(m_serial->readAll());
#endif
}

void RffeProbe::consume(const QByteArray &data)
{
	if (data.isEmpty()) return;
	emit dataReceived(data);

	m_buffer.append(data);
	// Split on LF; keep any trailing partial line buffered.
	int nl;
	while ((nl = m_buffer.indexOf('\n')) >= 0) {
		QByteArray raw = m_buffer.left(nl);
		m_buffer.remove(0, nl + 1);
		if (raw.endsWith('\r')) raw.chop(1);
		const QString line = QString::fromLatin1(raw).trimmed();
		if (line.isEmpty()) continue;
		emit lineReceived(line);

		// Open firmware: "DEV <usid> <reg0>".
		if (line.startsWith(QLatin1String("DEV "))) {
			const QStringList f = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
			if (f.size() >= 2) emit deviceDetected(f.at(1));
			continue;
		}

		// Vendor firmware: surface runs of digits as candidate IDs.
		auto it = idPattern().globalMatch(line);
		while (it.hasNext()) emit deviceDetected(it.next().captured(1));
	}

	// Guard against an unterminated stream filling memory.
	if (m_buffer.size() > 65536) {
		const QString line = QString::fromLatin1(m_buffer).trimmed();
		m_buffer.clear();
		if (!line.isEmpty()) emit lineReceived(line);
	}
}

} // namespace w2r
