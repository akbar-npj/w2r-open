// W2R Open — entry point.
//
// Two modes:
//   w2r-open --stats <file>   headless: parse a boardview and print statistics (no display)
//   w2r-open [file]           GUI viewer
#include "core/BoardLoader.h"
#include "core/DiodeSheet.h"
#include "core/Settings.h"
#include "core/Theme.h"
#include "core/library/LibraryIndex.h"
#include "core/rffe/RffeFlasher.h"
#include "core/rffe/RffeModel.h"
#include "core/rffe/RffeProbe.h"
#include "core/rffe/RffeTheme.h"
#include "ui/MainWindow.h"
#include "ui/PcbView.h"
#include "ui/RffePanel.h"
#include "ui/rffe/RffeFlashDialog.h"
#include "ui/rffe/RffePromptDialog.h"
#include "ui/rffe/RffeSettingsDialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QLocale>
#include <QPainter>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include <cstdio>

namespace {

// Loads Qt's own translations plus any w2r-open_<locale>.qm shipped alongside the binary.
// The language chosen in Help -> Language (Settings::language) wins over the system locale.
// Absent translations are not an error — the UI simply stays in its source language.
void installTranslators(QApplication &app)
{
	const QString saved = w2r::Settings::language();
	const QString locale = saved.isEmpty() ? QLocale::system().name() : saved;

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
	const QByteArray animEnv = qgetenv("W2R_ANIM");
	if (!animEnv.isEmpty()) win.setAnimations(QString::fromLocal8Bit(animEnv));

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

// Round-trips a theme through the pcb_themes.json schema and back, verifying that edited
// colours and preserved unknown keys survive serialisation. Used by the test suite.
int runThemeSelftest()
{
	w2r::ThemeManager source;
	if (source.count() < 1) {
		std::fprintf(stderr, "error: no built-in themes loaded\n");
		return 1;
	}

	w2r::Theme t = source.theme(0);
	t.name = QStringLiteral("__selftest__");
	t.pin = QColor(0xff112233);
	t.background = QColor(0xff445566);
	if (!t.layers.isEmpty()) t.layers[0] = QColor(0xff778899);

	// Serialise to a {"themes":[...]} document and reload it through the public loader.
	QJsonObject root;
	root.insert(QStringLiteral("themes"), QJsonArray{t.toJson()});

	QTemporaryFile tmp;
	if (!tmp.open()) {
		std::fprintf(stderr, "error: could not create temp file\n");
		return 1;
	}
	tmp.write(QJsonDocument(root).toJson());
	tmp.flush();

	w2r::ThemeManager reloaded;
	if (!reloaded.loadFromFile(tmp.fileName())) {
		std::fprintf(stderr, "FAIL: could not reload serialised theme\n");
		return 1;
	}

	const w2r::Theme &r = reloaded.theme(0);
	int failures = 0;
	auto check = [&failures](const char *what, bool ok) {
		if (!ok) {
			std::fprintf(stderr, "FAIL: %s\n", what);
			++failures;
		}
	};
	check("name", r.name == QStringLiteral("__selftest__"));
	check("pin", r.pin == QColor(0xff112233));
	check("background", r.background == QColor(0xff445566));
	check("layer1", r.layers.isEmpty() || r.layers[0] == QColor(0xff778899));
	// The original "ratsnet" key spelling must survive.
	check("ratsnet key", t.toJson().contains(QStringLiteral("ratsnet")));

	if (failures == 0) std::printf("theme round-trip: OK\n");
	return failures == 0 ? 0 : 1;
}

// Round-trips the RFFE shields/test-points store through its JSON schema, verifying that
// names, roles, expected devices and the per-user path survive serialisation. Test-suite aid.
int runRffeSelftest()
{
	using namespace w2r::rffe;

	int failures = 0;
	auto check = [&failures](const char *what, bool ok) {
		if (!ok) {
			std::fprintf(stderr, "FAIL: %s\n", what);
			++failures;
		}
	};

	check("role clk", roleFromKey(QStringLiteral("clk")) == Role::Clock);
	check("role v1v8", roleFromKey(QStringLiteral("v1v8")) == Role::V1V8);
	check("role data", roleFromKey(QStringLiteral("data")) == Role::Data);
	check("role key round-trip", roleKey(roleFromKey(QStringLiteral("v1v8"))) == QStringLiteral("v1v8"));

	Store out;
	Shield a;
	a.name = QStringLiteral("SHIELD_RFFE1");
	a.hidden = false;
	a.added = true;
	a.expectedDevices << QStringLiteral("00123") << QStringLiteral("00456");
	TestPoint tp;
	tp.refDes = QStringLiteral("C2406_E");
	tp.net = QStringLiteral("PP_1V8_LDO6");
	tp.role = Role::V1V8;
	a.testPoints << tp;
	TestPoint clk;
	clk.refDes = QStringLiteral("R1201_E");
	clk.net = QStringLiteral("RFFE1_CLK");
	clk.role = Role::Clock;
	a.testPoints << clk;
	out.addShield(a);

	Shield b;
	b.name = QStringLiteral("SHIELD_RFFE2");
	b.hidden = true;
	out.addShield(b);

	Store in;
	QString error;
	if (!in.fromJson(out.toJson(), &error)) {
		std::fprintf(stderr, "FAIL: fromJson: %s\n", error.toUtf8().constData());
		return 1;
	}

	check("shield count", in.shields().size() == 2);
	const Shield *r = nullptr;
	for (const Shield &s : in.shields())
		if (s.name == QStringLiteral("SHIELD_RFFE1")) r = &s;
	check("shield found", r != nullptr);
	if (r) {
		check("shield added", r->added);
		check("shield hidden", !r->hidden);
		check("devices", r->expectedDevices == QStringList({QStringLiteral("00123"), QStringLiteral("00456")}));
		check("tp count", r->testPoints.size() == 2);
		check("tp ref", r->testPoints.value(0).refDes == QStringLiteral("C2406_E"));
		check("tp net", r->testPoints.value(0).net == QStringLiteral("PP_1V8_LDO6"));
		check("tp role", r->testPoints.value(0).role == Role::V1V8);
		check("tp2 role", r->testPoints.value(1).role == Role::Clock);
	}
	check("hidden shield", in.shields().value(1).hidden);
	check("visible tp count", in.visibleTestPointCount() == 2);

	check("path ends with file",
	      Store::path().endsWith(QStringLiteral("/rffe_testpoints.json")));

	// On-disk round-trip into a throwaway XDG data home (AppLocalDataLocation follows it).
	QTemporaryDir tmpDir;
	if (tmpDir.isValid()) {
		qputenv("XDG_DATA_HOME", tmpDir.path().toLocal8Bit());
		const bool saved = out.save();
		check("store saved to disk", saved);
		check("store file exists", QFile::exists(Store::path()));
		Store disk;
		check("store reloaded", disk.load());
		check("disk shield count", disk.shields().size() == 2);
		check("disk marked nets", disk.markedNets().contains(QStringLiteral("PP_1V8_LDO6")));
	} else {
		std::fprintf(stderr, "note: skipped disk round-trip (no temp dir)\n");
	}

	if (failures == 0) std::printf("rffe store round-trip: OK\n");
	return failures == 0 ? 0 : 1;
}

// Round-trips the RFFE UI palette (the original app's RFFE_* colour tokens) through its JSON
// schema, verifies every token resolves, and that the stylesheet builder leaves no "@RFFE_*"
// placeholder unsubstituted. Test-suite aid.
int runRffePaletteSelftest()
{
	using namespace w2r::rffe;

	int failures = 0;
	auto check = [&failures](const QString &what, bool ok) {
		if (!ok) {
			std::fprintf(stderr, "FAIL: %s\n", what.toUtf8().constData());
			++failures;
		}
	};

	const Palette pal = Palette::defaultPalette();

	// The 26 recovered tokens, in the original app's order.
	const char *tokens[] = {
	    "RFFE_BG",             "RFFE_BACKDROP_DOT",  "RFFE_SURFACE",       "RFFE_SURFACE_RAISED",
	    "RFFE_SURFACE_SUNKEN", "RFFE_BORDER",        "RFFE_BORDER_SOFT",   "RFFE_TEXT",
	    "RFFE_TEXT_DIM",       "RFFE_TEXT_FAINT",    "RFFE_BRAND_RED",     "RFFE_ACCENT",
	    "RFFE_ACCENT_STRONG",  "RFFE_ACCENT_INK",    "RFFE_ON_INK",        "RFFE_LED_ON",
	    "RFFE_LED_ON_GLOW",    "RFFE_LED_OFF",       "RFFE_LED_DANGER",    "RFFE_LED_DANGER_GLOW",
	    "RFFE_LED_SCAN",       "RFFE_LED_SCAN_GLOW", "RFFE_FOCUS_RING",    "RFFE_SHADOW_INSET",
	    "RFFE_SHADOW_AMBIENT", "RFFE_SHADOW_CONTACT",
	};
	constexpr int kTokenCount = int(sizeof(tokens) / sizeof(tokens[0]));
	check("26 tokens", kTokenCount == 26);

	int valid = 0;
	for (const char *t : tokens)
		if (pal.color(QLatin1String(t)).isValid()) ++valid;
	check("all tokens resolve", valid == kTokenCount);
	check("unknown token invalid", !pal.color(QStringLiteral("RFFE_NOPE")).isValid());

	// The stylesheet must have every placeholder resolved.
	const QString qss = pal.styleSheet();
	check("stylesheet non-empty", qss.size() > 200);
	check("no @RFFE_ placeholder left", !qss.contains(QStringLiteral("@RFFE_")));
	check("brand red substituted", qss.contains(pal.brandRed.name(QColor::HexRgb)));
	check("surface substituted", qss.contains(pal.surface.name(QColor::HexRgb)));
	check("card selector present", qss.contains(QStringLiteral("RffeCard")));
	// Regression: prefix-colliding tokens must not leave a stray suffix (e.g. "#161a22_SUNKEN").
	for (const char *suffix : {"_RAISED", "_SUNKEN", "_SOFT", "_DIM", "_FAINT", "_STRONG", "_INK"}) {
		const QString s = QString::fromLatin1(suffix);
		check(QStringLiteral("no ") + s + QStringLiteral(" remnant"), !qss.contains(s));
	}

	// JSON round-trip preserves every token colour.
	const Palette rt = Palette::fromJson(pal.toJson());
	bool same = true;
	for (const char *t : tokens)
		if (rt.color(QLatin1String(t)) != pal.color(QLatin1String(t))) same = false;
	check("json round-trip", same);

	// Settings persistence round-trip into a throwaway config home.
	QTemporaryDir cfg;
	if (cfg.isValid()) {
		qputenv("XDG_CONFIG_HOME", cfg.path().toLocal8Bit());
		Palette edited = pal;
		edited.accent = QColor(0xff123456);
		edited.save();
		const Palette loaded = Palette::load();
		check("settings accent", loaded.accent == QColor(0xff123456));
	} else {
		std::fprintf(stderr, "note: skipped settings round-trip (no temp dir)\n");
	}

	// The recovered per-dialog sheets: every %N arg must be substituted, and the shared
	// RffE object names / fonts must be present.
	struct SheetCase {
		const char *name;
		QString sheet;
		const char *mustContain;
	};
	const SheetCase sheets[] = {
	    {"flash", pal.flashDialogStyleSheet(), "QWidget#rffeFlashCard"},
	    {"flash", pal.flashDialogStyleSheet(), "QScrollBar#rffeFlashLogBar"},
	    {"settings", pal.settingsDialogStyleSheet(), "QWidget#rffeSettingsCard"},
	    {"prompt", pal.promptDialogStyleSheet(), "QWidget#rffePromptCard"},
	};
	for (const SheetCase &sc : sheets) {
		check(QStringLiteral("%1 sheet: %2 present").arg(QLatin1String(sc.name), QLatin1String(sc.mustContain)),
		      sc.sheet.contains(QLatin1String(sc.mustContain)));
		check(QStringLiteral("%1 sheet: no %%N left").arg(QLatin1String(sc.name)),
		      !sc.sheet.contains(QLatin1Char('%')));
	}

	if (failures == 0) std::printf("rffe palette round-trip: OK\n");
	return failures == 0 ? 0 : 1;
}

// Constructs the RFFE dialog set + panel offscreen, renders each to a PNG and checks the
// basic interaction invariants. Used for the visual/render verification pass; an optional
// output directory keeps the PNGs (otherwise a throwaway dir is used).
int runRffeDialogSelftest(const QString &outDir)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	qputenv("W2R_NO_SETTINGS", "1");
	QTemporaryDir tmp;
	if (tmp.isValid()) {
		// Isolate the per-user store so the panel starts from a clean slate.
		qputenv("XDG_DATA_HOME", tmp.path().toLocal8Bit());
		qputenv("XDG_CONFIG_HOME", tmp.path().toLocal8Bit());
	}
	static int fakeArgc = 1;
	static char arg0[] = "w2r-open";
	static char *fakeArgv[] = {arg0, nullptr};
	QApplication app(fakeArgc, fakeArgv);

	using namespace w2r;
	using namespace w2r::rffe;

	int failures = 0;
	auto check = [&failures](const char *what, bool ok) {
		if (!ok) {
			std::fprintf(stderr, "FAIL: %s\n", what);
			++failures;
		}
	};

	const Palette pal = Palette::defaultPalette();

	const QString dir = outDir.isEmpty() ? (tmp.isValid() ? tmp.path() : QString()) : outDir;
	if (!dir.isEmpty()) QDir().mkpath(dir);

	// The dialogs and the panel each install their own stylesheet in their constructor.
	auto render = [&](QWidget &w, const QString &name) -> bool {
		w.resize(w.sizeHint().expandedTo(QSize(380, 260)));
		w.show();
		QCoreApplication::processEvents();
		QImage img(w.size(), QImage::Format_ARGB32);
		img.fill(Qt::black);
		QPainter p(&img);
		w.render(&p);
		p.end();
		if (img.isNull()) return false;
		if (dir.isEmpty()) return true;
		const QString path = QDir(dir).filePath(name + QStringLiteral(".png"));
		const bool ok = img.save(path);
		if (ok)
			std::printf("wrote %s (%dx%d)\n", path.toUtf8().constData(), img.width(), img.height());
		return ok;
	};

	// Settings dialog: build, render, verify defaults survive a no-op construction.
	const MarkSettings ms;
	RffeSettingsDialog settings(pal, ms);
	check("settings dialog renders", render(settings, QStringLiteral("rffe-settings")));
	check("settings defaults kept", settings.settings().markBlinkMs == ms.markBlinkMs);

	// Prompt dialog: the initial text round-trips through text().
	RffePromptDialog prompt(pal, QStringLiteral("Rename Shield"),
	                        QStringLiteral("New shield name for %1:").arg(QStringLiteral("SHIELD_RFFE1")),
	                        QStringLiteral("SHIELD_RFFE1"));
	check("prompt initial text", prompt.text() == QStringLiteral("SHIELD_RFFE1"));
	check("prompt dialog renders", render(prompt, QStringLiteral("rffe-prompt")));

	// Flash dialog: builds against a flasher without starting a transfer.
	RffeFlasher flasher;
	RffeFlashDialog flash(pal, &flasher, QStringLiteral("COM3"));
	check("flash dialog renders", render(flash, QStringLiteral("rffe-flash")));

	// Panel: the full card-styled RFFE dock.
	RffePanel panel;
	panel.addTestPoint(QStringLiteral("C2406_E"), QStringLiteral("PP_1V8_LDO6"), int(Role::V1V8));
	check("panel marked nets", panel.markedNets().contains(QStringLiteral("PP_1V8_LDO6")));
	check("panel renders", render(panel, QStringLiteral("rffe-panel")));

	if (failures == 0) std::printf("rffe dialog set: OK\n");
	return failures == 0 ? 0 : 1;
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

	// Theme serialisation round-trip (test-suite aid).
	if (args.contains(QStringLiteral("--theme-selftest"))) return runThemeSelftest();

	// RFFE shields/test-points store round-trip (test-suite aid).
	if (args.contains(QStringLiteral("--rffe-selftest"))) return runRffeSelftest();

	// RFFE UI palette (RFFE_* token) round-trip + stylesheet substitution (test-suite aid).
	if (args.contains(QStringLiteral("--rffe-palette-selftest"))) return runRffePaletteSelftest();

	// RFFE dialog set + panel render verification (test-suite aid). Optional output dir keeps PNGs.
	const int rdsIdx = args.indexOf(QStringLiteral("--rffe-dialog-selftest"));
	if (rdsIdx >= 0) {
		const QString outDir =
		    (rdsIdx + 1 < args.size() && !args[rdsIdx + 1].startsWith(QLatin1Char('-')))
		        ? args[rdsIdx + 1]
		        : QString();
		return runRffeDialogSelftest(outDir);
	}

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
