// W2R Open — entry point.
//
// Two modes:
//   w2r-open --stats <file>   headless: parse a boardview and print statistics (no display)
//   w2r-open [file]           GUI viewer
#include "core/BoardLoader.h"
#include "core/DiodeSheet.h"
#include "core/library/LibraryIndex.h"
#include "core/rffe/RffeFlasher.h"
#include "core/rffe/RffeProbe.h"
#include "ui/MainWindow.h"
#include "ui/PcbView.h"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QLibraryInfo>
#include <QLocale>
#include <QPainter>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include <cstdio>

namespace {

// Loads Qt's own translations plus any w2r-open_<locale>.qm shipped alongside the binary.
// Absent translations are not an error — the UI simply stays in its source language.
void installTranslators(QApplication &app)
{
	const QString locale = QLocale::system().name();

	auto *qtTr = new QTranslator(&app);
	if (qtTr->load(QStringLiteral("qt_") + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
		app.installTranslator(qtTr);
	else
		delete qtTr;

	auto *appTr = new QTranslator(&app);
	const QString name = QStringLiteral("w2r-open_") + locale;
	const QStringList dirs = {
	    QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
	    QCoreApplication::applicationDirPath() + QStringLiteral("/../share/w2r-open/translations"),
	    QCoreApplication::applicationDirPath(),
	};
	bool loaded = false;
	for (const QString &dir : dirs) {
		if (appTr->load(name, dir)) {
			loaded = true;
			break;
		}
	}
	if (loaded)
		app.installTranslator(appTr);
	else
		delete appTr;
}

int runStats(const QString &path)
{
	const w2r::LoadResult r = w2r::loadBoardFile(path);
	if (!r.ok) {
		std::fprintf(stderr, "error: %s\n", r.error.toUtf8().constData());
		return 1;
	}
	const w2r::Board &b = r.board;
	std::printf("file      : %s\n", path.toUtf8().constData());
	std::printf("format    : %s\n", b.sourceFormat.toUtf8().constData());
	std::printf("parts     : %d\n", b.parts.size());
	std::printf("pins      : %d\n", b.pins.size());
	std::printf("nets      : %d\n", b.nets.size());
	std::printf("outline   : %d points\n", b.outline.size());
	std::printf("bounds    : x=[%.0f..%.0f] y=[%.0f..%.0f] mils  (%.1f x %.1f mm)\n",
	            b.bounds.left(), b.bounds.right(), b.bounds.top(), b.bounds.bottom(),
	            b.widthMm(), b.heightMm());

	int ground = 0, nc = 0, named = 0;
	for (const w2r::Pin &p : b.pins) {
		if (p.isGround()) ++ground;
		if (p.isNC()) ++nc;
		if (!p.net.isEmpty()) ++named;
	}
	std::printf("pins net  : %d named, %d ground, %d NC\n", named, ground, nc);

	const int sample = std::min(5, static_cast<int>(b.parts.size()));
	for (int i = 0; i < sample; ++i) {
		const w2r::Part &p = b.parts[i];
		std::printf("  part[%d] %-10s side=%s mount=%-3s pins=%d\n", i, p.name.toUtf8().constData(),
		            p.layer == 0 ? "T" : "B", p.mount.toUtf8().constData(), p.pins.size());
	}
	return 0;
}

// Headless render: w2r-open --render <file> <out.png> [width height]
int runRender(const QString &path, const QString &outPath, int w, int h)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	qputenv("W2R_NO_SETTINGS", "1");
	static int fakeArgc = 1;
	static char arg0[] = "w2r-open";
	static char *fakeArgv[] = {arg0, nullptr};
	QApplication app(fakeArgc, fakeArgv);

	w2r::PcbView view;
	view.resize(w, h);

	w2r::LoadResult r = w2r::loadBoardFile(path);
	if (!r.ok) {
		std::fprintf(stderr, "error: %s\n", r.error.toUtf8().constData());
		return 1;
	}
	// Optional: overlay a diode sheet for validation (W2R_DIODE=<sheet.json|csv>).
	const QByteArray diodeEnv = qgetenv("W2R_DIODE");
	if (!diodeEnv.isEmpty()) {
		QString derr;
		const int applied = w2r::applyDiodeFile(r.board, QString::fromLocal8Bit(diodeEnv), &derr);
		if (applied < 0) std::fprintf(stderr, "diode: %s\n", derr.toUtf8().constData());
		else std::fprintf(stderr, "diode: applied %d reading(s)\n", applied);
		view.setShowDiodeReadings(true);
	}
	view.setBoard(r.board);
	view.fitToBoard();
	view.resize(w, h);
	view.fitToBoard();

	QImage img(w, h, QImage::Format_ARGB32);
	img.fill(Qt::black);
	QPainter p(&img);
	view.render(&p);
	p.end();

	if (!img.save(outPath)) {
		std::fprintf(stderr, "error: could not write %s\n", outPath.toUtf8().constData());
		return 1;
	}
	std::printf("wrote %s (%dx%d)\n", outPath.toUtf8().constData(), w, h);
	return 0;
}

// Headless library index: w2r-open --library [query]
int runLibrary(const QString &query)
{
	w2r::LibraryIndex idx;
	idx.setRoots(w2r::LibraryIndex::defaultRoots());
	if (idx.roots().isEmpty()) {
		std::fprintf(stderr, "no library roots configured (set W2R_LIBRARY_ROOTS)\n");
		return 1;
	}

	const bool tty = true;
	idx.scan([tty](int done, int total, const QString &label) {
		if (!tty || total <= 0) return;
		if (done == total || done % 20 == 0)
			std::fprintf(stderr, "\rindexing %d/%d  %-24s", done, total, label.toUtf8().constData());
	});
	std::fprintf(stderr, "\r%*s\r", 60, "");

	const auto &entries = idx.entries();
	int withBoard = 0, withPdf = 0, fromArchive = 0;
	for (const auto &e : entries) {
		if (e.hasBoard()) ++withBoard;
		if (e.hasSchematic()) ++withPdf;
		for (const auto &b : e.boards)
			if (b.isInArchive()) {
				++fromArchive;
				break;
			}
	}

	std::printf("roots      : %s\n", idx.roots().join(QStringLiteral(", ")).toUtf8().constData());
	std::printf("entries    : %d  (%d with boardview, %d with schematic, %d from archives)\n", entries.size(),
	            withBoard, withPdf, fromArchive);

	QString archDetail;
	const bool tools = w2r::ArchiveBackend::toolsAvailable(&archDetail);
	std::printf("archives   : %s (%s)\n", tools ? "enabled" : "DISABLED", archDetail.toUtf8().constData());

	const auto hits = idx.search(query);
	std::printf("search '%s': %d hit(s)\n", query.toUtf8().constData(), hits.size());
	const int show = std::min<int>(25, hits.size());
	for (int i = 0; i < show; ++i) {
		const auto &e = entries[hits[i]];
		std::printf("  %-8s %-46s b=%d s=%d  %s\n", e.brand.toUtf8().constData(),
		            e.name.left(46).toUtf8().constData(), e.boards.size(), e.schematics.size(),
		            e.boardNumbers.join(QLatin1Char(',')).toUtf8().constData());
	}
	if (hits.size() > show) std::printf("  ... and %d more\n", hits.size() - show);
	return 0;
}

// Resolve a search query to a loadable board: w2r-open --resolve <query>
int runResolve(const QString &query)
{
	w2r::LibraryIndex idx;
	idx.setRoots(w2r::LibraryIndex::defaultRoots());
	idx.scan();

	const auto hits = idx.search(query);
	const auto &entries = idx.entries();
	for (int i : hits) {
		const w2r::LibraryEntry &e = entries[i];
		if (!e.hasBoard()) continue;

		const w2r::LibraryFile &b = e.boards.first();
		QString error;
		const QString path = w2r::LibraryIndex::materialize(b, &error);
		std::printf("entry : %s / %s  [%s]\n", e.brand.toUtf8().constData(), e.name.toUtf8().constData(),
		            e.boardNumbers.join(QLatin1Char(',')).toUtf8().constData());
		std::printf("board : %s\n", b.displayName().toUtf8().constData());
		if (path.isEmpty()) {
			std::printf("error : %s\n", error.toUtf8().constData());
			return 1;
		}
		std::printf("local : %s\n", path.toUtf8().constData());

		const w2r::LoadResult r = w2r::loadBoardFile(path);
		if (!r.ok) {
			std::printf("parse : FAILED (%s)\n", r.error.toUtf8().constData());
			return 1;
		}
		std::printf("parse : OK  %s  parts=%d pins=%d nets=%d  %.1f x %.1f mm\n",
		            r.board.sourceFormat.toUtf8().constData(), r.board.parts.size(), r.board.pins.size(),
		            r.board.nets.size(), r.board.widthMm(), r.board.heightMm());
		return 0;
	}
	std::printf("no matching entry with a boardview for '%s'\n", query.toUtf8().constData());
	return 1;
}

// Renders the whole main window (incl. library dock) to a PNG after the index settles.
// Used for visual checks: w2r-open --render-window <out.png> [width height] [delayMs]
int runRenderWindow(const QString &outPath, int w, int h, int delayMs)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	qputenv("W2R_NO_SETTINGS", "1");
	static int fakeArgc = 1;
	static char arg0[] = "w2r-open";
	static char *fakeArgv[] = {arg0, nullptr};
	QApplication app(fakeArgc, fakeArgv);

	w2r::MainWindow win;
	win.resize(w, h);
	win.show();

	// Optional preload for validation: W2R_BOARD / W2R_PDF env vars.
	const QByteArray boardEnv = qgetenv("W2R_BOARD");
	if (!boardEnv.isEmpty()) win.openBoard(QString::fromLocal8Bit(boardEnv));
	const QByteArray pdfEnv = qgetenv("W2R_PDF");
	if (!pdfEnv.isEmpty()) win.openSchematic(QString::fromLocal8Bit(pdfEnv));
	const QByteArray netEnv = qgetenv("W2R_NET");
	if (!netEnv.isEmpty()) win.crossProbeNet(QString::fromLocal8Bit(netEnv));
	const QByteArray themeEnv = qgetenv("W2R_THEME");
	if (!themeEnv.isEmpty()) win.setThemeByName(QString::fromLocal8Bit(themeEnv));
	if (!qgetenv("W2R_RFFE").isEmpty()) win.setRffeVisible(true);

	QTimer::singleShot(delayMs, [&] {
		QImage img(w, h, QImage::Format_ARGB32);
		img.fill(Qt::black);
		QPainter p(&img);
		win.render(&p);
		p.end();
		if (img.save(outPath))
			std::printf("wrote %s (%dx%d)\n", outPath.toUtf8().constData(), w, h);
		else
			std::fprintf(stderr, "error: could not write %s\n", outPath.toUtf8().constData());
		QCoreApplication::quit();
	});

	return app.exec();
}

// Applies a diode sheet to a board and reports how many pins matched.
int runDiode(const QString &boardPath, const QString &sheetPath)
{
	w2r::LoadResult r = w2r::loadBoardFile(boardPath);
	if (!r.ok) {
		std::fprintf(stderr, "error: %s\n", r.error.toUtf8().constData());
		return 1;
	}
	QString error;
	const int applied = w2r::applyDiodeFile(r.board, sheetPath, &error);
	if (applied < 0) {
		std::fprintf(stderr, "error: %s\n", error.toUtf8().constData());
		return 1;
	}
	std::printf("board  : %s (%d pins)\n", boardPath.toUtf8().constData(), r.board.pins.size());
	std::printf("sheet  : %s\n", sheetPath.toUtf8().constData());
	std::printf("applied: %d reading(s)\n", applied);
	int shown = 0;
	for (const w2r::Pin &p : r.board.pins) {
		if (p.diode.isEmpty()) continue;
		std::printf("  %s.%s = %s\n", p.part.toUtf8().constData(), p.name.toUtf8().constData(),
		            p.diode.toUtf8().constData());
		if (++shown >= 8) break;
	}
	return applied > 0 ? 0 : 1;
}

// Reports RFFE probe environment: serial support, ports, bundled firmware and esptool.
int runRffeInfo(int argc, char **argv)
{
	// applicationDirPath() needs an application instance to resolve the executable's directory.
	QCoreApplication app(argc, argv);

	std::printf("serial support : %s\n", w2r::RffeProbe::serialSupported() ? "yes" : "no");
	std::printf("scan command   : %s\n", w2r::RffeProbe::scanCommand());

	const QString fw = w2r::RffeFlasher::bundledFirmwarePath();
	std::printf("firmware       : %s\n", fw.isEmpty() ? "(not found)" : fw.toUtf8().constData());
	std::printf("flash offset   : %s\n", w2r::RffeFlasher::flashOffset().toUtf8().constData());

	QString tool = w2r::RffeFlasher::esptoolProgram();
	std::printf("esptool        : %s\n", tool.isEmpty() ? "(not found)" : tool.toUtf8().constData());

	const QVector<w2r::RffePort> ports = w2r::RffeProbe::availablePorts();
	std::printf("ports          : %d\n", ports.size());
	for (const w2r::RffePort &p : ports) {
		std::printf("  %-16s %s", p.name.toUtf8().constData(), p.description.toUtf8().constData());
		if (p.isUsb) std::printf("  [VID_%04x PID_%04x]", p.vid, p.pid);
		std::printf("\n");
	}
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	QStringList args;
	args.reserve(argc);
	for (int i = 1; i < argc; ++i)
		args << QString::fromLocal8Bit(argv[i]);

	// Headless stats mode — no QApplication / display required.
	const int statsIdx = args.indexOf(QStringLiteral("--stats"));
	if (statsIdx >= 0) {
		if (statsIdx + 1 >= args.size()) {
			std::fprintf(stderr, "usage: w2r-open --stats <boardview file>\n");
			return 2;
		}
		return runStats(args[statsIdx + 1]);
	}

	// Headless render mode — used for visual checks and the golden-image harness.
	const int renderIdx = args.indexOf(QStringLiteral("--render"));
	if (renderIdx >= 0) {
		if (renderIdx + 2 >= args.size()) {
			std::fprintf(stderr, "usage: w2r-open --render <boardview file> <out.png> [width height]\n");
			return 2;
		}
		const int w = (renderIdx + 3 < args.size()) ? args[renderIdx + 3].toInt() : 1400;
		const int h = (renderIdx + 4 < args.size()) ? args[renderIdx + 4].toInt() : 900;
		return runRender(args[renderIdx + 1], args[renderIdx + 2], w > 0 ? w : 1400, h > 0 ? h : 900);
	}

	// Render the whole main window (library dock included).
	const int rwIdx = args.indexOf(QStringLiteral("--render-window"));
	if (rwIdx >= 0) {
		if (rwIdx + 1 >= args.size()) {
			std::fprintf(stderr, "usage: w2r-open --render-window <out.png> [width height delayMs]\n");
			return 2;
		}
		const int w = (rwIdx + 2 < args.size()) ? args[rwIdx + 2].toInt() : 1400;
		const int h = (rwIdx + 3 < args.size()) ? args[rwIdx + 3].toInt() : 900;
		const int d = (rwIdx + 4 < args.size()) ? args[rwIdx + 4].toInt() : 3000;
		return runRenderWindow(args[rwIdx + 1], w > 0 ? w : 1400, h > 0 ? h : 900, d > 0 ? d : 3000);
	}

	// Headless library index mode.
	const int libIdx = args.indexOf(QStringLiteral("--library"));
	if (libIdx >= 0) {
		const QString q = (libIdx + 1 < args.size() && !args[libIdx + 1].startsWith(QLatin1Char('-')))
		                      ? args[libIdx + 1]
		                      : QString();
		return runLibrary(q);
	}

	// Resolve a library query to a loadable board (match -> extract -> parse).
	const int resolveIdx = args.indexOf(QStringLiteral("--resolve"));
	if (resolveIdx >= 0) {
		if (resolveIdx + 1 >= args.size()) {
			std::fprintf(stderr, "usage: w2r-open --resolve <query>\n");
			return 2;
		}
		return runResolve(args[resolveIdx + 1]);
	}

	// Apply a diode sheet: w2r-open --diode <board> <sheet.json|csv>
	const int diodeIdx = args.indexOf(QStringLiteral("--diode"));
	if (diodeIdx >= 0) {
		if (diodeIdx + 2 >= args.size()) {
			std::fprintf(stderr, "usage: w2r-open --diode <boardview> <sheet.json|csv>\n");
			return 2;
		}
		return runDiode(args[diodeIdx + 1], args[diodeIdx + 2]);
	}

	// Report RFFE probe environment.
	if (args.contains(QStringLiteral("--rffe-info"))) return runRffeInfo(argc, argv);

	QApplication app(argc, argv);
	app.setApplicationName(QStringLiteral("w2r-open"));
	app.setApplicationDisplayName(QStringLiteral("W2R Open Schematics & PCB Viewer"));
	app.setOrganizationName(QStringLiteral("W2R Open"));
	installTranslators(app);

	w2r::MainWindow win;
	win.show();

	// Load positional files: first = board, second = schematic PDF.
	QStringList positional;
	for (const QString &a : args) {
		if (!a.startsWith(QLatin1Char('-'))) positional << a;
	}
	if (!positional.isEmpty()) win.openBoard(positional.at(0));
	if (positional.size() > 1) win.openSchematic(positional.at(1));

	return app.exec();
}
