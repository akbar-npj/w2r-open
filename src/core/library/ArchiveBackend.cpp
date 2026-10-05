#include "core/library/ArchiveBackend.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

namespace w2r {

namespace {

bool runTool(const QString &program, const QStringList &args, QByteArray *out, QString *err,
             int timeoutMs = 60000)
{
	QProcess p;
	p.start(program, args);
	if (!p.waitForStarted(5000)) {
		if (err) *err = QStringLiteral("could not start %1").arg(program);
		return false;
	}
	if (!p.waitForFinished(timeoutMs)) {
		p.kill();
		p.waitForFinished(2000);
		if (err) *err = QStringLiteral("%1 timed out").arg(program);
		return false;
	}
	if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
		if (err)
			*err = QStringLiteral("%1 exited %2: %3")
			           .arg(program)
			           .arg(p.exitCode())
			           .arg(QString::fromLocal8Bit(p.readAllStandardError()).trimmed());
		return false;
	}
	if (out) *out = p.readAllStandardOutput();
	return true;
}

QString findTool(const QString &name)
{
	return QStandardPaths::findExecutable(name);
}

} // namespace

bool ArchiveBackend::toolsAvailable(QString *detail)
{
	const bool lsar = !findTool(QStringLiteral("lsar")).isEmpty();
	const bool unar = !findTool(QStringLiteral("unar")).isEmpty();
	if (detail) {
		if (lsar && unar)
			*detail = QStringLiteral("lsar + unar found");
		else
			*detail = QStringLiteral("missing: %1%2 (install the 'unar' package)")
			              .arg(lsar ? QString() : QStringLiteral("lsar "))
			              .arg(unar ? QString() : QStringLiteral("unar"));
	}
	return lsar && unar;
}

bool ArchiveBackend::isArchive(const QString &path)
{
	const QString ext = QFileInfo(path).suffix().toLower();
	static const QStringList exts = {QStringLiteral("rar"), QStringLiteral("zip"),
	                                 QStringLiteral("7z"),  QStringLiteral("tar"),
	                                 QStringLiteral("gz"),  QStringLiteral("tgz"),
	                                 QStringLiteral("xz"),  QStringLiteral("bz2")};
	return exts.contains(ext);
}

QVector<ArchiveMember> ArchiveBackend::list(const QString &archivePath)
{
	QVector<ArchiveMember> members;
	const QString lsar = findTool(QStringLiteral("lsar"));
	if (lsar.isEmpty()) return members;

	QByteArray out;
	QString err;
	if (!runTool(lsar, {QStringLiteral("-j"), archivePath}, &out, &err)) return members;

	QJsonParseError perr{};
	const QJsonDocument doc = QJsonDocument::fromJson(out, &perr);
	if (perr.error != QJsonParseError::NoError || !doc.isObject()) return members;

	const QJsonArray contents = doc.object().value(QStringLiteral("lsarContents")).toArray();
	members.reserve(contents.size());
	for (const QJsonValue &v : contents) {
		const QJsonObject o = v.toObject();
		if (o.value(QStringLiteral("XADIsDirectory")).toBool()) continue;
		ArchiveMember m;
		m.path = o.value(QStringLiteral("XADFileName")).toString();
		m.size = static_cast<qint64>(o.value(QStringLiteral("XADFileSize")).toDouble());
		if (!m.path.isEmpty()) members.append(m);
	}
	return members;
}

bool ArchiveBackend::extract(const QString &archivePath, const QStringList &members,
                             const QString &destDir)
{
	const QString unar = findTool(QStringLiteral("unar"));
	if (unar.isEmpty()) return false;
	if (!QDir().mkpath(destDir)) return false;

	QStringList args;
	args << QStringLiteral("-q") << QStringLiteral("-f") << QStringLiteral("-D")
	     << QStringLiteral("-o") << destDir << archivePath;
	args << members; // empty => extract all

	QString err;
	return runTool(unar, args, nullptr, &err, 5 * 60 * 1000);
}

QString ArchiveBackend::cacheRoot()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation))
	    .filePath(QStringLiteral("w2r-open"));
}

QString ArchiveBackend::cacheDirFor(const QString &archivePath)
{
	const QString abs = QFileInfo(archivePath).absoluteFilePath();
	const QString hash =
	    QString::fromLatin1(QCryptographicHash::hash(abs.toUtf8(), QCryptographicHash::Sha1).toHex());
	return QDir(cacheRoot()).filePath(QStringLiteral("extract/") + hash);
}

} // namespace w2r
