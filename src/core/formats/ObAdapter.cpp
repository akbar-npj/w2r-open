#include "core/formats/ObAdapter.h"

#include "BRDFileBase.h"

#include <QHash>

namespace w2r {

static QString fromCStr(const char *s)
{
	if (!s) return QString();
	// OpenBoardView hands back latin1/utf8-ish byte strings; interpret leniently.
	return QString::fromUtf8(s);
}

static int layerFromPartSide(BRDPartMountingSide side)
{
	return side == BRDPartMountingSide::Bottom ? 1 : 0;
}

static int layerFromPinSide(BRDPinSide side, int fallback)
{
	switch (side) {
	case BRDPinSide::Top: return 0;
	case BRDPinSide::Bottom: return 1;
	default: return fallback; // Both / unspecified -> inherit part side
	}
}

Board adaptObBoard(BRDFileBase &ob, const QString &name, const QString &formatTag, bool includeNails)
{
	Board board;
	board.name = name;
	board.sourceFormat = formatTag;

	// Outline (already assembled into a polygon by the parsers).
	board.outline.reserve(ob.format.size());
	for (const BRDPoint &pt : ob.format)
		board.outline.append(QPointF(pt.x, pt.y));

	// Part metadata (mount type), keyed by refdes, applied after finalize() builds parts.
	QHash<QString, QString> mountByName;
	QHash<QString, int> layerByName;
	for (const BRDPart &part : ob.parts) {
		const QString pname = fromCStr(part.name);
		if (pname.isEmpty()) continue;
		mountByName.insert(pname, part.part_type == BRDPartType::SMD ? QStringLiteral("SMD")
		                                                             : QStringLiteral("TH"));
		layerByName.insert(pname, layerFromPartSide(part.mounting_side));
	}

	// Pins.
	board.pins.reserve(ob.pins.size());
	for (const BRDPin &pin : ob.pins) {
		Pin p;
		p.name = fromCStr(pin.snum && *pin.snum ? pin.snum : pin.name);
		p.net = fromCStr(pin.net);

		if (pin.part >= 1 && pin.part <= ob.parts.size()) {
			p.part = fromCStr(ob.parts[pin.part - 1].name);
		}
		const int partLayer = layerByName.value(p.part, 0);
		p.layer = layerFromPinSide(pin.side, partLayer);

		p.x = pin.pos.x;
		p.y = pin.pos.y;
		p.radius = pin.radius;
		board.pins.append(p);
	}

	// Optional: standalone test points (nails).
	if (includeNails) {
		for (const BRDNail &nail : ob.nails) {
			Pin p;
			p.name = QStringLiteral("TP%1").arg(nail.probe);
			p.part = QStringLiteral("NAIL");
			p.net = fromCStr(nail.net);
			p.x = nail.pos.x;
			p.y = nail.pos.y;
			p.layer = layerFromPartSide(nail.side);
			board.pins.append(p);
		}
	}

	board.finalize();

	// Apply part mount/side metadata gathered above.
	for (Part &part : board.parts) {
		if (mountByName.contains(part.name)) part.mount = mountByName.value(part.name);
		if (layerByName.contains(part.name)) part.layer = layerByName.value(part.name);
	}

	return board;
}

} // namespace w2r
