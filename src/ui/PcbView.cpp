#include "ui/PcbView.h"

#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace w2r {

PcbView::PcbView(QWidget *parent) : QWidget(parent)
{
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	setAutoFillBackground(false);
	setMinimumSize(320, 240);
}

void PcbView::setTheme(const Theme &theme)
{
	m_theme = theme;
	if (m_theme.layers.isEmpty()) {
		for (int i = 0; i < 16; ++i) m_theme.layers.append(QColor(0xa75b8fd6));
	}
	update();
}

void PcbView::setShowRatsnest(bool on)
{
	m_showRatsnest = on;
	update();
}

void PcbView::setBoard(const Board &board)
{
	m_board = board;
	m_hasBoard = true;
	m_selectedNet.clear();
	m_selectedPin = -1;
	m_hoverPin = -1;
	m_rotation = 0;
	m_showSide = 0;
	fitToBoard();
	update();
}

void PcbView::clearBoard()
{
	m_hasBoard = false;
	m_board = Board();
	update();
}

void PcbView::fitToBoard()
{
	if (!m_hasBoard || width() <= 0 || height() <= 0) return;
	const double bw = std::max(1.0, m_board.bounds.width());
	const double bh = std::max(1.0, m_board.bounds.height());
	const double sx = (width() * 0.88) / bw;
	const double sy = (height() * 0.88) / bh;
	m_scale = std::max(1e-4, std::min(sx, sy));
	m_pan = QPointF(0.0, 0.0);
	update();
}

void PcbView::flipSide()
{
	m_showSide = 1 - m_showSide;
	update();
}

void PcbView::rotateCw()
{
	m_rotation = (m_rotation + 90) % 360;
	update();
}

void PcbView::zoomBy(double factor)
{
	if (!m_hasBoard) return;
	const QPointF cursor(width() / 2.0, height() / 2.0);
	const QPointF before = screenToBoard(cursor.x(), cursor.y());
	m_scale = std::clamp(m_scale * factor, 1e-3, 5000.0);
	const QPointF after = boardToScreen(before.x(), before.y());
	m_pan += (cursor - after);
	update();
}

void PcbView::setShowDiodeReadings(bool on)
{
	m_showDiode = on;
	update();
}

void PcbView::selectNet(const QString &net)
{
	if (m_selectedNet == net) return;
	m_selectedNet = net;
	update();
}

QPointF PcbView::boardToScreen(double bx, double by) const
{
	const QPointF c = m_board.bounds.center();
	double dx = bx - c.x();
	double dy = by - c.y();

	if (m_showSide == 1) dx = -dx; // mirror horizontally for the bottom side

	if (m_rotation != 0) {
		const double rad = m_rotation * M_PI / 180.0;
		const double cs = std::cos(rad), sn = std::sin(rad);
		const double rx = dx * cs - dy * sn;
		const double ry = dx * sn + dy * cs;
		dx = rx;
		dy = ry;
	}

	return QPointF(width() / 2.0 + dx * m_scale + m_pan.x(),
	               height() / 2.0 + dy * m_scale + m_pan.y());
}

QPointF PcbView::screenToBoard(double sx, double sy) const
{
	double dx = (sx - width() / 2.0 - m_pan.x()) / m_scale;
	double dy = (sy - height() / 2.0 - m_pan.y()) / m_scale;

	if (m_rotation != 0) {
		const double rad = -m_rotation * M_PI / 180.0;
		const double cs = std::cos(rad), sn = std::sin(rad);
		const double rx = dx * cs - dy * sn;
		const double ry = dx * sn + dy * cs;
		dx = rx;
		dy = ry;
	}

	if (m_showSide == 1) dx = -dx;

	const QPointF c = m_board.bounds.center();
	return QPointF(c.x() + dx, c.y() + dy);
}

int PcbView::pinAt(const QPointF &screenPos) const
{
	if (!m_hasBoard) return -1;
	const double hit = 6.0;
	int best = -1;
	double bestD2 = hit * hit;
	for (int i = 0; i < m_board.pins.size(); ++i) {
		const Pin &pin = m_board.pins[i];
		if (pin.layer != m_showSide) continue;
		const QPointF sp = boardToScreen(pin.x, pin.y);
		const double r = std::max(2.0, (pin.radius * m_scale) / 2.0);
		const double dx = screenPos.x() - sp.x();
		const double dy = screenPos.y() - sp.y();
		const double d2 = dx * dx + dy * dy;
		const double rr = std::max(hit, r);
		if (d2 <= rr * rr && (best < 0 || d2 < bestD2)) {
			best = i;
			bestD2 = d2;
		}
	}
	return best;
}

void PcbView::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::TextAntialiasing, true);
	p.fillRect(rect(), m_theme.background);

	if (!m_hasBoard) {
		p.setPen(QColor(150, 150, 165));
		p.setFont(QFont(QStringLiteral("Sans"), 12));
		p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No board loaded.\nOpen a .brd or .bvr file."));
		return;
	}

	drawOutline(p);
	drawParts(p);
	drawPins(p);
	if (m_showRatsnest && !m_selectedNet.isEmpty()) drawRatsnest(p);

	// HUD
	p.setFont(QFont(QStringLiteral("Sans"), 9, QFont::Bold));
	p.setPen(QColor(190, 190, 205));
	const QString side = m_showSide == 0 ? QStringLiteral("TOP") : QStringLiteral("BOTTOM");
	p.drawText(12, 22, QStringLiteral("%1  |  %2  |  %3 pins  |  %4 parts  |  zoom %5")
	                        .arg(m_board.sourceFormat, side)
	                        .arg(m_board.pins.size())
	                        .arg(m_board.parts.size())
	                        .arg(m_scale, 0, 'f', 3));
}

void PcbView::drawOutline(QPainter &p)
{
	if (m_board.outline.size() >= 2) {
		QPainterPath path;
		bool first = true;
		for (const QPointF &pt : m_board.outline) {
			const QPointF sp = boardToScreen(pt.x(), pt.y());
			if (first) {
				path.moveTo(sp);
				first = false;
			} else {
				path.lineTo(sp);
			}
		}
		path.closeSubpath();
		p.setPen(QPen(m_theme.outline, 1.6));
		p.setBrush(m_theme.boardFill);
		p.drawPath(path);
	} else {
		const QPointF a = boardToScreen(m_board.bounds.left(), m_board.bounds.top());
		const QPointF b = boardToScreen(m_board.bounds.right(), m_board.bounds.bottom());
		p.setPen(QPen(m_theme.outline, 1.4));
		p.setBrush(m_theme.boardFill);
		p.drawRect(QRectF(a, b).normalized());
	}
}

void PcbView::drawParts(QPainter &p)
{
	p.setFont(QFont(QStringLiteral("Sans"), 8, QFont::Bold));
	for (const Part &part : m_board.parts) {
		if (part.layer != m_showSide || !part.bbox.isValid()) continue;
		const QPointF a = boardToScreen(part.bbox.left(), part.bbox.top());
		const QPointF b = boardToScreen(part.bbox.right(), part.bbox.bottom());
		const QRectF r = QRectF(a, b).normalized();
		if (!r.intersects(rect())) continue; // cull

		p.setPen(QPen(m_theme.partOutline, 1.0, Qt::DashLine));
		p.setBrush(Qt::NoBrush);
		p.drawRect(r);
		if (r.width() > 26 && r.height() > 14) {
			p.setPen(m_theme.componentNameText);
			p.drawText(r.adjusted(2, 2, -2, -2), Qt::AlignTop | Qt::AlignLeft, part.name);
		}
	}
}

void PcbView::drawPins(QPainter &p)
{
	const QRect viewport = rect().adjusted(-8, -8, 8, 8);
	p.setFont(QFont(QStringLiteral("Sans"), 7));

	for (int i = 0; i < m_board.pins.size(); ++i) {
		const Pin &pin = m_board.pins[i];
		if (pin.layer != m_showSide) continue;

		const QPointF sp = boardToScreen(pin.x, pin.y);
		const double radius = std::max(1.6, (pin.radius * m_scale) / 2.0);
		const QRectF pr(sp.x() - radius, sp.y() - radius, radius * 2, radius * 2);
		if (!viewport.contains(sp.toPoint())) continue; // cull

		const bool selected = !m_selectedNet.isEmpty() && pin.net == m_selectedNet;
		const bool clicked = (i == m_selectedPin);
		const bool hovered = (i == m_hoverPin);

		QColor fill;
		if (clicked) {
			fill = m_theme.selectedPin;
		} else if (selected) {
			fill = m_theme.sameNetPin;
		} else if (hovered) {
			fill = m_theme.partHighlightBorder;
		} else if (pin.isGround()) {
			fill = m_theme.groundPin;
		} else if (pin.isNC()) {
			fill = m_theme.ncPin;
		} else {
			fill = m_theme.pin;
		}

		p.setPen(QPen(m_theme.pinOutline, selected || clicked ? 1.6 : 0.8));
		p.setBrush(fill);
		p.drawEllipse(pr);

		if (radius >= 6.0) {
			p.setPen(m_theme.pinText);
			p.drawText(pr, Qt::AlignCenter, pin.name);
			if (m_showDiode && !pin.diode.isEmpty()) {
				p.setPen(QColor(255, 215, 0));
				p.drawText(QRectF(sp.x() - 30, sp.y() + radius + 1, 60, 12), Qt::AlignCenter, pin.diode);
			}
		}
	}
}

void PcbView::drawRatsnest(QPainter &p)
{
	if (m_selectedNet.isEmpty()) return;

	QVector<QPointF> pts;
	pts.reserve(64);
	for (const Pin &pin : m_board.pins) {
		if (pin.layer != m_showSide) continue;
		if (pin.net != m_selectedNet) continue;
		pts.append(boardToScreen(pin.x, pin.y));
		if (pts.size() >= 200) break; // avoid drawing thousands of flylines
	}
	if (pts.size() < 2) return;

	// Star from the first pin to every other pin on the net.
	p.setPen(QPen(m_theme.ratsnest, 1.2, Qt::DashLine));
	for (int i = 1; i < pts.size(); ++i) p.drawLine(pts[0], pts[i]);
}

void PcbView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
		m_panning = true;
		m_lastMouse = event->position();
		setCursor(Qt::ClosedHandCursor);
		return;
	}
	if (event->button() == Qt::LeftButton) {
		const int idx = pinAt(event->position());
		m_selectedPin = idx;
		if (idx >= 0) {
			m_selectedNet = m_board.pins[idx].net;
			emit pinSelected(idx);
			emit netSelected(m_selectedNet);
		} else {
			m_selectedNet.clear();
		}
		update();
	}
}

void PcbView::mouseMoveEvent(QMouseEvent *event)
{
	if (m_panning) {
		const QPointF d = event->position() - m_lastMouse;
		m_pan += d;
		m_lastMouse = event->position();
		update();
		return;
	}

	const int idx = pinAt(event->position());
	if (idx != m_hoverPin) {
		m_hoverPin = idx;
		if (idx >= 0) {
			const Pin &pin = m_board.pins[idx];
			QToolTip::showText(event->globalPosition().toPoint(),
			                   QStringLiteral("<b>%1</b> pin <b>%2</b><br>Net: <b>%3</b>")
			                       .arg(pin.part, pin.name, pin.net.isEmpty() ? QStringLiteral("(none)") : pin.net),
			                   this);
		} else {
			QToolTip::hideText();
		}
		update();
	}
}

void PcbView::mouseReleaseEvent(QMouseEvent *)
{
	if (m_panning) {
		m_panning = false;
		setCursor(Qt::ArrowCursor);
	}
}

void PcbView::wheelEvent(QWheelEvent *event)
{
	const QPointF cursor = event->position();
	const QPointF before = screenToBoard(cursor.x(), cursor.y());

	const double steps = event->angleDelta().y() / 120.0;
	const double factor = std::pow(1.2, steps);
	m_scale = std::clamp(m_scale * factor, 1e-3, 5000.0);

	const QPointF after = boardToScreen(before.x(), before.y());
	m_pan += (cursor - after);
	update();
}

void PcbView::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Space:
	case Qt::Key_F: flipSide(); break;
	case Qt::Key_R: rotateCw(); break;
	case Qt::Key_Home:
	case Qt::Key_0: fitToBoard(); break;
	case Qt::Key_Plus:
	case Qt::Key_Equal: zoomIn(); break;
	case Qt::Key_Minus:
	case Qt::Key_Underscore: zoomOut(); break;
	default: QWidget::keyPressEvent(event); return;
	}
	update();
}

} // namespace w2r
