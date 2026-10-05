// Interactive PCB boardview canvas.
//
// Custom QWidget + QPainter with viewport culling and level-of-detail, chosen over
// QOpenGLWidget for robustness on Asahi/Mesa while still being fast in C++. The renderer is
// intentionally simple here; PcbRenderer is the seam where an OpenGL backend can be added.
#pragma once

#include "core/Board.h"
#include "core/Theme.h"

#include <QPointF>
#include <QString>
#include <QWidget>

namespace w2r {

class PcbView : public QWidget {
	Q_OBJECT
  public:
	explicit PcbView(QWidget *parent = nullptr);

	void setBoard(const Board &board);
	void clearBoard();
	const Board &board() const { return m_board; }
	Board &mutableBoard() { return m_board; }
	void refresh() { update(); }
	bool hasBoard() const { return m_hasBoard; }

	void setTheme(const Theme &theme);

	void fitToBoard();
	void flipSide();
	void rotateCw();
	void zoomBy(double factor);
	void zoomIn() { zoomBy(1.25); }
	void zoomOut() { zoomBy(1.0 / 1.25); }
	void setShowDiodeReadings(bool on);
	void setShowRatsnest(bool on);
	void selectNet(const QString &net);

	QSize sizeHint() const override { return QSize(900, 700); }

  signals:
	void pinSelected(int pinIndex);
	void netSelected(const QString &net);

  protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

  private:
	QPointF boardToScreen(double bx, double by) const;
	QPointF screenToBoard(double sx, double sy) const;
	int pinAt(const QPointF &screenPos) const;

	void drawOutline(QPainter &p);
	void drawParts(QPainter &p);
	void drawPins(QPainter &p);
	void drawRatsnest(QPainter &p);

	Board m_board;
	bool m_hasBoard = false;
	Theme m_theme;

	double m_scale = 1.0; // screen pixels per mil
	QPointF m_pan{0.0, 0.0};
	int m_rotation = 0;   // 0/90/180/270
	int m_showSide = 0;   // 0 = top, 1 = bottom

	bool m_showDiode = true;
	bool m_showRatsnest = true;
	QString m_selectedNet;
	int m_selectedPin = -1;
	int m_hoverPin = -1;

	bool m_panning = false;
	QPointF m_lastMouse;
};

} // namespace w2r
