#include "ui/rffe/RffeWidgets.h"

#include <QAbstractItemView>
#include <QFont>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QVariant>

namespace w2r {
namespace rffe {

// --------------------------------------------------------------------------- RffeCard

RffeCard::RffeCard(const Palette &pal, const QString &title, QWidget *parent) : QFrame(parent)
{
	m_body = new QVBoxLayout(this);
	m_body->setContentsMargins(10, 10, 10, 10);
	m_body->setSpacing(8);
	if (!title.isEmpty()) {
		auto *label = new QLabel(title, this);
		label->setObjectName(QStringLiteral("rffeSectionTitle"));
		m_body->addWidget(label);
	}
	Q_UNUSED(pal);
}

// --------------------------------------------------------------------------- RffeBrand

RffeBrand::RffeBrand(QWidget *parent) : QWidget(parent)
{
	auto *row = new QHBoxLayout(this);
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(4);

	auto *mark = new QLabel(QStringLiteral("W2R"), this);
	mark->setObjectName(QStringLiteral("brandMark"));
	auto *name = new QLabel(QStringLiteral("SCHEMATICS"), this);
	name->setObjectName(QStringLiteral("brandName"));
	row->addWidget(mark);
	row->addWidget(name);
	row->addStretch(1);
}

// --------------------------------------------------------------------------- RffeButton

RffeButton::RffeButton(const QString &text, Variant variant, QWidget *parent)
    : QPushButton(text, parent)
{
	setVariant(variant);
}

void RffeButton::setVariant(Variant variant)
{
	const char *v = nullptr;
	switch (variant) {
	case Primary: v = "primary"; break;
	case Danger: v = "danger"; break;
	case Normal: break;
	}
	setProperty("variant", v ? QString::fromLatin1(v) : QVariant());
	style()->unpolish(this);
	style()->polish(this);
}

// --------------------------------------------------------------------------- RffeCombo

RffeCombo::RffeCombo(const Palette &pal, QWidget *parent) : QComboBox(parent)
{
	if (view()) view()->setObjectName(QStringLiteral("rffeComboPopup"));
	setItemDelegate(new RffeComboDelegate(pal, this));
}

RffeComboDelegate::RffeComboDelegate(const Palette &pal, QObject *parent)
    : QStyledItemDelegate(parent), m_pal(pal)
{
}

void RffeComboDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
	painter->save();
	const bool selected = option.state & QStyle::State_Selected;
	painter->fillRect(option.rect, selected ? m_pal.accentInk : m_pal.surfaceRaised);
	painter->setPen(selected ? m_pal.text : m_pal.textDim);
	painter->drawText(option.rect.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
	                  index.data(Qt::DisplayRole).toString());
	painter->restore();
}

// --------------------------------------------------------------------------- RffeCountChip

RffeCountChip::RffeCountChip(QWidget *parent) : QLabel(parent) {}

void RffeCountChip::setCount(int current, int total, const QString &noun)
{
	setText(tr("%1 of %2 %3").arg(current).arg(total).arg(noun));
}

// --------------------------------------------------------------------------- RffeSegmented

RffeSegmented::RffeSegmented(QWidget *parent) : QWidget(parent)
{
	auto *row = new QHBoxLayout(this);
	row->setContentsMargins(2, 2, 2, 2);
	row->setSpacing(2);
	row->addStretch(1);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

int RffeSegmented::addSegment(const QString &label)
{
	auto *row = qobject_cast<QHBoxLayout *>(layout());
	auto *button = new QPushButton(label, this);
	button->setCheckable(true);
	button->setCursor(Qt::PointingHandCursor);
	const int index = m_buttons.size();
	connect(button, &QPushButton::clicked, this, [this, index] { setCurrentIndex(index); });
	// Insert before the trailing stretch.
	row->insertWidget(row->count() - 1, button);
	m_buttons.append(button);
	if (m_current < 0) setCurrentIndex(0);
	return index;
}

void RffeSegmented::setCurrentIndex(int index)
{
	if (index < 0 || index >= m_buttons.size()) return;
	for (int i = 0; i < m_buttons.size(); ++i) m_buttons[i]->setChecked(i == index);
	if (m_current == index) return;
	m_current = index;
	emit currentChanged(index);
}

// --------------------------------------------------------------------------- RffeStatusLed

RffeStatusLed::RffeStatusLed(const Palette &pal, QWidget *parent) : QWidget(parent), m_pal(pal)
{
	setFixedSize(16, 16);
}

void RffeStatusLed::setState(State state)
{
	if (m_state == state) return;
	m_state = state;
	update();
}

void RffeStatusLed::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);

	QColor core = m_pal.ledOff;
	QColor glow;
	switch (m_state) {
	case On: core = m_pal.ledOn; glow = m_pal.ledOnGlow; break;
	case Danger: core = m_pal.ledDanger; glow = m_pal.ledDangerGlow; break;
	case Scan: core = m_pal.ledScan; glow = m_pal.ledScanGlow; break;
	case Off: break;
	}

	const QPointF c(width() / 2.0, height() / 2.0);
	if (glow.alpha() > 0) {
		p.setPen(Qt::NoPen);
		p.setBrush(glow);
		p.drawEllipse(c, 7.0, 7.0);
	}
	p.setPen(Qt::NoPen);
	p.setBrush(core);
	p.drawEllipse(c, 3.5, 3.5);
}

// --------------------------------------------------------------------------- RffeTerminal

RffeTerminal::RffeTerminal(const Palette &pal, QWidget *parent)
    : QPlainTextEdit(parent), m_pal(pal)
{
	setReadOnly(true);
	setLineWrapMode(QPlainTextEdit::NoWrap);
	QFont f(QStringLiteral("Monospace"));
	f.setStyleHint(QFont::TypeWriter);
	f.setPointSize(9);
	setFont(f);
}

void RffeTerminal::appendLine(const QString &text) { appendColored(text, m_pal.textDim); }
void RffeTerminal::appendOk(const QString &text) { appendColored(text, m_pal.ledOn); }
void RffeTerminal::appendWarn(const QString &text) { appendColored(text, m_pal.accent); }
void RffeTerminal::appendError(const QString &text) { appendColored(text, m_pal.ledDanger); }

void RffeTerminal::appendColored(const QString &text, const QColor &color)
{
	if (text.isEmpty()) return;
	appendHtml(QStringLiteral("<span style=\"color:%1\">%2</span>")
	               .arg(color.name(QColor::HexRgb), text.toHtmlEscaped()));
}

} // namespace rffe
} // namespace w2r
