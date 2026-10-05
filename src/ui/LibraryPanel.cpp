#include "ui/LibraryPanel.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <map>

namespace w2r {

namespace {
constexpr int kEntryRole = Qt::UserRole + 1;
}

LibraryPanel::LibraryPanel(QWidget *parent) : QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(6);

	m_filter = new QLineEdit(this);
	m_filter->setPlaceholderText(QStringLiteral("Filter brand, model or board number..."));
	m_filter->setClearButtonEnabled(true);
	connect(m_filter, &QLineEdit::textChanged, this, &LibraryPanel::onFilterChanged);
	layout->addWidget(m_filter);

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderLabels({QStringLiteral("Device / Package"), QStringLiteral("Contents")});
	m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_tree->setUniformRowHeights(true);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &LibraryPanel::onItemActivated);
	layout->addWidget(m_tree);

	auto *status = new QLabel(this);
	status->setObjectName(QStringLiteral("libraryStatus"));
	status->setStyleSheet(QStringLiteral("color:#9ca3af;font-size:10px;"));
	status->setText(QStringLiteral("Indexing..."));
	layout->addWidget(status);
	m_status = QStringLiteral("Indexing...");
}

void LibraryPanel::setStatus(const QString &text)
{
	m_status = text;
	if (auto *lbl = findChild<QLabel *>(QStringLiteral("libraryStatus"))) lbl->setText(text);
}

void LibraryPanel::setEntries(const QVector<LibraryEntry> &entries)
{
	m_entries = entries;
	repopulate();
}

void LibraryPanel::onFilterChanged(const QString &)
{
	repopulate();
}

void LibraryPanel::repopulate()
{
	m_tree->clear();

	const QString q = m_filter->text().trimmed().toLower();
	const QStringList tokens = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);

	// brand -> list of entry indices
	std::map<QString, QVector<int>> byBrand;
	for (int i = 0; i < m_entries.size(); ++i) {
		const LibraryEntry &e = m_entries[i];
		bool all = true;
		for (const QString &t : tokens) {
			if (!e.searchText.contains(t)) {
				all = false;
				break;
			}
		}
		if (!all) continue;
		byBrand[e.brand].append(i);
	}

	int shown = 0;
	for (auto &kv : byBrand) {
		auto *brandItem = new QTreeWidgetItem(m_tree);
		brandItem->setText(0, kv.first);
		brandItem->setText(1, QStringLiteral("%1").arg(kv.second.size()));
		brandItem->setFlags(brandItem->flags() & ~Qt::ItemIsSelectable);
		QFont f = brandItem->font(0);
		f.setBold(true);
		brandItem->setFont(0, f);

		for (int idx : kv.second) {
			const LibraryEntry &e = m_entries[idx];
			QStringList parts;
			if (e.hasBoard()) parts << QStringLiteral("PCB");
			if (e.hasSchematic()) parts << QStringLiteral("PDF");
			auto *item = new QTreeWidgetItem(brandItem);
			item->setText(0, e.name);
			item->setText(1, parts.join(QStringLiteral(" | ")));
			item->setToolTip(0, e.folder);
			item->setData(0, kEntryRole, idx);
			++shown;
		}
	}
	m_tree->expandAll();

	setStatus(QStringLiteral("%1 of %2 packages shown%3")
	              .arg(shown)
	              .arg(m_entries.size())
	              .arg(q.isEmpty() ? QString() : QStringLiteral("  (filter: \"%1\")").arg(m_filter->text())));
}

void LibraryPanel::onItemActivated(QTreeWidgetItem *item, int)
{
	if (!item) return;
	const QVariant v = item->data(0, kEntryRole);
	if (!v.isValid()) return;
	const int idx = v.toInt();
	if (idx >= 0 && idx < m_entries.size()) emit openRequested(m_entries[idx]);
}

} // namespace w2r
