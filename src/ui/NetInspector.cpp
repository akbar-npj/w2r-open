#include "ui/NetInspector.h"

#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace w2r {

namespace {
constexpr int kNetRole = Qt::UserRole + 1; // net index on a top-level item
constexpr int kPinRole = Qt::UserRole + 2; // pin index on a child item
} // namespace

NetInspector::NetInspector(QWidget *parent) : QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(6);

	m_filter = new QLineEdit(this);
	m_filter->setPlaceholderText(QStringLiteral("Filter nets..."));
	m_filter->setClearButtonEnabled(true);
	connect(m_filter, &QLineEdit::textChanged, this, &NetInspector::onFilterChanged);
	layout->addWidget(m_filter);

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderLabels({QStringLiteral("Net / Pin"), QStringLiteral("Pins")});
	m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_tree->setUniformRowHeights(true);
	m_tree->setExpandsOnDoubleClick(false);
	connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &NetInspector::onSelectionChanged);
	connect(m_tree, &QTreeWidget::itemExpanded, this, &NetInspector::onItemExpanded);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
		if (!item) return;
		const QVariant pv = item->data(0, kPinRole);
		if (pv.isValid()) {
			const int idx = pv.toInt();
			if (idx >= 0 && idx < m_board.pins.size())
				emit pinActivated(m_board.pins[idx].part, m_board.pins[idx].name);
		}
	});
	layout->addWidget(m_tree);

	auto *status = new QLabel(this);
	status->setObjectName(QStringLiteral("netStatus"));
	status->setStyleSheet(QStringLiteral("color:#9ca3af;font-size:10px;"));
	status->setText(QStringLiteral("No board loaded."));
	layout->addWidget(status);
}

void NetInspector::setStatus(const QString &text)
{
	if (auto *lbl = findChild<QLabel *>(QStringLiteral("netStatus"))) lbl->setText(text);
}

void NetInspector::clearBoard()
{
	m_board = Board();
	m_hasBoard = false;
	m_tree->clear();
	setStatus(QStringLiteral("No board loaded."));
}

void NetInspector::setBoard(const Board &board)
{
	m_board = board;
	m_hasBoard = true;
	repopulate();
}

void NetInspector::onFilterChanged(const QString &) { repopulate(); }

void NetInspector::repopulate()
{
	m_populating = true;
	m_tree->clear();

	if (!m_hasBoard) {
		setStatus(QStringLiteral("No board loaded."));
		m_populating = false;
		return;
	}

	const QString q = m_filter->text().trimmed().toLower();
	const QStringList tokens = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);

	// Sort nets by descending pin count (busiest first) for a useful default view.
	QVector<int> order;
	order.reserve(m_board.nets.size());
	for (int i = 0; i < m_board.nets.size(); ++i) order.append(i);
	std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
		return m_board.nets[a].pins.size() > m_board.nets[b].pins.size();
	});

	int shown = 0;
	for (int ni : order) {
		const Net &net = m_board.nets[ni];
		bool all = true;
		for (const QString &t : tokens) {
			if (!net.name.toLower().contains(t)) {
				all = false;
				break;
			}
		}
		if (!all) continue;

		auto *item = new QTreeWidgetItem(m_tree);
		item->setText(0, net.name);
		item->setText(1, QStringLiteral("%1").arg(net.pins.size()));
		item->setData(0, kNetRole, ni);
		if (net.ground) {
			QFont f = item->font(0);
			f.setItalic(true);
			item->setFont(0, f);
			item->setForeground(0, QColor(0x9a, 0xa3, 0xaf));
		}
		// A placeholder child so the expand arrow appears before lazy population.
		new QTreeWidgetItem(item);
		++shown;
	}

	setStatus(QStringLiteral("%1 of %2 nets%3")
	              .arg(shown)
	              .arg(m_board.nets.size())
	              .arg(q.isEmpty() ? QString() : QStringLiteral("  (filter: \"%1\")").arg(m_filter->text())));
	m_populating = false;
}

void NetInspector::onItemExpanded(QTreeWidgetItem *item)
{
	if (!item || !m_hasBoard) return;
	const QVariant nv = item->data(0, kNetRole);
	if (!nv.isValid()) return;
	if (item->childCount() != 1 || item->child(0)->data(0, kPinRole).isValid()) return; // already filled

	const int ni = nv.toInt();
	if (ni < 0 || ni >= m_board.nets.size()) return;
	const Net &net = m_board.nets[ni];

	// Replace the placeholder with the real pin list.
	delete item->takeChild(0);
	for (int pi : net.pins) {
		if (pi < 0 || pi >= m_board.pins.size()) continue;
		const Pin &pin = m_board.pins[pi];
		auto *child = new QTreeWidgetItem(item);
		child->setText(0, pin.name.isEmpty() ? QStringLiteral("(pin %1)").arg(pi)
		                                     : QStringLiteral("%1.%2").arg(pin.part, pin.name));
		child->setText(1, pin.layer == 0 ? QStringLiteral("T") : QStringLiteral("B"));
		child->setData(0, kPinRole, pi);
	}
}

void NetInspector::onSelectionChanged()
{
	if (m_populating) return;
	const auto sel = m_tree->selectedItems();
	if (sel.isEmpty()) return;
	const QVariant nv = sel.first()->data(0, kNetRole);
	if (!nv.isValid()) return;
	const int ni = nv.toInt();
	if (ni >= 0 && ni < m_board.nets.size()) emit netActivated(m_board.nets[ni].name);
}

void NetInspector::selectNet(const QString &net)
{
	if (!m_hasBoard || net.isEmpty()) return;
	m_populating = true;
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
		QTreeWidgetItem *item = m_tree->topLevelItem(i);
		if (item->text(0).compare(net, Qt::CaseInsensitive) == 0) {
			m_tree->setCurrentItem(item);
			m_tree->scrollToItem(item);
			item->setExpanded(true);
			break;
		}
	}
	m_populating = false;
}

} // namespace w2r
