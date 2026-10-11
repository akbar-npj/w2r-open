// Occurrences panel: lists every hit of the current schematic search (page + context)
// and jumps the PDF to the selected occurrence. Mirrors the app's OccurrencesPanel /
// pdfOccurrencesOverlay.
#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;

namespace w2r {

class OccurrencesPanel : public QWidget {
	Q_OBJECT
  public:
	explicit OccurrencesPanel(QWidget *parent = nullptr);

	// Replaces the list with the hits of a search; an empty list clears it.
	void setOccurrences(const QString &net, const QStringList &labels);
	void clear();

  signals:
	void occurrenceActivated(int index);

  private:
	QLabel *m_title = nullptr;
	QListWidget *m_list = nullptr;
};

} // namespace w2r
