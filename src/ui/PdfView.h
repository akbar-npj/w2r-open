// Schematic PDF viewer built on QtPdf (Google PDFium backend).
//
// Supports continuous multi-page scrolling, zoom / fit-to-width, full-text search with
// match navigation, and cross-probing: selecting a net on the PCB searches the schematic,
// and searching in the schematic highlights the net on the PCB.
//
// Compiled only when Qt6::Pdf is available; otherwise a placeholder panel is shown.
#pragma once

#include <QString>
#include <QWidget>

#ifdef W2R_HAVE_PDF
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfSearchModel>
#endif

class QLabel;
class QLineEdit;
class QSpinBox;
class QToolButton;
#ifdef W2R_HAVE_PDF
class QPdfView;
#endif

namespace w2r {

class PdfView : public QWidget {
	Q_OBJECT
  public:
	explicit PdfView(QWidget *parent = nullptr);

	// Loads a schematic PDF. Empty path clears the view.
	void loadPdf(const QString &path);
	void clear();
	bool hasDocument() const { return m_hasDoc; }
	QString currentPath() const { return m_path; }

  public slots:
	// Cross-probe target: search for a net and jump to the first occurrence.
	void searchNet(const QString &net);
	void zoomIn();
	void zoomOut();
	void fitWidth();
	void nextMatch();
	void prevMatch();

  signals:
	// Emitted when the user searches in the PDF (to highlight the net on the PCB).
	void netSearched(const QString &text);

  private slots:
	void onSearchSubmitted();
	void onMatchCountChanged();
	void onDocStatusChanged();

  private:
	void buildUi();
	void jumpToMatch(int index);

#ifdef W2R_HAVE_PDF
	QPdfDocument *m_doc = nullptr;
	QPdfSearchModel *m_search = nullptr;
	QPdfView *m_view = nullptr;
#endif
	QSpinBox *m_pageSpin = nullptr;
	QLabel *m_pageTotal = nullptr;
	QLabel *m_matchCount = nullptr;
	QLineEdit *m_searchEdit = nullptr;

	QString m_path;
	bool m_hasDoc = false;
	int m_currentMatch = 0;
};

} // namespace w2r
