// Local library index: discovers boardviews and schematics under one or more roots
// (default: ~/Projects/Schematics), including files inside .rar/.zip/.7z
// archives. Archive member listings are cached on disk so rescans are cheap.
#pragma once

#include "core/library/ArchiveBackend.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace w2r {

// A boardview or schematic, either a loose file or a member inside an archive.
struct LibraryFile {
	QString path;   // loose file path, or the archive path
	QString member; // member path inside the archive (empty when loose)
	QString format; // "BRD", "BVR3", or an uppercase extension
	qint64 size = 0;
	bool isSchematic = false;

	bool isInArchive() const { return !member.isEmpty(); }
	QString displayName() const;
};

// One openable device/package: a set of boardview(s) plus schematic(s).
struct LibraryEntry {
	QString brand;  // top-level folder under a root (e.g. "Apple")
	QString name;   // display name (board or archive name)
	QString folder; // containing folder or archive path
	QVector<LibraryFile> boards;
	QVector<LibraryFile> schematics;
	QStringList boardNumbers; // normalised identifiers (820-02443, A2442, NM-B661, ...)
	QString searchText;       // lowercased haystack for filtering

	bool hasBoard() const { return !boards.isEmpty(); }
	bool hasSchematic() const { return !schematics.isEmpty(); }
};

class LibraryIndex {
  public:
	// Scans all roots. Cache-aware: archive listings are reused when the archive mtime matches.
	// `onProgress(done, total, label)` may be null; scan() is blocking, call it from a worker thread.
	void scan(const std::function<void(int, int, const QString &)> &onProgress = nullptr);

	void setRoots(const QStringList &roots) { m_roots = roots; }
	QStringList roots() const { return m_roots; }
	static QStringList defaultRoots();

	const QVector<LibraryEntry> &entries() const { return m_entries; }

	// Returns indices into entries() matching `query` (substring over name/brand/board numbers).
	QVector<int> search(const QString &query) const;

	// Ensures the file exists locally, extracting from its archive if needed. Returns the
	// local path (empty on failure, with `error` set).
	static QString materialize(const LibraryFile &file, QString *error);

	// Extracts normalised board identifiers from arbitrary text (filenames, folder paths).
	static QStringList extractBoardNumbers(const QString &text);

  private:
	void scanRoot(const QString &root,
	              const std::function<void(int, int, const QString &)> &onProgress, int &done, int &total);
	void addLooseFolder(const QString &brand, const QString &dir);
	void addArchive(const QString &brand, const QString &archivePath);

	QVector<ArchiveMember> listArchiveCached(const QString &archivePath);

	void loadCache();
	void saveCache() const;

	QStringList m_roots;
	QVector<LibraryEntry> m_entries;

	struct CacheEntry {
		qint64 mtime = 0;
		QVector<ArchiveMember> members;
	};
	QHash<QString, CacheEntry> m_archiveCache;
	mutable bool m_cacheDirty = false;
};

} // namespace w2r
