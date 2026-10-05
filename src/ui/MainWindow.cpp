#include "ui/MainWindow.h"

#include "core/BoardLoader.h"
#include "core/DiodeSheet.h"
#include "core/Settings.h"
#include "ui/LibraryPanel.h"
#include "ui/NetInspector.h"
#include "ui/PcbView.h"
#include "ui/PdfView.h"
#include "ui/PreferencesDialog.h"
#include "ui/RffePanel.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
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

	m_statusLabel = new QLabel(this);
	statusBar()->addPermanentWidget(m_statusLabel);
	statusBar()->showMessage(QStringLiteral("Ready. Open a .brd or .bvr boardview."));

	connect(m_view, &PcbView::pinSelected, this, &MainWindow::onPinSelected);
	connect(m_view, &PcbView::netSelected, this, &MainWindow::onNetSelected);
	connect(m_pdf, &PdfView::netSearched, this, &MainWindow::onPdfNetSearched);

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

	startLibraryScan();
}

void MainWindow::buildCentralViews()
{
	m_splitter = new QSplitter(Qt::Horizontal, this);
	m_view = new PcbView(m_splitter);
	m_pdf = new PdfView(m_splitter);
	m_splitter->addWidget(m_view);
	m_splitter->addWidget(m_pdf);
	m_splitter->setStretchFactor(0, 1);
	m_splitter->setStretchFactor(1, 1);
	setCentralWidget(m_splitter);
}

void MainWindow::setViewMode(const QString &mode)
{
	if (mode == QLatin1String("pcb")) {
		m_view->show();
		m_pdf->hide();
	} else if (mode == QLatin1String("pdf")) {
		m_view->hide();
		m_pdf->show();
	} else {
		m_view->show();
		m_pdf->show();
		const int half = qMax(200, m_splitter->width() / 2);
		m_splitter->setSizes({half, half});
	}
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
	QAction *both = tb->addAction(QStringLiteral("Both"));
	both->setShortcut(QKeySequence(QStringLiteral("Ctrl+3")));
	connect(both, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("both")); });
	QAction *pcbOnly = tb->addAction(QStringLiteral("PCB Only"));
	pcbOnly->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
	connect(pcbOnly, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pcb")); });
	QAction *pdfOnly = tb->addAction(QStringLiteral("PDF Only"));
	pdfOnly->setShortcut(QKeySequence(QStringLiteral("Ctrl+2")));
	connect(pdfOnly, &QAction::triggered, this, [this] { setViewMode(QStringLiteral("pdf")); });

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

void MainWindow::setRffeVisible(bool on)
{
	if (m_rffeAction) m_rffeAction->setChecked(on);
	if (m_rffeDock) {
		m_rffeDock->setVisible(on);
		if (on) m_rffeDock->raise();
	}
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

void MainWindow::buildMenus()
{
	QMenu *file = menuBar()->addMenu(QStringLiteral("&File"));
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

	QMenu *help = menuBar()->addMenu(QStringLiteral("&Help"));
	QAction *about = help->addAction(QStringLiteral("&About"));
	connect(about, &QAction::triggered, this, [this] {
		QMessageBox::about(this, QStringLiteral("About W2R Open"),
		                   QStringLiteral("<b>W2R Open Schematics &amp; PCB Viewer</b><br>"
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
