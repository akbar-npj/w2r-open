#include "core/DiodeSheet.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace w2r {

namespace {

using PinKey = QPair<QString, QString>;

PinKey keyFor(const QString &part, const QString &pin)
{
	return qMakePair(part.trimmed().toUpper(), pin.trimmed().toUpper());
}

QHash<PinKey, int> buildIndex(const Board &board)
{
	QHash<PinKey, int> index;
	index.reserve(board.pins.size());
	for (int i = 0; i < board.pins.size(); ++i)
		index.insert(keyFor(board.pins[i].part, board.pins[i].name), i);
	return index;
}

int applyJson(Board &board, const QHash<PinKey, int> &index, const QByteArray &data)
{
	const QJsonDocument doc = QJsonDocument::fromJson(data);
	int applied = 0;

	auto put = [&](const QString &comp, const QString &pin, const QString &val) {
		const int idx = index.value(keyFor(comp, pin), -1);
		if (idx >= 0 && !val.isEmpty()) {
			board.pins[idx].diode = val;
			++applied;
		}
	};

	if (doc.isObject()) {
		const QJsonObject root = doc.object();
		for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
			if (!it.value().isObject()) continue;
			const QJsonObject pins = it.value().toObject();
			for (auto pit = pins.constBegin(); pit != pins.constEnd(); ++pit)
				put(it.key(), pit.key(), pit.value().toString());
		}
	} else if (doc.isArray()) {
		for (const QJsonValue &v : doc.array()) {
			const QJsonObject o = v.toObject();
			put(o.value(QStringLiteral("component")).toString(),
			    o.value(QStringLiteral("pin")).toString(),
			    o.value(QStringLiteral("diode")).toString());
		}
	}
	return applied;
}

int applyCsv(Board &board, const QHash<PinKey, int> &index, const QByteArray &data)
{
	QTextStream in(data);
	int applied = 0;
	bool first = true;
	while (!in.atEnd()) {
		const QString line = in.readLine().trimmed();
		if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;

		const QStringList parts = line.split(QLatin1Char(','));
		if (parts.size() < 3) continue;
		const QString c0 = parts.at(0).trimmed();
		const QString c1 = parts.at(1).trimmed();
		const QString c2 = parts.at(2).trimmed();

		if (first) {
			first = false;
			// Skip a header row like "component,pin,diode".
			if (c0.compare(QLatin1String("component"), Qt::CaseInsensitive) == 0 ||
			    c0.compare(QLatin1String("refdes"), Qt::CaseInsensitive) == 0 ||
			    c0.compare(QLatin1String("part"), Qt::CaseInsensitive) == 0)
				continue;
		}
		const int idx = index.value(keyFor(c0, c1), -1);
		if (idx >= 0 && !c2.isEmpty()) {
			board.pins[idx].diode = c2;
			++applied;
		}
	}
	return applied;
}

} // namespace

int applyDiodeFile(Board &board, const QString &path, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		if (error) *error = QStringLiteral("Cannot open %1").arg(path);
		return -1;
	}
	const QByteArray data = f.readAll();
	const QHash<PinKey, int> index = buildIndex(board);

	const QString ext = QFileInfo(path).suffix().toLower();
	int applied = 0;
	if (ext == QLatin1String("csv"))
		applied = applyCsv(board, index, data);
	else
		applied = applyJson(board, index, data);

	if (applied == 0 && error)
		*error = QStringLiteral("No pins matched in %1").arg(QFileInfo(path).fileName());
	return applied;
}

} // namespace w2r
