#include "ui/PdfView.h"

#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef W2R_HAVE_PDF
#include <QtPdf/QPdfLink>
#include <QtPdf/QPdfPageNavigator>
#include <QtPdfWidgets/QPdfView>
#endif

namespace w2r {

PdfView::PdfView(QWidget *parent) : QWidget(parent)
{
	buildUi();
}

void PdfView::buildUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto *bar = new QToolBar(QStringLiteral("Schematic"), this);

	bar->addWidget(new QLabel(QStringLiteral(" Page "), this));
	m_pageSpin = new QSpinBox(this);
	m_pageSpin->setMinimum(1);
	m_pageSpin->setMaximum(1);
	m_pageSpin->setEnabled(false);
	bar->addWidget(m_pageSpin);

	m_pageTotal = new QLabel(QStringLiteral(" / 1  "), this);
	bar->addWidget(m_pageTotal);

	bar->addSeparator();
	auto *zoomOut = new QToolButton(this);
	zoomOut->setText(QStringLiteral("Zoom -"));
	connect(zoomOut, &QToolButton::clicked, this, &PdfView::zoomOut);
	bar->addWidget(zoomOut);

	auto *zoomIn = new QToolButton(this);
	zoomIn->setText(QStringLiteral("Zoom +"));
	connect(zoomIn, &QToolButton::clicked, this, &PdfView::zoomIn);
	bar->addWidget(zoomIn);

	auto *fit = new QToolButton(this);
	fit->setText(QStringLiteral("Fit Width"));
	connect(fit, &QToolButton::clicked, this, &PdfView::fitWidth);
	bar->addWidget(fit);

	bar->addSeparator();
	bar->addWidget(new QLabel(QStringLiteral(" Search net "), this));
	m_searchEdit = new QLineEdit(this);
	m_searchEdit->setPlaceholderText(QStringLiteral("net or component..."));
	m_searchEdit->setClearButtonEnabled(true);
	m_searchEdit->setFixedWidth(180);
	connect(m_searchEdit, &QLineEdit::returnPressed, this, &PdfView::onSearchSubmitted);
	bar->addWidget(m_searchEdit);

	auto *prev = new QToolButton(this);
	prev->setText(QStringLiteral("<"));
	prev->setToolTip(QStringLiteral("Previous match"));
	connect(prev, &QToolButton::clicked, this, &PdfView::prevMatch);
	bar->addWidget(prev);

	auto *next = new QToolButton(this);
	next->setText(QStringLiteral(">"));
	next->setToolTip(QStringLiteral("Next match"));
	connect(next, &QToolButton::clicked, this, &PdfView::nextMatch);
	bar->addWidget(next);

	m_matchCount = new QLabel(QStringLiteral("  0 matches"), this);
	bar->addWidget(m_matchCount);

	layout->addWidget(bar);

#ifdef W2R_HAVE_PDF
	m_doc = new QPdfDocument(this);
	m_search = new QPdfSearchModel(this);
	m_search->setDocument(m_doc);

	m_view = new QPdfView(this);
	m_view->setDocument(m_doc);
	m_view->setSearchModel(m_search);
	m_view->setPageMode(QPdfView::PageMode::MultiPage);
	m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
	layout->addWidget(m_view, 1);

	connect(m_doc, &QPdfDocument::statusChanged, this, &PdfView::onDocStatusChanged);
	connect(m_search, &QPdfSearchModel::countChanged, this, &PdfView::onMatchCountChanged);
	connect(m_pageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) {
		if (!m_view || !m_doc) return;
		auto *nav = m_view->pageNavigator();
		if (nav) nav->jump(page - 1, nav->currentLocation(), nav->currentZoom());
	});
#else
	auto *placeholder = new QLabel(
	    QStringLiteral("PDF support was not built in.\nRebuild with Qt6::Pdf (qt6-qtpdf-devel)."), this);
	placeholder->setAlignment(Qt::AlignCenter);
	placeholder->setStyleSheet(QStringLiteral("color:#9ca3af;"));
	layout->addWidget(placeholder, 1);
	m_searchEdit->setEnabled(false);
#endif
}

void PdfView::loadPdf(const QString &path)
{
#ifdef W2R_HAVE_PDF
	if (path.isEmpty()) {
		clear();
		return;
	}
	m_path = path;
	m_doc->load(path);
	m_hasDoc = true;
#else
	Q_UNUSED(path);
#endif
}

void PdfView::clear()
{
#ifdef W2R_HAVE_PDF
	if (m_doc) m_doc->close();
#endif
	m_path.clear();
	m_hasDoc = false;
	m_currentMatch = 0;
	if (m_matchCount) m_matchCount->setText(QStringLiteral("  0 matches"));
	if (m_pageTotal) m_pageTotal->setText(QStringLiteral(" / 1  "));
	if (m_pageSpin) {
		m_pageSpin->setMaximum(1);
		m_pageSpin->setEnabled(false);
	}
}

#ifdef W2R_HAVE_PDF
void PdfView::onDocStatusChanged()
{
	if (m_doc->status() != QPdfDocument::Status::Ready) return;
	const int pages = m_doc->pageCount();
	m_pageSpin->setMaximum(pages);
	m_pageSpin->setEnabled(pages > 0);
	m_pageTotal->setText(QStringLiteral(" / %1  ").arg(pages));
	fitWidth();
}

void PdfView::onMatchCountChanged()
{
	const int count = m_search->count();
	m_matchCount->setText(QStringLiteral("  %1 matches").arg(count));
	if (count > 0) {
		m_currentMatch = 0;
		jumpToMatch(0);
	}
}

void PdfView::jumpToMatch(int index)
{
	if (!m_view || !m_search) return;
	if (index < 0 || index >= m_search->count()) return;
	const QPdfLink link = m_search->resultAtIndex(index);
	if (!link.isValid()) return;
	auto *nav = m_view->pageNavigator();
	if (!nav) return;
	nav->jump(link.page(), link.location(), nav->currentZoom());
	m_pageSpin->blockSignals(true);
	m_pageSpin->setValue(link.page() + 1);
	m_pageSpin->blockSignals(false);
}
#endif

void PdfView::searchNet(const QString &net)
{
#ifdef W2R_HAVE_PDF
	if (net.isEmpty() || net.compare(QStringLiteral("GND"), Qt::CaseInsensitive) == 0) return;
	m_searchEdit->setText(net);
	m_search->setSearchString(net);
	m_currentMatch = 0;
#else
	Q_UNUSED(net);
#endif
}

void PdfView::onSearchSubmitted()
{
	const QString text = m_searchEdit->text().trimmed();
	if (text.isEmpty()) return;
#ifdef W2R_HAVE_PDF
	m_search->setSearchString(text);
	m_currentMatch = 0;
#endif
	emit netSearched(text);
}

void PdfView::nextMatch()
{
#ifdef W2R_HAVE_PDF
	const int count = m_search->count();
	if (count <= 0) return;
	m_currentMatch = (m_currentMatch + 1) % count;
	jumpToMatch(m_currentMatch);
#endif
}

void PdfView::prevMatch()
{
#ifdef W2R_HAVE_PDF
	const int count = m_search->count();
	if (count <= 0) return;
	m_currentMatch = (m_currentMatch - 1 + count) % count;
	jumpToMatch(m_currentMatch);
#endif
}

void PdfView::zoomIn()
{
#ifdef W2R_HAVE_PDF
	if (!m_view) return;
	m_view->setZoomMode(QPdfView::ZoomMode::Custom);
	m_view->setZoomFactor(m_view->zoomFactor() * 1.2);
#endif
}

void PdfView::zoomOut()
{
#ifdef W2R_HAVE_PDF
	if (!m_view) return;
	m_view->setZoomMode(QPdfView::ZoomMode::Custom);
	m_view->setZoomFactor(m_view->zoomFactor() / 1.2);
#endif
}

void PdfView::fitWidth()
{
#ifdef W2R_HAVE_PDF
	if (m_view) m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
#endif
}

} // namespace w2r
