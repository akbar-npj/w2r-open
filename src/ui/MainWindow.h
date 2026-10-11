// Main application window: toolbar + PCB canvas + status bar.
// Phase 1 scope; library, PDF and cross-probing docks are added in later phases.
#pragma once

#include "core/Theme.h"
#include "core/library/LibraryIndex.h"

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QComboBox;
class QAction;
class QMenu;
class QDockWidget;
class QSplitter;
class QCloseEvent;
class QTranslator;
template <typename T>
class QFutureWatcher;

namespace w2r {

class PcbView;
class PdfView;
class LibraryPanel;
class NetInspector;
class RffePanel;
class OccurrencesPanel;

class MainWindow : public QMainWindow {
	Q_OBJECT
  public:
	explicit MainWindow(QWidget *parent = nullptr);

	// Loads a board file and reports the outcome in the status bar.
	bool openBoard(const QString &path);
	// Loads a schematic PDF into the right pane.
	bool openSchematic(const QString &path);

	// Cross-probe a net across both panes (PCB highlight + schematic search).
	void crossProbeNet(const QString &net);

	// Applies a PCB theme by name (used by the CLI/test harness).
	void setThemeByName(const QString &name);

	// Shows or hides the RFFE probe dock (used by the CLI/test harness).
	void setRffeVisible(bool on);

	// Enables animation effects by name (comma-separated: spotlight, netPulse, traceReveal);
	// used by the CLI/test harness.
	void setAnimations(const QString &spec);

  private slots:
	void onOpenBoard();
	void onOpenSchematic();
	void onPinSelected(int index);
	void onNetSelected(const QString &net);
	void onPdfNetSearched(const QString &net);
	void onLibraryOpen(const LibraryEntry &entry);
	void onImportDiode();
	void onThemeChanged(int index);
	void onThemeCreate();
	void onThemeEdit();
	void onThemeDelete();
	void onNetInspectorPin(const QString &part, const QString &pin);
	void onRffeDevice(const QString &id);
	void onPreferences();
	void toggleFullScreen();
	void swapPanes();
	void onTreeFontIncrease();
	void onTreeFontDecrease();
	void onTreeFontReset();
	void onLanguageSelected(const QString &code);

  protected:
	void closeEvent(QCloseEvent *event) override;

  private:
	void buildToolbar();
	void buildMenus();
	void buildLibraryDock();
	void buildNetDock();
	void buildRffeDock();
	void buildOccurrencesDock();
	void buildCentralViews();
	void setViewMode(const QString &mode);
	void applyTreeFont(int pointSize);
	void startLibraryScan();
	void addRecentFile(const QString &path);
	void rebuildRecentMenu();
	void rebuildThemeCombo();
	void retranslateMenus();

	ThemeManager m_themes;
	QComboBox *m_themeCombo = nullptr;
	QAction *m_diodeAction = nullptr;
	QAction *m_ratsnestAction = nullptr;
	QAction *m_rffeAction = nullptr;
	QAction *m_animMasterAction = nullptr;
	QAction *m_spotlightAction = nullptr;
	QAction *m_netPulseAction = nullptr;
	QAction *m_traceRevealAction = nullptr;
	PcbView *m_view = nullptr;
	PdfView *m_pdf = nullptr;
	PdfView *m_pdf2 = nullptr;
	QSplitter *m_splitter = nullptr;
	LibraryPanel *m_library = nullptr;
	NetInspector *m_nets = nullptr;
	RffePanel *m_rffe = nullptr;
	OccurrencesPanel *m_occ = nullptr;
	QDockWidget *m_rffeDock = nullptr;
	QLabel *m_statusLabel = nullptr;
	QFutureWatcher<QVector<LibraryEntry>> *m_scanWatcher = nullptr;
	QMenu *m_recentMenu = nullptr;
	QMenu *m_fileMenu = nullptr;
	QMenu *m_viewMenu = nullptr;
	QMenu *m_themeMenu = nullptr;
	QMenu *m_helpMenu = nullptr;
	QMenu *m_languageMenu = nullptr;
	QTranslator *m_translator = nullptr;
	QStringList m_recentFiles;
	QString m_viewMode = QStringLiteral("both");
	bool m_swapped = false;
	int m_treeFontPointSize = 0;
};

} // namespace w2r
