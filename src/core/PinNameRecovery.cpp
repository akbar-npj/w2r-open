#include "core/PinNameRecovery.h"

#include "core/formats/ObAdapter.h"

#include "BVR3File.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QVector>

#include <algorithm>
#include <vector>

namespace w2r {

namespace {

using Coord = QPair<qint64, qint64>;

// A coordinate-space mapping between two boardview formats, expressed as
//   a = swap ? y : x ;  b = swap ? x : y ;  (tx, ty) = (sx * a, sy * b)
struct Transform {
	bool swap;
	int sx;
	int sy;
};

const Transform kCandidates[] = {
	{true, -1, 1},  // (-y,  x)  BRD -> BVR
	{true, 1, -1},  // ( y, -x)
	{false, 1, 1},  // ( x,  y)
	{false, -1, -1},// (-x, -y)
	{true, 1, 1},   // ( y,  x)
	{true, -1, -1}, // (-y, -x)
};

Coord applyTransform(const Transform &t, double x, double y)
{
	const double a = t.swap ? y : x;
	const double b = t.swap ? x : y;
	return Coord(qRound64(t.sx * a), qRound64(t.sy * b));
}

bool readBytes(const QString &path, std::vector<char> &out)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return false;
	const QByteArray data = f.readAll();
	if (data.isEmpty()) return false;
	out.assign(data.constData(), data.constData() + data.size());
	return true;
}

// Parses a BVR file into a Board; returns false on failure.
bool loadBvr(const QString &path, Board &out)
{
	std::vector<char> buf;
	if (!readBytes(path, buf)) return false;
	if (!BVR3File::verifyFormat(buf)) return false;
	BVR3File ob(buf);
	if (!ob.valid) return false;
	out = adaptObBoard(ob, QFileInfo(path).completeBaseName(), QStringLiteral("BVR3"));
	return true;
}

// Picks the sibling .bvr whose base name shares the longest prefix with `boardPath`.
QString findSiblingBvr(const QString &boardPath)
{
	const QFileInfo info(boardPath);
	const QDir dir = info.dir();
	const QString stem = info.completeBaseName().toLower();

	QString best;
	int bestScore = -1;
	const QStringList filters{QStringLiteral("*.bvr"), QStringLiteral("*.BVR")};
	for (const QFileInfo &fi : dir.entryInfoList(filters, QDir::Files)) {
		if (fi.absoluteFilePath() == info.absoluteFilePath()) continue;
		const QString other = fi.completeBaseName().toLower();

		int common = 0;
		const int n = std::min(stem.size(), other.size());
		while (common < n && stem.at(common) == other.at(common)) ++common;

		if (common > bestScore) {
			bestScore = common;
			best = fi.absoluteFilePath();
		}
	}
	return best;
}

} // namespace

int recoverPinNamesFromSibling(Board &board, const QString &boardPath)
{
	bool anyEmpty = false;
	for (const Pin &p : board.pins) {
		if (p.name.isEmpty()) {
			anyEmpty = true;
			break;
		}
	}
	if (!anyEmpty || board.pins.isEmpty()) return 0;

	const QString sibling = findSiblingBvr(boardPath);
	if (sibling.isEmpty()) return 0;

	Board ref;
	if (!loadBvr(sibling, ref) || ref.pins.isEmpty()) return 0;

	// Coordinate -> list of (part, pinName). Several parts can share a coordinate, so we keep
	// every candidate and disambiguate by refdes at lookup time.
	QHash<Coord, QVector<QPair<QString, QString>>> byCoord;
	byCoord.reserve(ref.pins.size());
	for (const Pin &p : ref.pins) {
		if (p.name.isEmpty()) continue;
		byCoord[Coord(qRound64(p.x), qRound64(p.y))].append(qMakePair(p.part, p.name));
	}
	if (byCoord.isEmpty()) return 0;

	// Choose the transform that resolves the most pins.
	auto lookup = [&](const Transform &t, const Pin &pin) -> QString {
		const auto it = byCoord.constFind(applyTransform(t, pin.x, pin.y));
		if (it == byCoord.constEnd()) return QString();
		const QVector<QPair<QString, QString>> &cands = it.value();
		for (const auto &c : cands)
			if (c.first == pin.part) return c.second;
		return cands.size() == 1 ? cands.first().second : QString();
	};

	const Transform *bestT = nullptr;
	int bestHits = 0;
	for (const Transform &t : kCandidates) {
		int hits = 0;
		for (const Pin &p : board.pins) {
			if (!p.name.isEmpty()) continue;
			if (!lookup(t, p).isEmpty()) ++hits;
		}
		if (hits > bestHits) {
			bestHits = hits;
			bestT = &t;
		}
	}

	// Require a strong match so a wrong/stale sibling cannot corrupt the board.
	if (!bestT || bestHits * 2 < board.pins.size()) return 0;

	int recovered = 0;
	for (Pin &p : board.pins) {
		if (!p.name.isEmpty()) continue;
		const QString name = lookup(*bestT, p);
		if (!name.isEmpty()) {
			p.name = name;
			++recovered;
		}
	}
	return recovered;
}

} // namespace w2r
