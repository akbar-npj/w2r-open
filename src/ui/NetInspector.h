// Net Inspector dock: browse every net on the loaded board and its pins.
//
// Selecting a net cross-probes it (PCB highlight + schematic search). Pins are populated
// lazily when a net is expanded so boards with thousands of nets stay responsive.
#pragma once

#include "core/Board.h"

#include <QString>
#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace w2r {

class NetInspector : public QWidget {
	Q_OBJECT
  public:
	explicit NetInspector(QWidget *parent = nullptr);

	void setBoard(const Board &board);
	void clearBoard();
	void setStatus(const QString &text);

	// Programmatic selection (used by the test harness / cross-probe).
	void selectNet(const QString &net);

  signals:
	void netActivated(const QString &net);
	void pinActivated(const QString &part, const QString &pin);

  private slots:
	void onFilterChanged(const QString &text);
	void onSelectionChanged();
	void onItemExpanded(QTreeWidgetItem *item);

  private:
	void repopulate();

	QLineEdit *m_filter = nullptr;
	QTreeWidget *m_tree = nullptr;
	Board m_board;
	bool m_hasBoard = false;
	bool m_populating = false;
};

} // namespace w2r
