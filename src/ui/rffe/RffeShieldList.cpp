#include "ui/rffe/RffeShieldList.h"

#include <QColor>

namespace w2r {
namespace rffe {

RffeShieldList::RffeShieldList(QWidget *parent) : QListWidget(parent)
{
	setSelectionMode(QAbstractItemView::SingleSelection);
}

void RffeShieldList::setShields(const QList<Shield> &shields, const QString &currentName)
{
	clear();
	for (const Shield &s : shields) {
		QString label = s.name;
		if (s.hidden) label += QStringLiteral("  (hidden)");
		auto *item = new QListWidgetItem(label, this);
		item->setData(Qt::UserRole, s.name);
	}
	for (int i = 0; i < count(); ++i) {
		if (item(i)->data(Qt::UserRole).toString() == currentName) {
			setCurrentRow(i);
			break;
		}
	}
}

QString RffeShieldList::currentShield() const
{
	const QListWidgetItem *item = currentItem();
	return item ? item->data(Qt::UserRole).toString() : QString();
}

RffeReferenceList::RffeReferenceList(QWidget *parent) : QListWidget(parent)
{
	setSelectionMode(QAbstractItemView::SingleSelection);
	setUniformItemSizes(true);
}

void RffeReferenceList::setItems(const QStringList &items)
{
	clear();
	if (items.isEmpty()) {
		if (m_emptyHint.isEmpty()) return;
		auto *hint = new QListWidgetItem(m_emptyHint, this);
		hint->setFlags(Qt::NoItemFlags); // disabled, not selectable
		hint->setForeground(QColor(0xff6b7280));
		return;
	}
	for (const QString &text : items) addItem(text);
}

} // namespace rffe
} // namespace w2r
