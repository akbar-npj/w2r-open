#include "Board.h"

#include <algorithm>

namespace w2r {

static bool netLooksGround(const QString &net)
{
	const QString u = net.trimmed().toUpper();
	if (u.isEmpty()) return false;
	if (u == "GND" || u == "GROUND" || u == "VSS" || u == "AGND" || u == "DGND" || u == "0V") return true;
	return u.startsWith("GND") || u.endsWith("_GND");
}

static bool netLooksNC(const QString &net)
{
	const QString u = net.trimmed().toUpper();
	return u.isEmpty() || u == "NC" || u == "N/C" || u == "NO_CONNECT" || u == "UNCONNECTED";
}

bool Pin::isGround() const { return netLooksGround(net); }
bool Pin::isNC() const { return netLooksNC(net); }

void Board::finalize()
{
	parts.clear();
	partIndex.clear();
	nets.clear();
	netIndex.clear();

	for (int i = 0; i < pins.size(); ++i) {
		const Pin &pin = pins[i];

		// --- Parts ---
		if (!pin.part.isEmpty()) {
			int pi = partIndex.value(pin.part, -1);
			if (pi < 0) {
				Part p;
				p.name = pin.part;
				p.layer = pin.layer;
				pi = parts.size();
				parts.push_back(p);
				partIndex.insert(pin.part, pi);
			}
			parts[pi].pins.push_back(i);
		}

		// --- Nets ---
		if (!pin.net.isEmpty()) {
			int ni = netIndex.value(pin.net, -1);
			if (ni < 0) {
				Net n;
				n.name = pin.net;
				n.ground = netLooksGround(pin.net);
				ni = nets.size();
				nets.push_back(n);
				netIndex.insert(pin.net, ni);
			}
			nets[ni].pins.push_back(i);
		}
	}

	// Per-part bounding boxes.
	for (Part &part : parts) {
		if (part.pins.isEmpty()) continue;
		double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
		for (int idx : part.pins) {
			const Pin &pin = pins[idx];
			minX = std::min(minX, pin.x);
			minY = std::min(minY, pin.y);
			maxX = std::max(maxX, pin.x);
			maxY = std::max(maxY, pin.y);
		}
		const double pad = 20.0; // mils of padding around the pad cluster
		part.bbox = QRectF(QPointF(minX - pad, minY - pad), QPointF(maxX + pad, maxY + pad));
	}

	// Overall bounds from outline + pins.
	double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
	auto acc = [&](double x, double y) {
		minX = std::min(minX, x);
		minY = std::min(minY, y);
		maxX = std::max(maxX, x);
		maxY = std::max(maxY, y);
	};
	for (const QPointF &p : outline) acc(p.x(), p.y());
	for (const Pin &pin : pins) acc(pin.x, pin.y);

	if (minX > maxX) { // no geometry at all
		bounds = QRectF(0, 0, 1000, 1000);
	} else {
		const double margin = 50.0;
		bounds = QRectF(QPointF(minX - margin, minY - margin), QPointF(maxX + margin, maxY + margin));
	}
}

} // namespace w2r
