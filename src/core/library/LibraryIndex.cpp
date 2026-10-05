#include "core/library/LibraryIndex.h"

#include "core/Settings.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace w2r {

namespace {

const QStringList kBoardExts = {QStringLiteral("brd"), QStringLiteral("bvr"), QStringLiteral("cad"),
                                QStringLiteral("bdv"), QStringLiteral("bv"),  QStringLiteral("fz"),
                                QStringLiteral("tvw"), QStringLiteral("cst"), QStringLiteral("asc"),
                                QStringLiteral("pcb"), QStringLiteral("xzz"), QStringLiteral("xzzpcb"),
                                QStringLiteral("w2r"), QStringLiteral("w2s")};

const QStringList kSkipDirs = {QStringLiteral("datasheet")};

QString formatTagFor(const QString &ext)
{
	if (ext == QLatin1String("brd")) return QStringLiteral("BRD");
	if (ext == QLatin1String("bvr")) return QStringLiteral("BVR3");
	return ext.toUpper();
}

QString primaryBoardNumber(const QStringList &numbers)
{
	return numbers.isEmpty() ? QString() : numbers.first();
}

} // namespace

QString LibraryFile::displayName() const
{
	if (isInArchive()) return member.section(QLatin1Char('/'), -1);
	return QFileInfo(path).fileName();
}

QStringList LibraryIndex::defaultRoots()
{
	return Settings::libraryRoots();
}

QStringList LibraryIndex::extractBoardNumbers(const QString &text)
{
	static const QRegularExpression re(
	    QStringLiteral(R"((\d{3}-\d{4,5}(?:-\d{2})?)|(NM-[A-Z0-9]{3,})|(80-[A-Z0-9]{4,})|(\b[A-Z]{1,2}\d{3,4}\b))"),
	    QRegularExpression::CaseInsensitiveOption);

	QStringList out;
	QSet<QString> seen;
	auto it = re.globalMatch(text);
	while (it.hasNext()) {
		const QString m = it.next().captured(0).toUpper();
		if (m.isEmpty() || seen.contains(m)) continue;
		seen.insert(m);
		out << m;
	}
	return out;
}

void LibraryIndex::scan(const std::function<void(int, int, const QString &)> &onProgress)
{
	m_entries.clear();
	loadCache();

	// Count top-level brand folders up front so progress is meaningful.
	int total = 0;
	for (const QString &root : m_roots) {
		QDir r(root);
		for (const QFileInfo &brand : r.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
			if (!kSkipDirs.contains(brand.fileName().toLower())) ++total;
	}
	int done = 0;

	for (const QString &root : m_roots) {
		if (!QDir(root).exists()) continue;
		scanRoot(root, onProgress, done, total);
	}

	saveCache();
}

void LibraryIndex::scanRoot(const QString &root,
                            const std::function<void(int, int, const QString &)> &onProgress,
                            int &done, int &total)
{
	QDir r(root);
	const QFileInfoList brands = r.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QFileInfo &brandInfo : brands) {
		const QString brand = brandInfo.fileName();
		if (kSkipDirs.contains(brand.toLower())) continue;

		// Walk the brand tree. Board files and archives anywhere below are catalogued.
		QDirIterator it(brandInfo.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
		QHash<QString, QStringList> looseByDir; // dir -> file paths
		QStringList archives;
		while (it.hasNext()) {
			const QString path = it.next();
			const QString ext = QFileInfo(path).suffix().toLower();
			if (ArchiveBackend::isArchive(path)) {
				archives << path;
			} else if (kBoardExts.contains(ext) || ext == QLatin1String("pdf")) {
				looseByDir[QFileInfo(path).absolutePath()] << path;
			}
		}

		for (auto dirIt = looseByDir.constBegin(); dirIt != looseByDir.constEnd(); ++dirIt) {
			addLooseFolder(brand, dirIt.key());
		}
		for (const QString &a : archives) addArchive(brand, a);

		++done;
		if (onProgress) onProgress(done, total, brand);
	}
}

void LibraryIndex::addLooseFolder(const QString &brand, const QString &dir)
{
	QDir d(dir);
	const QFileInfoList files = d.entryInfoList(QDir::Files);

	// Group files by primary board number so a board + its schematic(s) land in one entry.
	struct Group {
		QVector<LibraryFile> boards;
		QVector<LibraryFile> schematics;
		QStringList numbers;
	};
	QHash<QString, Group> groups;
	QStringList order;

	auto groupKeyFor = [](const QString &fileName, const QStringList &numbers, const QString &ext) {
		if (ext == QLatin1String("asc")) return QStringLiteral("@asc"); // split Allegro set
		const QString n = primaryBoardNumber(numbers);
		if (!n.isEmpty()) return n;
		return QStringLiteral("@file:") + fileName;
	};

	for (const QFileInfo &fi : files) {
		const QString ext = fi.suffix().toLower();
		const bool isPdf = (ext == QLatin1String("pdf"));
		if (!isPdf && !kBoardExts.contains(ext)) continue;

		const QStringList numbers = extractBoardNumbers(fi.fileName());
		const QString key = groupKeyFor(fi.fileName(), numbers, ext);

		LibraryFile lf;
		lf.path = fi.absoluteFilePath();
		lf.size = fi.size();
		lf.isSchematic = isPdf;
		lf.format = isPdf ? QStringLiteral("PDF") : formatTagFor(ext);

		Group &g = groups[key];
		if (g.boards.isEmpty() && g.schematics.isEmpty()) order << key;
		if (isPdf)
			g.schematics << lf;
		else
			g.boards << lf;
		for (const QString &n : numbers)
			if (!g.numbers.contains(n)) g.numbers << n;
	}

	for (const QString &key : order) {
		const Group &g = groups[key];
		if (g.boards.isEmpty() && g.schematics.isEmpty()) continue;

		LibraryEntry e;
		e.brand = brand;
		e.folder = dir;
		e.boards = g.boards;
		e.schematics = g.schematics;
		e.boardNumbers = g.numbers;

		// Name: prefer the first board file's base name, else the folder name.
		if (!g.boards.isEmpty())
			e.name = QFileInfo(g.boards.first().path).completeBaseName();
		else if (!g.schematics.isEmpty())
			e.name = QFileInfo(g.schematics.first().path).completeBaseName();
		else
			e.name = QFileInfo(dir).fileName();

		e.searchText = (brand + QLatin1Char(' ') + e.name + QLatin1Char(' ') + dir + QLatin1Char(' ') +
		                e.boardNumbers.join(QLatin1Char(' ')))
		                   .toLower();
		m_entries.append(e);
	}
}

void LibraryIndex::addArchive(const QString &brand, const QString &archivePath)
{
	const QVector<ArchiveMember> members = listArchiveCached(archivePath);
	const QFileInfo afi(archivePath);

	LibraryEntry e;
	e.brand = brand;
	e.folder = archivePath;
	e.name = afi.completeBaseName();
	e.boardNumbers = extractBoardNumbers(afi.fileName());

	for (const ArchiveMember &m : members) {
		const QString ext = QFileInfo(m.path).suffix().toLower();
		LibraryFile lf;
		lf.path = archivePath;
		lf.member = m.path;
		lf.size = m.size;
		if (ext == QLatin1String("pdf")) {
			lf.isSchematic = true;
			lf.format = QStringLiteral("PDF");
			e.schematics << lf;
		} else if (kBoardExts.contains(ext)) {
			lf.format = formatTagFor(ext);
			e.boards << lf;
		}
		for (const QString &n : extractBoardNumbers(m.path))
			if (!e.boardNumbers.contains(n)) e.boardNumbers << n;
	}

	// Skip archives with nothing we can open.
	if (e.boards.isEmpty() && e.schematics.isEmpty()) return;

	e.searchText = (brand + QLatin1Char(' ') + e.name + QLatin1Char(' ') + archivePath + QLatin1Char(' ') +
	                e.boardNumbers.join(QLatin1Char(' ')))
	                   .toLower();
	m_entries.append(e);
}

QVector<ArchiveMember> LibraryIndex::listArchiveCached(const QString &archivePath)
{
	const QFileInfo fi(archivePath);
	const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();

	auto it = m_archiveCache.find(archivePath);
	if (it != m_archiveCache.end() && it->mtime == mtime) return it->members;

	CacheEntry ce;
	ce.mtime = mtime;
	ce.members = ArchiveBackend::list(archivePath);
	m_archiveCache.insert(archivePath, ce);
	m_cacheDirty = true;
	return ce.members;
}

QVector<int> LibraryIndex::search(const QString &query) const
{
	QVector<int> hits;
	const QString q = query.trimmed().toLower();
	if (q.isEmpty()) {
		hits.reserve(m_entries.size());
		for (int i = 0; i < m_entries.size(); ++i) hits << i;
		return hits;
	}
	const QStringList tokens = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	for (int i = 0; i < m_entries.size(); ++i) {
		bool all = true;
		for (const QString &t : tokens) {
			if (!m_entries[i].searchText.contains(t)) {
				all = false;
				break;
			}
		}
		if (all) hits << i;
	}
	return hits;
}

QString LibraryIndex::materialize(const LibraryFile &file, QString *error)
{
	if (!file.isInArchive()) {
		if (QFileInfo::exists(file.path)) return file.path;
		if (error) *error = QStringLiteral("file not found: %1").arg(file.path);
		return QString();
	}

	const QString destDir = ArchiveBackend::cacheDirFor(file.path);
	const QString destFile = QDir(destDir).filePath(file.member);
	if (QFileInfo::exists(destFile)) return destFile;

	if (!ArchiveBackend::extract(file.path, {file.member}, destDir)) {
		if (error)
			*error = QStringLiteral("failed to extract %1 from %2 (is 'unar' installed?)")
			             .arg(file.member, QFileInfo(file.path).fileName());
		return QString();
	}
	if (!QFileInfo::exists(destFile)) {
		if (error) *error = QStringLiteral("extracted file missing: %1").arg(destFile);
		return QString();
	}
	return destFile;
}

void LibraryIndex::loadCache()
{
	m_archiveCache.clear();
	m_cacheDirty = false;

	const QString base = ArchiveBackend::cacheRoot();
	const QString path = QDir(base).filePath(QStringLiteral("archive-index.json"));
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return;

	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isObject()) return;
	const QJsonObject root = doc.object();
	for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
		const QJsonObject o = it.value().toObject();
		CacheEntry ce;
		ce.mtime = static_cast<qint64>(o.value(QStringLiteral("mtime")).toDouble());
		const QJsonArray arr = o.value(QStringLiteral("members")).toArray();
		for (const QJsonValue &v : arr) {
			const QJsonArray pair = v.toArray();
			if (pair.size() < 1) continue;
			ArchiveMember m;
			m.path = pair.at(0).toString();
			m.size = pair.size() > 1 ? static_cast<qint64>(pair.at(1).toDouble()) : 0;
			ce.members << m;
		}
		m_archiveCache.insert(it.key(), ce);
	}
}

void LibraryIndex::saveCache() const
{
	if (!m_cacheDirty) return;

	QJsonObject root;
	for (auto it = m_archiveCache.constBegin(); it != m_archiveCache.constEnd(); ++it) {
		QJsonObject o;
		o.insert(QStringLiteral("mtime"), static_cast<double>(it->mtime));
		QJsonArray arr;
		for (const ArchiveMember &m : it->members) {
			QJsonArray pair;
			pair.append(m.path);
			pair.append(static_cast<double>(m.size));
			arr.append(pair);
		}
		o.insert(QStringLiteral("members"), arr);
		root.insert(it.key(), o);
	}

	const QString base = ArchiveBackend::cacheRoot();
	QDir().mkpath(base);
	QFile f(QDir(base).filePath(QStringLiteral("archive-index.json")));
	if (!f.open(QIODevice::WriteOnly)) return;
	f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace w2r
