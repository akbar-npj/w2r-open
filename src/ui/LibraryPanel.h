// Library browser dock: brand -> package tree with a live filter over the local index.
#pragma once

#include "core/library/LibraryIndex.h"

#include <QVector>
#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace w2r {

class LibraryPanel : public QWidget {
	Q_OBJECT
  public:
	explicit LibraryPanel(QWidget *parent = nullptr);

	void setEntries(const QVector<LibraryEntry> &entries);
	void setStatus(const QString &text);

  signals:
	void openRequested(const LibraryEntry &entry);

  private slots:
	void onFilterChanged(const QString &text);
	void onItemActivated(QTreeWidgetItem *item, int column);

  private:
	void repopulate();

	QVector<LibraryEntry> m_entries;
	QLineEdit *m_filter = nullptr;
	QTreeWidget *m_tree = nullptr;
	QString m_status;
};

} // namespace w2r
