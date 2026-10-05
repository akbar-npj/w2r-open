#include "core/rffe/RffeFlasher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace w2r {

namespace {
constexpr const char *kDefaultFirmware = "rffe-open-1.0.0.bin";
constexpr const char *kManifestName = "manifest.json";
constexpr const char *kDefaultOffset = "0x10000"; // app partition, per firmware/manifest.json
constexpr int kFlashBaud = 460800;

// Directories that may contain the bundled firmware, in priority order.
QStringList firmwareDirs()
{
	const QString appDir = QCoreApplication::applicationDirPath();
	QStringList dirs = {
	    appDir + QStringLiteral("/firmware"),
	    appDir + QStringLiteral("/../share/w2r-open/firmware"),
	    appDir + QStringLiteral("/../firmware"),
	    appDir + QStringLiteral("/../../firmware"),
	    QStringLiteral("/usr/share/w2r-open/firmware"),
	};
#ifdef W2R_FIRMWARE_DIR
	dirs << QStringLiteral(W2R_FIRMWARE_DIR);
#endif
	return dirs;
}
} // namespace

RffeFlasher::RffeFlasher(QObject *parent) : QObject(parent) {}

RffeFlasher::~RffeFlasher()
{
	if (m_proc) {
		m_proc->disconnect(this);
		if (m_proc->state() != QProcess::NotRunning) {
			m_proc->kill();
			m_proc->waitForFinished(1000);
		}
	}
}

QString RffeFlasher::bundledFirmwarePath()
{
	const QByteArray env = qgetenv("W2R_RFFE_FIRMWARE");
	if (!env.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(env)))
		return QString::fromLocal8Bit(env);

	for (const QString &dir : firmwareDirs()) {
		// Prefer the file named in the manifest so version bumps need no code change.
		QFile mf(QDir(dir).filePath(QLatin1String(kManifestName)));
		if (mf.open(QIODevice::ReadOnly)) {
			const QJsonObject o = QJsonDocument::fromJson(mf.readAll()).object();
			const QString file = o.value(QStringLiteral("file")).toString();
			if (!file.isEmpty()) {
				const QString abs = QDir(dir).filePath(file);
				if (QFileInfo::exists(abs)) return QDir::cleanPath(abs);
			}
		}
		const QString fallback = QDir(dir).filePath(QLatin1String(kDefaultFirmware));
		if (QFileInfo::exists(fallback)) return QDir::cleanPath(fallback);
	}
	return QString();
}

QString RffeFlasher::flashOffset()
{
	for (const QString &dir : firmwareDirs()) {
		QFile mf(QDir(dir).filePath(QLatin1String(kManifestName)));
		if (!mf.open(QIODevice::ReadOnly)) continue;
		const QJsonObject o = QJsonDocument::fromJson(mf.readAll()).object();
		const QJsonValue v = o.value(QStringLiteral("offset"));
		if (v.isDouble()) return QStringLiteral("0x%1").arg(qulonglong(v.toDouble()), 0, 16);
		if (v.isString() && !v.toString().isEmpty()) return v.toString();
	}
	return QLatin1String(kDefaultOffset);
}

QString RffeFlasher::esptoolProgram()
{
	for (const char *name : {"esptool", "esptool.py", "esptool-ck"}) {
		const QString p = QStandardPaths::findExecutable(QString::fromLatin1(name));
		if (!p.isEmpty()) return p;
	}
	// Fall back to the Python module form.
	const QString py = QStandardPaths::findExecutable(QStringLiteral("python3"));
	if (!py.isEmpty()) return py; // caller adds "-m esptool"
	return QString();
}

void RffeFlasher::start(const QString &port, const QString &firmwarePath)
{
	if (m_busy) return;

	const QString firmware = firmwarePath.isEmpty() ? bundledFirmwarePath() : firmwarePath;
	if (firmware.isEmpty()) {
		emit finished(false, tr("Probe firmware image not found."));
		return;
	}
	if (port.isEmpty()) {
		emit finished(false, tr("Pick the probe's COM port first."));
		return;
	}

	const QString tool = esptoolProgram();
	if (tool.isEmpty()) {
		emit finished(false, tr("esptool was not found. Install it (pip install esptool) to update the probe."));
		return;
	}

	QStringList args;
	if (tool.endsWith(QStringLiteral("python3"))) args << QStringLiteral("-m") << QStringLiteral("esptool");
	args << QStringLiteral("--chip") << QStringLiteral("esp32")
	     << QStringLiteral("--port") << port
	     << QStringLiteral("--baud") << QString::number(kFlashBaud)
	     << QStringLiteral("--before") << QStringLiteral("default_reset")
	     << QStringLiteral("--after") << QStringLiteral("hard_reset")
	     << QStringLiteral("write_flash") << QStringLiteral("-z") << flashOffset()
	     << firmware;

	m_proc = new QProcess(this);
	m_proc->setProcessChannelMode(QProcess::MergedChannels);
	connect(m_proc, &QProcess::readyRead, this, &RffeFlasher::handleOutput);
	connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
		m_busy = false;
		handleOutput();
		const bool ok = status == QProcess::NormalExit && code == 0;
		emit finished(ok, ok ? tr("Firmware updated. The probe has restarted.")
		                     : tr("Writing failed part-way. The probe's firmware is incomplete; run the update again."));
	});
	connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
		m_busy = false;
		emit finished(false, tr("Couldn't reach the probe's flash memory. Nothing was written."));
	});

	emit outputLine(tr("Updating probe firmware (%1)...").arg(QFileInfo(firmware).fileName()));
	m_busy = true;
	m_proc->start(tool, args);
}

void RffeFlasher::cancel()
{
	if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
	m_proc->kill();
	m_busy = false;
	emit outputLine(tr("Update cancelled."));
}

void RffeFlasher::handleOutput()
{
	if (!m_proc) return;
	const QByteArray chunk = m_proc->readAll();
	const QString text = QString::fromLocal8Bit(chunk);

	// esptool prints carriage-return progress updates; split on both CR and LF.
	static const QRegularExpression pct(QStringLiteral("\\((\\d+)\\s*%\\)"));
	for (const QString &raw : text.split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts)) {
		const QString line = raw.trimmed();
		if (line.isEmpty()) continue;
		emit outputLine(line);
		const auto m = pct.match(line);
		if (m.hasMatch()) emit progress(m.captured(1).toInt());
	}
}

} // namespace w2r
