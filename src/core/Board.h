// Core boardview data model for W2R Open.
//
// Coordinates are stored in **mils** (thousandths of an inch) because that is the native
// unit of both the BVRAW_FORMAT_3 (.bvr) and the obfuscated BRD (.brd) formats, and of the
// OpenBoardView parsers we adapt. The renderer converts mils -> screen as needed.
#pragma once

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace w2r {

// Which copper side a part/pin lives on.
enum class Side { Top = 0, Bottom = 1, Both = 2 };

struct Pin {
	QString name;   // pin number / name (e.g. "A1", "1")
	QString part;   // owning part refdes (e.g. "U0600")
	QString net;    // net name (e.g. "PP1V2_AWAKE"); may be empty
	double x = 0.0; // mils
	double y = 0.0; // mils
	double radius = 0.0; // pad radius, mils
	int layer = 0;  // 0 = top, 1 = bottom
	QString diode;  // optional diode-mode reading (imported), e.g. "0.435V", "OL"

	bool isGround() const;
	bool isNC() const;
};

struct Part {
	QString name;   // refdes
	int layer = 0;  // 0 = top, 1 = bottom
	QString mount;  // "SMD", "DIP", "TH"
	QVector<int> pins; // indices into Board::pins
	QRectF bbox;    // mils, computed in finalize()
};

struct Net {
	QString name;
	QVector<int> pins; // indices into Board::pins
	bool ground = false;
};

struct Trace {
	QString net;
	int layer = 0;
	QVector<QPointF> points; // mils
	double width = 0.0;      // mils
};

class Board {
  public:
	QString name;
	QString sourceFormat; // e.g. "BRD", "BVR3"
	QVector<QPointF> outline; // mils
	QVector<Part> parts;
	QVector<Pin> pins;
	QHash<QString, int> partIndex; // refdes -> parts index
	QHash<QString, int> netIndex;  // net name -> nets index
	QVector<Net> nets;
	QVector<Trace> traces;
	QRectF bounds; // mils

	// Builds parts/nets/bboxes/bounds from the flat pin list. Call once after loading.
	void finalize();

	// Approximate millimetre extent (for display / sanity checks).
	double widthMm() const { return bounds.width() * 0.0254; }
	double heightMm() const { return bounds.height() * 0.0254; }
};

} // namespace w2r
