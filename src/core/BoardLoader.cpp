#include "core/BoardLoader.h"

#include "core/PinNameRecovery.h"
#include "core/formats/ObAdapter.h"

#include "BRDFile.h"
#include "BVR3File.h"

#include <QFile>
#include <QFileInfo>

#include <vector>

namespace w2r {

static bool readBytes(const QString &path, std::vector<char> &out, QString &error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		error = QStringLiteral("Cannot open %1: %2").arg(path, f.errorString());
		return false;
	}
	const QByteArray data = f.readAll();
	out.assign(data.constData(), data.constData() + data.size());
	if (out.empty()) {
		error = QStringLiteral("File is empty: %1").arg(path);
		return false;
	}
	return true;
}

// Ordered content sniffing; returns a tag or empty string.
static QString detect(std::vector<char> &buf, const QString &ext)
{
	if (BRDFile::verifyFormat(buf)) return QStringLiteral("BRD");
	if (BVR3File::verifyFormat(buf)) return QStringLiteral("BVR3");

	const QString e = ext.toLower();
	if (e == QLatin1String("brd")) return QStringLiteral("BRD");
	if (e == QLatin1String("bvr")) return QStringLiteral("BVR3");
	return QString();
}

QString sniffBoardFormat(const QString &path)
{
	std::vector<char> buf;
	QString error;
	if (!readBytes(path, buf, error)) return QString();
	return detect(buf, QFileInfo(path).suffix());
}

LoadResult loadBoardFile(const QString &path)
{
	LoadResult result;

	std::vector<char> buf;
	QString error;
	if (!readBytes(path, buf, error)) {
		result.error = error;
		return result;
	}

	const QString tag = detect(buf, QFileInfo(path).suffix());
	const QString name = QFileInfo(path).completeBaseName();

	if (tag == QLatin1String("BRD")) {
		BRDFile ob(buf);
		if (!ob.valid) {
			result.error = QStringLiteral("Failed to parse BRD: %1").arg(QString::fromStdString(ob.error_msg));
			return result;
		}
		result.board = adaptObBoard(ob, name, tag);
		result.ok = true;
		// BRD carries no pin numbers; recover them from a sibling BVR when one exists.
		if (qEnvironmentVariableIsEmpty("W2R_NO_PIN_RECOVERY"))
			recoverPinNamesFromSibling(result.board, path);
		return result;
	}

	if (tag == QLatin1String("BVR3")) {
		BVR3File ob(buf);
		if (!ob.valid) {
			result.error = QStringLiteral("Failed to parse BVR3: %1").arg(QString::fromStdString(ob.error_msg));
			return result;
		}
		result.board = adaptObBoard(ob, name, tag);
		result.ok = true;
		return result;
	}

	result.error = QStringLiteral("Unsupported board format: %1").arg(QFileInfo(path).fileName());
	return result;
}

} // namespace w2r
