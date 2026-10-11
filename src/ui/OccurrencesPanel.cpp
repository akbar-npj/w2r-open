#include "ui/OccurrencesPanel.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace w2r {

OccurrencesPanel::OccurrencesPanel(QWidget *parent) : QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(4);

	m_title = new QLabel(QStringLiteral("No search yet."), this);
	m_title->setStyleSheet(QStringLiteral("color:#9ca3af;font-size:10px;"));
	layout->addWidget(m_title);

	m_list = new QListWidget(this);
	m_list->setUniformItemSizes(false);
	m_list->setWordWrap(true);
	connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
		if (item) emit occurrenceActivated(m_list->row(item));
	});
	connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
		if (item) emit occurrenceActivated(m_list->row(item));
	});
	layout->addWidget(m_list, 1);
}

void OccurrencesPanel::setOccurrences(const QString &net, const QStringList &labels)
{
	m_list->clear();
	if (labels.isEmpty()) {
		m_title->setText(net.isEmpty() ? QStringLiteral("No search yet.")
		                               : QStringLiteral("No occurrences of \"%1\".").arg(net));
		return;
	}
	m_title->setText(QStringLiteral("%1 occurrence(s) of \"%2\"").arg(labels.size()).arg(net));
	m_list->addItems(labels);
	m_list->setCurrentRow(0);
}

void OccurrencesPanel::clear()
{
	m_list->clear();
	m_title->setText(QStringLiteral("No search yet."));
}

} // namespace w2r
