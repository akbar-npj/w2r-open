// Archive access for the local library.
//
// Your collection is dominated by .rar/.zip/.7z archives holding boardviews and schematics.
// We list members and extract on demand using the XAD tools (`lsar` for listing, `unar` for
// extraction) which ship with most distros and support RAR5. Nothing is extracted until a
// board is actually opened; extracted members land in a cache dir keyed by archive path.
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace w2r {

struct ArchiveMember {
	QString path; // path of the member inside the archive
	qint64 size = 0;
};

class ArchiveBackend {
  public:
	// True if `lsar` and `unar` are on PATH. `detail` receives a human-readable reason.
	static bool toolsAvailable(QString *detail = nullptr);

	// True if the file extension looks like a supported archive.
	static bool isArchive(const QString &path);

	// Lists regular-file members of an archive (empty on failure). Directories are skipped.
	static QVector<ArchiveMember> list(const QString &archivePath);

	// Extracts `members` from `archivePath` into `destDir` (created if needed). If `members`
	// is empty, extracts everything. Returns true on success.
	static bool extract(const QString &archivePath, const QStringList &members, const QString &destDir);

	// Cache directory for a given archive: ~/.cache/w2r-open/extract/<hash>/
	static QString cacheDirFor(const QString &archivePath);

	// Stable cache root: ~/.cache/w2r-open (independent of the application name).
	static QString cacheRoot();
};

} // namespace w2r
