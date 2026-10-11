#include "ui/MainWindow.h"

#include "core/BoardLoader.h"
#include "core/DiodeSheet.h"
#include "core/Settings.h"
#include "ui/LibraryPanel.h"
#include "ui/NetInspector.h"
#include "ui/OccurrencesPanel.h"
#include "ui/PcbView.h"
#include "ui/PdfView.h"
#include "ui/PreferencesDialog.h"
#include "ui/RffePanel.h"
#include "ui/ThemeEditorDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTranslator>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace w2r {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle(QStringLiteral("W2R Open Schematics & PCB Viewer"));
	resize(1400, 900);

	buildCentralViews();
	m_view->setTheme(m_themes.theme(0));
	buildToolbar();
	buildMenus();
	buildLibraryDock();
	buildNetDock();
	buildRffeDock();
	buildOccurrencesDock();

	m_statusLabel = new QLabel(this);
	statusBar()->addPermanentWidget(m_statusLabel);
	statusBar()->showMessage(QStringLiteral("Ready. Open a .brd or .bvr boardview."));

	connect(m_view, &PcbView::pinSelected, this, &MainWindow::onPinSelected);
	connect(m_view, &PcbView::netSelected, this, &MainWindow::onNetSelected);
	connect(m_pdf, &PdfView::netSearched, this, &MainWindow::onPdfNetSearched);
	connect(m_pdf2, &PdfView::netSearched, this, &MainWindow::onPdfNetSearched);

	// Restore saved window geometry and dock layout.
	const QByteArray geo = Settings::windowGeometry();
	if (!geo.isEmpty()) restoreGeometry(geo);
	const QByteArray state = Settings::windowState();
	if (!state.isEmpty()) restoreState(state);

	// Restore saved view preferences.
	const QString savedTheme = Settings::themeName();
	if (!savedTheme.isEmpty()) setThemeByName(savedTheme);
	if (m_diodeAction) {
		m_diodeAction->setChecked(Settings::showDiodeReadings());
		m_view->setShowDiodeReadings(m_diodeAction->isChecked());
	}
	if (m_ratsnestAction) {
		m_ratsnestAction->setChecked(Settings::showRatsnest());
		m_view->setShowRatsnest(m_ratsnestAction->isChecked());
	}

	applyTreeFont(Settings::treeFontPointSize());

	// Restore animation preferences.
	m_view->setAnimationsEnabled(Settings::animationsEnabled());
	m_view->setSpotlightDim(Settings::spotlightDim());
	m_view->setNetPulse(Settings::netPulse());
	m_view->setTraceReveal(Settings::traceReveal());

	startLibraryScan();
}

void MainWindow::buildCentralViews()
{
	m_splitter = new QSplitter(Qt::Horizontal, this);
	m_view = new PcbView(m_splitter);
	m_pdf = new PdfView(m_splitter);
	m_pdf2 = new PdfView(m_splitter);
	m_splitter->addWidget(m_view);
	m_splitter->addWidget(m_pdf);
	m_splitter->addWidget(m_pdf2);
	m_splitter->setStretchFactor(0, 1);
	m_splitter->setStretchFactor(1, 1);
	m_splitter->setStretchFactor(2, 1);
	m_pdf2->hide(); // only shown in the PDF/PDF split mode
	setCentralWidget(m_splitter);
}

void MainWindow::setViewMode(const QString &mode)
{
	if (!mode.isEmpty()) m_viewMode = mode;

	QVector<QWidget *> order;
	if (m_viewMode == QLatin1String("pcb"))
		order = {m_view};
	else if (m_viewMode == QLatin1String("pdf"))
		order = {m_pdf};
	else if (m_viewMode == QLatin1String("pdfpdf"))
		order = {m_pdf, m_pdf2};
	else
		order = {m_view, m_pdf}; // "both": split PDF / PCB

	if (m_swapped) std::reverse(order.begin(), order.end());

	m_view->hide();
	m_pdf->hide();
	m_pdf2->hide();
	for (QWidget *w : order) w->show();

	// Re-insert visible panes in order, then the hidden ones (hidden panes take no space).
	int pos = 0;
	for (QWidget *w : order) m_splitter->insertWidget(pos++, w);
	const QVector<QWidget *> all = {m_view, m_pdf, m_pdf2};
	for (QWidget *w : all)
		if (!order.contains(w)) m_splitter->insertWidget(pos++, w);

	if (order.size() == 2) {
		const int half = qMax(200, m_splitter->width() / 2);
		m_splitter->setSizes({half, half});
	}
}

void MainWindow::swapPanes()
{
	m_swapped = !m_swapped;
	setViewMode(QString());
}

void MainWindow::buildLibraryDock()
{
	auto *dock = new QDockWidget(QStringLiteral("Device Library"), this);
	dock->setObjectName(QStringLiteral("libraryDock"));
	dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	m_library = new LibraryPanel(dock);
	dock->setWidget(m_library);
	addDockWidget(Qt::LeftDockWidgetArea, dock);
	dock->setMinimumWidth(320);
	connect(m_library, &LibraryPanel::openRequested, this, &MainWindow::onLibraryOpen);
}

void MainWindow::buildNetDock()
{
	auto *dock = new QDockWidget(QStringLiteral("Net Inspector"), this);
	dock->setObjectName(QStringLiteral("netDock"));
	dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	m_nets = new NetInspector(dock);
	dock->setWidget(m_nets);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	dock->setMinimumWidth(280);
	connect(m_nets, &NetInspector::netActivated, this, &MainWindow::crossProbeNet);
	connect(m_nets, &NetInspector::pinActivated, this, &MainWindow::onNetInspectorPin);
}

void MainWindow::onNetInspectorPin(const QString &part, const QString &pin)
{
	if (part.isEmpty() && pin.isEmpty()) return;
	statusBar()->showMessage(QStringLiteral("Pin %1.%2").arg(part, pin), 5000);
}

void MainWindow::buildRffeDock()
{
	m_rffeDock = new QDockWidget(QStringLiteral("RFFE Probe"), this);
	m_rffeDock->setObjectName(QStringLiteral("rffeDock"));
	m_rffeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
	                        QDockWidget::DockWidgetClosable);
	m_rffe = new RffePanel(m_rffeDock);
	m_rffeDock->setWidget(m_rffe);
	addDockWidget(Qt::RightDockWidgetArea, m_rffeDock);
	// Stack below the Net Inspector rather than tabbing over it.
	if (auto *netDock = findChild<QDockWidget *>(QStringLiteral("netDock")))
		splitDockWidget(netDock, m_rffeDock, Qt::Vertical);
	m_rffeDock->hide(); // opened on demand from the toolbar
	connect(m_rffe, &RffePanel::deviceDetected, this, &MainWindow::onRffeDevice);
	if (m_rffeAction)
		connect(m_rffeDock, &QDockWidget::visibilityChanged, m_rffeAction, &QAction::setChecked);

	// --- RFFE <-> boardview wiring ---
	if (m_view) {
		// Marks collected in the panel are highlighted/blinked on the canvas.
		m_view->setRffeMarkSettings(rffe::MarkSettings::load());
		m_view->setRffeMarkedNets(m_rffe->markedNets());
		connect(m_rffe, &RffePanel::markedNetsChanged, m_view, &PcbView::setRffeMarkedNets);
		connect(m_rffe, &RffePanel::blinkRequested, m_view, &PcbView::blinkRffeNets);
		connect(m_rffe, &RffePanel::markSettingsChanged, this, [this] {
			m_view->setRffeMarkSettings(rffe::MarkSettings::load());
		});
		// Boardview context-menu actions feed the panel.
		connect(m_view, &PcbView::rffeTestPointRequested, this,
		        [this](const QString &refDes, const QString &net) {
			        m_rffe->addTestPoint(refDes, net, static_cast<int>(rffe::Role::None));
		        });
		connect(m_view, &PcbView::rffeRoleRequested, this,
		        [this](const QString &net, int role) { m_rffe->setTestPointRole(net, role); });
		connect(m_view, &PcbView::netSelected, m_rffe, &RffePanel::setCurrentNet);
	}
}

void MainWindow::buildOccurrencesDock()
{
	auto *dock = new QDockWidget(QStringLiteral("Occurrences"), this);
	dock->setObjectName(QStringLiteral("occDock"));
	dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
	                  QDockWidget::DockWidgetClosable);
	m_occ = new OccurrencesPanel(dock);
	dock->setWidget(m_occ);
	addDockWidget(Qt::BottomDockWidgetArea, dock);
	dock->hide(); // appears when a schematic search yields hits

	// Primary PDF pane drives the panel; clicking an entry jumps the PDF to it.
	connect(m_pdf, &PdfView::occurrencesChanged, this, [this, dock](const QString &net, const QStringList &labels) {
		m_occ->setOccurrences(net, labels);
		if (!labels.isEmpty()) dock->show();
	});
	connect(m_occ, &OccurrencesPanel::occurrenceActivated, m_pdf, &PdfView::activateMatch);
}

void MainWindow::onRffeDevice(const QString &id)
{
	statusBar()->showMessage(QStringLiteral("RFFE device detected: %1").arg(id), 8000);
}

void MainWindow::startLibraryScan()
{
	const QStringList roots = LibraryIndex::defaultRoots();
	if (roots.isEmpty()) {
		m_library->setStatus(QStringLiteral("No library roots (set W2R_LIBRARY_ROOTS)"));
		return;
	}

	m_library->setStatus(QStringLiteral("Indexing %1...").arg(roots.join(QStringLiteral(", "))));
	statusBar()->showMessage(QStringLiteral("Indexing library (first run can take a minute)..."));

	m_scanWatcher = new QFutureWatcher<QVector<LibraryEntry>>(this);
	connect(m_scanWatcher, &QFutureWatcher<QVector<LibraryEntry>>::finished, this, [this] {
		const QVector<LibraryEntry> entries = m_scanWatcher->result();
		m_library->setEntries(entries);
		statusBar()->showMessage(
		    QStringLiteral("Library ready: %1 packages.").arg(entries.size()), 6000);
	});

	m_scanWatcher->setFuture(QtConcurrent::run([] {
		LibraryIndex idx;
		idx.setRoots(LibraryIndex::defaultRoots());
		idx.scan();
		return idx.entries();
	}));
}

void MainWindow::onLibraryOpen(const LibraryEntry &entry)
{
	if (entry.boards.isEmpty()) {
		statusBar()->showMessage(QStringLiteral("No boardview in this package."));
		return;
	}

	// Prefer a format we can already render.
	const LibraryFile *chosen = nullptr;
	for (const LibraryFile &b : entry.boards) {
		if (b.format == QLatin1String("BRD") || b.format == QLatin1String("BVR3")) {
			chosen = &b;
			break;
		}
	}
	if (!chosen) chosen = &entry.boards.first();

	statusBar()->showMessage(QStringLiteral("Loading %1...").arg(chosen->displayName()));
	QString error;
	const QString localPath = LibraryIndex::materialize(*chosen, &error);
	if (localPath.isEmpty()) {
		statusBar()->showMessage(error);
		QMessageBox::warning(this, QStringLiteral("Load failed"), error);
		return;
	}
	openBoard(localPath);

	if (!entry.schematics.isEmpty()) {
		QString serr;
		const QString pdf = LibraryIndex::materialize(entry.schematics.first(), &serr);
		if (!pdf.isEmpty())
			openSchematic(pdf);
		else
			statusBar()->showMessage(QStringLiteral("Schematic not available: %1").arg(serr));
	}
}

void MainWindow::buildToolbar()
{
	auto *tb = addToolBar(QStringLiteral("Main"));
	tb->setObjectName(QStringLiteral("mainToolBar"));
	tb->setMovable(false);

	QAction *open = tb->addAction(QStringLiteral("Open Board"));
	open->setShortcut(QKeySequence::Open);
	connect(open, &QAction::triggered, this, &MainWindow::onOpenBoard);

	QAction *openPdf = tb->addAction(QStringLiteral("Open Schematic"));
	connect(openPdf, &QAction::triggered, this, &MainWindow::onOpenSchematic);

	tb->addSeparator();

	QAction *fit = tb->addAction(QStringLiteral("Fit"));
	fit->setShortcut(Qt::Key_Home);
	connect(fit, &QAction::triggered, m_view, &PcbView::fitToBoard);

	QAction *flip = tb->addAction(QStringLiteral("Flip Side"));
	flip->setShortcut(Qt::Key_Space);
	connect(flip, &QAction::triggered, m_view, &PcbView::flipSide);

	QAction *rot = tb->addAction(QStringLiteral("Rotate"));
	rot->setShortcut(Qt::Key_R);
	connect(rot, &QAction::triggered, m_view, &PcbView::rotateCw);

	QAction *zoomIn = tb->addAction(QStringLiteral("Zoom +"));
	zoomIn->setShortcut(QKeySequence::ZoomIn);
	connect(zoomIn, &QAction::triggered, m_view, &PcbView::zoomIn);

	QAction *zoomOut = tb->addAction(QStringLiteral("Zoom -"));
	zoomOut->setShortcut(QKeySequence::ZoomOut);
	connect(zoomOut, &QAction::triggered, m_view, &PcbView::zoomOut);

	tb->addSeparator();

	QAction *diode = tb->addAction(QStringLiteral("Diode Readings"));
	diode->setCheckable(true);
	diode->setChecked(true);
	connect(diode, &QAction::toggled, m_view, &PcbView::setShowDiodeReadings);
	m_diodeAction = diode;

	tb->addSeparator();
	QAction *pcbOnly = tb->addAction(QStringLiteral("PCB Only"));
	pcbOnly->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
	connect(pcbOnly, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pcb")); });
	QAction *pdfOnly = tb->addAction(QStringLiteral("PDF Only"));
	pdfOnly->setShortcut(QKeySequence(QStringLiteral("Ctrl+2")));
	connect(pdfOnly, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pdf")); });
	QAction *splitPcb = tb->addAction(QStringLiteral("PDF | PCB"));
	splitPcb->setShortcut(QKeySequence(QStringLiteral("Ctrl+3")));
	connect(splitPcb, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("both")); });
	QAction *splitPdf = tb->addAction(QStringLiteral("PDF | PDF"));
	splitPdf->setShortcut(QKeySequence(QStringLiteral("Ctrl+4")));
	connect(splitPdf, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pdfpdf")); });
	QAction *swap = tb->addAction(QStringLiteral("Swap"));
	connect(swap, &QAction::triggered, this, &MainWindow::swapPanes);

	tb->addSeparator();
	QAction *ratsnest = tb->addAction(QStringLiteral("Ratsnest"));
	ratsnest->setCheckable(true);
	ratsnest->setChecked(true);
	connect(ratsnest, &QAction::toggled, m_view, &PcbView::setShowRatsnest);
	m_ratsnestAction = ratsnest;

	QAction *full = tb->addAction(QStringLiteral("Full Screen"));
	full->setShortcut(Qt::Key_F11);
	connect(full, &QAction::triggered, this, &MainWindow::toggleFullScreen);

	tb->addSeparator();
	QAction *rffe = tb->addAction(QStringLiteral("RFFE Probe"));
	rffe->setCheckable(true);
	m_rffeAction = rffe;
	connect(rffe, &QAction::toggled, this, [this](bool on) {
		if (!m_rffeDock) return;
		m_rffeDock->setVisible(on);
		if (on) m_rffeDock->raise();
	});

	tb->addSeparator();
	tb->addWidget(new QLabel(QStringLiteral(" Theme: "), this));
	m_themeCombo = new QComboBox(this);
	m_themeCombo->addItems(m_themes.names());
	connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
	        &MainWindow::onThemeChanged);
	tb->addWidget(m_themeCombo);
}

void MainWindow::setThemeByName(const QString &name)
{
	const int index = m_themes.indexOfName(name);
	m_view->setTheme(m_themes.theme(index));
	if (m_themeCombo && m_themeCombo->currentIndex() != index) {
		m_themeCombo->blockSignals(true);
		m_themeCombo->setCurrentIndex(index);
		m_themeCombo->blockSignals(false);
	}
}

void MainWindow::onThemeChanged(int index)
{
	m_view->setTheme(m_themes.theme(index));
	Settings::setThemeName(m_themes.names().value(index));
}

void MainWindow::rebuildThemeCombo()
{
	if (!m_themeCombo) return;
	const int previous = m_themeCombo->currentIndex();
	m_themeCombo->blockSignals(true);
	m_themeCombo->clear();
	m_themeCombo->addItems(m_themes.names());
	const int target = (previous >= 0 && previous < m_themeCombo->count()) ? previous : 0;
	m_themeCombo->setCurrentIndex(target);
	m_themeCombo->blockSignals(false);
}

// Applies the theme at index, syncs the combo and persists the choice.
void MainWindow::onThemeCreate()
{
	const int base = m_themeCombo ? m_themeCombo->currentIndex() : 0;
	Theme draft = m_themes.theme(base);
	draft.name = QStringLiteral("Custom");
	draft.raw = QJsonObject(); // new theme: start from a clean object

	ThemeEditorDialog dlg(draft, this);
	if (dlg.exec() != QDialog::Accepted) return;

	const int index = m_themes.addOrUpdate(dlg.theme());
	m_themes.saveUserThemes();
	rebuildThemeCombo();
	m_themeCombo->setCurrentIndex(index);
	setThemeByName(m_themes.names().value(index));
	Settings::setThemeName(m_themes.names().value(index));
	statusBar()->showMessage(QStringLiteral("Theme created: %1").arg(m_themes.names().value(index)), 5000);
}

void MainWindow::onThemeEdit()
{
	const int index = m_themeCombo ? m_themeCombo->currentIndex() : -1;
	if (index < 0) return;

	ThemeEditorDialog dlg(m_themes.theme(index), this);
	if (dlg.exec() != QDialog::Accepted) return;

	// Editing a built-in theme saves a copy as a new user theme; editing a user theme
	// updates it in place.
	const int saved = m_themes.addOrUpdate(dlg.theme(), index);
	m_themes.saveUserThemes();
	rebuildThemeCombo();
	m_themeCombo->setCurrentIndex(saved);
	setThemeByName(m_themes.names().value(saved));
	Settings::setThemeName(m_themes.names().value(saved));
	statusBar()->showMessage(QStringLiteral("Theme saved: %1").arg(m_themes.names().value(saved)), 5000);
}

void MainWindow::onThemeDelete()
{
	const int index = m_themeCombo ? m_themeCombo->currentIndex() : -1;
	if (!m_themes.isUserTheme(index)) {
		QMessageBox::information(this, QStringLiteral("Delete Theme"),
		                         QStringLiteral("Built-in themes cannot be deleted."));
		return;
	}
	m_themes.removeTheme(index);
	m_themes.saveUserThemes();
	rebuildThemeCombo();
	const int sel = qBound(0, index, qMax(0, m_themeCombo->count() - 1));
	m_themeCombo->setCurrentIndex(sel);
	setThemeByName(m_themes.names().value(sel));
	Settings::setThemeName(m_themes.names().value(sel));
	statusBar()->showMessage(QStringLiteral("Theme deleted."), 5000);
}

void MainWindow::setRffeVisible(bool on)
{
	if (m_rffeAction) m_rffeAction->setChecked(on);
	if (m_rffeDock) {
		m_rffeDock->setVisible(on);
		if (on) m_rffeDock->raise();
	}
}

void MainWindow::setAnimations(const QString &spec)
{
	const QStringList flags = spec.split(QLatin1Char(','), Qt::SkipEmptyParts);
	const bool spotlight = flags.contains(QStringLiteral("spotlight"));
	const bool pulse = flags.contains(QStringLiteral("netPulse"));
	const bool reveal = flags.contains(QStringLiteral("traceReveal"));

	m_view->setAnimationsEnabled(true);
	m_view->setSpotlightDim(spotlight);
	m_view->setNetPulse(pulse);
	m_view->setTraceReveal(reveal);
	if (m_spotlightAction) m_spotlightAction->setChecked(spotlight);
	if (m_netPulseAction) m_netPulseAction->setChecked(pulse);
	if (m_traceRevealAction) m_traceRevealAction->setChecked(reveal);
}

void MainWindow::onPreferences()
{
	const QStringList oldRoots = LibraryIndex::defaultRoots();
	PreferencesDialog dlg(m_themes.names(), this);
	if (dlg.exec() != QDialog::Accepted) return;

	Settings::setLibraryRoots(dlg.libraryRoots());
	Settings::setThemeName(dlg.themeName());
	Settings::setShowDiodeReadings(dlg.showDiodeReadings());
	Settings::setShowRatsnest(dlg.showRatsnest());

	setThemeByName(dlg.themeName());
	if (m_diodeAction) {
		m_diodeAction->setChecked(dlg.showDiodeReadings());
		m_view->setShowDiodeReadings(dlg.showDiodeReadings());
	}
	if (m_ratsnestAction) {
		m_ratsnestAction->setChecked(dlg.showRatsnest());
		m_view->setShowRatsnest(dlg.showRatsnest());
	}

	if (dlg.libraryRoots() != oldRoots) {
		statusBar()->showMessage(QStringLiteral("Library roots changed — rescanning..."));
		startLibraryScan();
	}
}

void MainWindow::toggleFullScreen()
{
	if (isFullScreen())
		showNormal();
	else
		showFullScreen();
}

void MainWindow::applyTreeFont(int pointSize)
{
	pointSize = qBound(0, pointSize, 32);
	m_treeFontPointSize = pointSize;
	if (m_library) m_library->setTreeFontPointSize(pointSize);
	if (m_nets) m_nets->setTreeFontPointSize(pointSize);
	Settings::setTreeFontPointSize(pointSize);
}

void MainWindow::onTreeFontIncrease()
{
	int base = m_treeFontPointSize;
	if (base <= 0) base = QApplication::font().pointSize();
	if (base <= 0) base = 10;
	applyTreeFont(base + 1);
}

void MainWindow::onTreeFontDecrease()
{
	int base = m_treeFontPointSize;
	if (base <= 0) base = QApplication::font().pointSize();
	if (base <= 0) base = 10;
	applyTreeFont(qMax(6, base - 1));
}

void MainWindow::onTreeFontReset()
{
	applyTreeFont(0);
}

void MainWindow::retranslateMenus()
{
	if (m_fileMenu) m_fileMenu->setTitle(tr("&File"));
	if (m_viewMenu) m_viewMenu->setTitle(tr("&View"));
	if (m_themeMenu) m_themeMenu->setTitle(tr("&Theme"));
	if (m_rffeMenu) m_rffeMenu->setTitle(tr("&RFFE"));
	if (m_helpMenu) m_helpMenu->setTitle(tr("&Help"));
	if (m_languageMenu) m_languageMenu->setTitle(tr("&Language"));
}

void MainWindow::onLanguageSelected(const QString &code)
{
	Settings::setLanguage(code);

	// Swap in the translator for the chosen language so menus retranslate where strings
	// use tr(). Missing .qm files are not an error: the UI stays in its source language.
	auto *translator = new QTranslator(this);
	const QString name = QStringLiteral("w2r-open_") + code;
	const QStringList dirs = {
	    QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
	    QCoreApplication::applicationDirPath() + QStringLiteral("/../share/w2r-open/translations"),
	    QCoreApplication::applicationDirPath(),
	};
	bool loaded = false;
	for (const QString &dir : dirs) {
		if (translator->load(name, dir)) {
			loaded = true;
			break;
		}
	}
	if (loaded) {
		if (m_translator) qApp->removeTranslator(m_translator);
		qApp->installTranslator(translator);
		m_translator = translator;
	} else {
		delete translator;
	}

	retranslateMenus();
	statusBar()->showMessage(tr("Language set to %1").arg(code), 5000);
}

void MainWindow::buildMenus()
{
	QMenu *file = menuBar()->addMenu(tr("&File"));
	m_fileMenu = file;
	QAction *open = file->addAction(QStringLiteral("&Open Board..."));
	open->setShortcut(QKeySequence::Open);
	connect(open, &QAction::triggered, this, &MainWindow::onOpenBoard);
	QAction *openPdf = file->addAction(QStringLiteral("Open &Schematic..."));
	connect(openPdf, &QAction::triggered, this, &MainWindow::onOpenSchematic);

	m_recentMenu = file->addMenu(QStringLiteral("Open &Recent"));
	m_recentFiles = Settings::recentFiles();
	rebuildRecentMenu();

	file->addSeparator();
	QAction *diode = file->addAction(QStringLiteral("&Import Diode Readings..."));
	connect(diode, &QAction::triggered, this, &MainWindow::onImportDiode);
	file->addSeparator();
	QAction *prefs = file->addAction(QStringLiteral("&Preferences..."));
	prefs->setShortcut(QKeySequence::Preferences);
	connect(prefs, &QAction::triggered, this, &MainWindow::onPreferences);
	file->addSeparator();
	QAction *quit = file->addAction(QStringLiteral("&Quit"));
	quit->setShortcut(QKeySequence::Quit);
	connect(quit, &QAction::triggered, this, &QWidget::close);

	QMenu *view = menuBar()->addMenu(tr("&View"));
	m_viewMenu = view;
	QAction *splitPcb = view->addAction(QStringLiteral("Split PDF / &PCB"));
	splitPcb->setShortcut(QKeySequence(QStringLiteral("Ctrl+3")));
	connect(splitPcb, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("both")); });
	QAction *splitPdf = view->addAction(QStringLiteral("Split PDF / PDF"));
	splitPdf->setShortcut(QKeySequence(QStringLiteral("Ctrl+4")));
	connect(splitPdf, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pdfpdf")); });
	QAction *pcbOnly = view->addAction(QStringLiteral("PCB Only"));
	pcbOnly->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
	connect(pcbOnly, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pcb")); });
	QAction *pdfOnly = view->addAction(QStringLiteral("PDF Only"));
	pdfOnly->setShortcut(QKeySequence(QStringLiteral("Ctrl+2")));
	connect(pdfOnly, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pdf")); });
	QAction *swap = view->addAction(QStringLiteral("&Swap Panes"));
	connect(swap, &QAction::triggered, this, &MainWindow::swapPanes);

	view->addSeparator();
	QMenu *anim = view->addMenu(QStringLiteral("&Animations"));
	m_animMasterAction = anim->addAction(QStringLiteral("Enable Animations"));
	m_animMasterAction->setCheckable(true);
	m_animMasterAction->setChecked(Settings::animationsEnabled());
	connect(m_animMasterAction, &QAction::toggled, this, [this](bool on) {
		Settings::setAnimationsEnabled(on);
		m_view->setAnimationsEnabled(on);
	});
	m_spotlightAction = anim->addAction(QStringLiteral("Spotlight (dim others)"));
	m_spotlightAction->setCheckable(true);
	m_spotlightAction->setChecked(Settings::spotlightDim());
	connect(m_spotlightAction, &QAction::toggled, this, [this](bool on) {
		Settings::setSpotlightDim(on);
		m_view->setSpotlightDim(on);
	});
	m_netPulseAction = anim->addAction(QStringLiteral("Net pulse"));
	m_netPulseAction->setCheckable(true);
	m_netPulseAction->setChecked(Settings::netPulse());
	connect(m_netPulseAction, &QAction::toggled, this, [this](bool on) {
		Settings::setNetPulse(on);
		m_view->setNetPulse(on);
	});
	m_traceRevealAction = anim->addAction(QStringLiteral("Trace reveal"));
	m_traceRevealAction->setCheckable(true);
	m_traceRevealAction->setChecked(Settings::traceReveal());
	connect(m_traceRevealAction, &QAction::toggled, this, [this](bool on) {
		Settings::setTraceReveal(on);
		m_view->setTraceReveal(on);
	});

	view->addSeparator();
	QMenu *treeFont = view->addMenu(QStringLiteral("Tree &Font"));
	QAction *fontInc = treeFont->addAction(QStringLiteral("Increase"));
	connect(fontInc, &QAction::triggered, this, &MainWindow::onTreeFontIncrease);
	QAction *fontDec = treeFont->addAction(QStringLiteral("Decrease"));
	connect(fontDec, &QAction::triggered, this, &MainWindow::onTreeFontDecrease);
	QAction *fontReset = treeFont->addAction(QStringLiteral("Reset"));
	connect(fontReset, &QAction::triggered, this, &MainWindow::onTreeFontReset);

	view->addSeparator();
	QAction *full = view->addAction(QStringLiteral("&Full Screen"));
	full->setShortcut(Qt::Key_F11);
	connect(full, &QAction::triggered, this, &MainWindow::toggleFullScreen);

	QMenu *theme = menuBar()->addMenu(tr("&Theme"));
	m_themeMenu = theme;
	QAction *themeCreate = theme->addAction(QStringLiteral("&Create Theme..."));
	connect(themeCreate, &QAction::triggered, this, &MainWindow::onThemeCreate);
	QAction *themeEdit = theme->addAction(QStringLiteral("&Edit Current Theme..."));
	connect(themeEdit, &QAction::triggered, this, &MainWindow::onThemeEdit);
	QAction *themeDelete = theme->addAction(QStringLiteral("&Delete Current Theme"));
	connect(themeDelete, &QAction::triggered, this, &MainWindow::onThemeDelete);

	// RFFE: probe panel + the original app's RFFE entry points.
	QMenu *rffe = menuBar()->addMenu(tr("&RFFE"));
	m_rffeMenu = rffe;
	QAction *rffeShow = rffe->addAction(QStringLiteral("Open RFFE &Panel"));
	rffeShow->setCheckable(true);
	rffeShow->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
	if (m_rffeAction) rffeShow->setChecked(m_rffeAction->isChecked());
	connect(rffeShow, &QAction::toggled, this, [this](bool on) {
		if (!m_rffeDock) return;
		m_rffeDock->setVisible(on);
		if (on) m_rffeDock->raise();
	});
	// Keep the menu check in sync when the dock is toggled from the toolbar.
	if (m_rffeAction) {
		connect(m_rffeAction, &QAction::toggled, rffeShow, &QAction::setChecked);
		connect(rffeShow, &QAction::toggled, m_rffeAction, &QAction::setChecked);
	}
	rffe->addSeparator();
	QAction *rffeFirmware = rffe->addAction(QStringLiteral("&Update Probe Firmware..."));
	connect(rffeFirmware, &QAction::triggered, this, [this] {
		if (m_rffe) m_rffe->openFlashDialog();
	});
	QAction *rffeSettings = rffe->addAction(QStringLiteral("RFFE &Settings..."));
	connect(rffeSettings, &QAction::triggered, this, [this] {
		if (m_rffe) m_rffe->openSettingsDialog();
	});
	rffe->addSeparator();
	QAction *rffeBlink = rffe->addAction(QStringLiteral("&Blink RFFE Nets"));
	connect(rffeBlink, &QAction::triggered, this, [this] {
		if (m_view) m_view->blinkRffeNets();
	});
	QAction *rffeExport = rffe->addAction(QStringLiteral("&Export RFFE Test Points..."));
	connect(rffeExport, &QAction::triggered, this, [this] {
		if (m_rffe) m_rffe->exportTestPoints();
	});

	QMenu *help = menuBar()->addMenu(tr("&Help"));
	m_helpMenu = help;

	// Help -> Language: same language set as the original app (en/ar/ru/ta/th).
	m_languageMenu = help->addMenu(tr("&Language"));
	auto *langGroup = new QActionGroup(this);
	langGroup->setExclusive(true);
	struct Lang { const char *code; const char *label; };
	const Lang langs[] = {{"en", "English"}, {"ar", "العربية"}, {"ru", "Русский"},
	                      {"ta", "தமிழ்"}, {"th", "ไทย"}};
	const QString current = Settings::language();
	for (const Lang &l : langs) {
		QAction *a = m_languageMenu->addAction(QString::fromUtf8(l.label));
		a->setCheckable(true);
		a->setChecked(current == QLatin1String(l.code) ||
		              (current.isEmpty() && QLocale::system().name().startsWith(QLatin1String(l.code))));
		const QString code = QString::fromLatin1(l.code);
		connect(a, &QAction::triggered, this, [this, code] { onLanguageSelected(code); });
		langGroup->addAction(a);
	}

	help->addSeparator();
	QAction *about = help->addAction(tr("&About"));
	connect(about, &QAction::triggered, this, [this] {
		QMessageBox::about(this, tr("About W2R Open"),
		                   tr("<b>W2R Open Schematics &amp; PCB Viewer</b><br>"
		                      "Offline Linux boardview viewer.<br><br>"
		                      "Boardview parsers adapted from OpenBoardView (MIT)."));
	});
}

bool MainWindow::openBoard(const QString &path)
{
	const LoadResult r = loadBoardFile(path);
	if (!r.ok) {
		statusBar()->showMessage(r.error);
		QMessageBox::warning(this, QStringLiteral("Load failed"), r.error);
		return false;
	}

	m_view->setBoard(r.board);
	m_nets->setBoard(r.board);
	addRecentFile(path);
	setWindowTitle(QStringLiteral("W2R Open — %1").arg(QFileInfo(path).fileName()));
	statusBar()->showMessage(QStringLiteral("Loaded %1  (%2: %3 parts, %4 pins, %5 nets)")
	                             .arg(QFileInfo(path).fileName(), r.board.sourceFormat)
	                             .arg(r.board.parts.size())
	                             .arg(r.board.pins.size())
	                             .arg(r.board.nets.size()));
	m_statusLabel->setText(QStringLiteral("%1 × %2 mm")
	                           .arg(r.board.widthMm(), 0, 'f', 1)
	                           .arg(r.board.heightMm(), 0, 'f', 1));
	return true;
}

void MainWindow::crossProbeNet(const QString &net)
{
	if (net.isEmpty()) return;
	m_view->selectNet(net);
	m_pdf->searchNet(net);
	m_nets->selectNet(net);
	statusBar()->showMessage(QStringLiteral("Cross-probe: %1").arg(net), 5000);
}

bool MainWindow::openSchematic(const QString &path)
{
	if (path.isEmpty() || !QFileInfo::exists(path)) {
		statusBar()->showMessage(QStringLiteral("Schematic not found: %1").arg(path));
		return false;
	}
	m_pdf->loadPdf(path);
	statusBar()->showMessage(QStringLiteral("Schematic: %1").arg(QFileInfo(path).fileName()), 5000);
	return true;
}

void MainWindow::onImportDiode()
{
	if (!m_view->hasBoard()) {
		statusBar()->showMessage(QStringLiteral("Load a board before importing diode readings."));
		return;
	}
	const QString path = QFileDialog::getOpenFileName(
	    this, QStringLiteral("Import Diode Readings"), QString(),
	    QStringLiteral("Diode sheets (*.json *.csv);;All files (*)"));
	if (path.isEmpty()) return;

	QString error;
	const int applied = applyDiodeFile(m_view->mutableBoard(), path, &error);
	if (applied < 0) {
		QMessageBox::warning(this, QStringLiteral("Import failed"), error);
		return;
	}
	m_view->refresh();
	statusBar()->showMessage(
	    QStringLiteral("Applied %1 diode reading(s) from %2").arg(applied).arg(QFileInfo(path).fileName()),
	    6000);
}

void MainWindow::onOpenBoard()
{
	const QString path = QFileDialog::getOpenFileName(
	    this, QStringLiteral("Open Boardview"), QString(),
	    QStringLiteral("Boardview (*.brd *.bvr);;All files (*)"));
	if (!path.isEmpty()) openBoard(path);
}

void MainWindow::onOpenSchematic()
{
	const QString path = QFileDialog::getOpenFileName(
	    this, QStringLiteral("Open Schematic PDF"), QString(),
	    QStringLiteral("PDF (*.pdf);;All files (*)"));
	if (!path.isEmpty()) openSchematic(path);
}

void MainWindow::onPinSelected(int index)
{
	const Board &b = m_view->board();
	if (index < 0 || index >= b.pins.size()) return;
	const Pin &pin = b.pins[index];
	statusBar()->showMessage(QStringLiteral("%1.%2  →  %3")
	                             .arg(pin.part, pin.name, pin.net.isEmpty() ? QStringLiteral("(no net)") : pin.net));
}

void MainWindow::onNetSelected(const QString &net)
{
	if (net.isEmpty()) return;
	statusBar()->showMessage(QStringLiteral("Net: %1").arg(net));
	// Cross-probe: highlight the same net in the schematic.
	m_pdf->searchNet(net);
}

void MainWindow::onPdfNetSearched(const QString &net)
{
	// Cross-probe: highlight the searched net on the PCB.
	m_view->selectNet(net);
}

void MainWindow::addRecentFile(const QString &path)
{
	const QString abs = QFileInfo(path).absoluteFilePath();
	m_recentFiles.removeAll(abs);
	m_recentFiles.prepend(abs);
	while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
	Settings::setRecentFiles(m_recentFiles);
	rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
	if (!m_recentMenu) return;
	m_recentMenu->clear();
	if (m_recentFiles.isEmpty()) {
		QAction *empty = m_recentMenu->addAction(QStringLiteral("(none)"));
		empty->setEnabled(false);
		return;
	}
	int n = 0;
	for (const QString &f : m_recentFiles) {
		QAction *a = m_recentMenu->addAction(QStringLiteral("&%1  %2").arg(++n).arg(QFileInfo(f).fileName()));
		a->setToolTip(f);
		connect(a, &QAction::triggered, this, [this, f] { openBoard(f); });
	}
	m_recentMenu->addSeparator();
	QAction *clear = m_recentMenu->addAction(QStringLiteral("Clear Recent"));
	connect(clear, &QAction::triggered, this, [this] {
		m_recentFiles.clear();
		Settings::setRecentFiles(m_recentFiles);
		rebuildRecentMenu();
	});
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	Settings::setWindowGeometry(saveGeometry());
	Settings::setWindowState(saveState());
	QMainWindow::closeEvent(event);
}

} // namespace w2r
