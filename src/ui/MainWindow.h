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
template <typename T>
class QFutureWatcher;

namespace w2r {

class PcbView;
class PdfView;
class LibraryPanel;
class NetInspector;
class RffePanel;

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

  private slots:
	void onOpenBoard();
	void onOpenSchematic();
	void onPinSelected(int index);
	void onNetSelected(const QString &net);
	void onPdfNetSearched(const QString &net);
	void onLibraryOpen(const LibraryEntry &entry);
	void onImportDiode();
	void onThemeChanged(int index);
	void onNetInspectorPin(const QString &part, const QString &pin);
	void onRffeDevice(const QString &id);
	void onPreferences();
	void toggleFullScreen();

  protected:
	void closeEvent(QCloseEvent *event) override;

  private:
	void buildToolbar();
	void buildMenus();
	void buildLibraryDock();
	void buildNetDock();
	void buildRffeDock();
	void buildCentralViews();
	void setViewMode(const QString &mode);
	void startLibraryScan();
	void addRecentFile(const QString &path);
	void rebuildRecentMenu();

	ThemeManager m_themes;
	QComboBox *m_themeCombo = nullptr;
	QAction *m_diodeAction = nullptr;
	QAction *m_ratsnestAction = nullptr;
	QAction *m_rffeAction = nullptr;
	PcbView *m_view = nullptr;
	PdfView *m_pdf = nullptr;
	QSplitter *m_splitter = nullptr;
	LibraryPanel *m_library = nullptr;
	NetInspector *m_nets = nullptr;
	RffePanel *m_rffe = nullptr;
	QDockWidget *m_rffeDock = nullptr;
	QLabel *m_statusLabel = nullptr;
	QFutureWatcher<QVector<LibraryEntry>> *m_scanWatcher = nullptr;
	QMenu *m_recentMenu = nullptr;
	QStringList m_recentFiles;
};

} // namespace w2r
