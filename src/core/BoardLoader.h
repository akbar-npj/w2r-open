// Board file loading: format sniffing + delegation to the vendored OpenBoardView parsers.
#pragma once

#include "core/Board.h"

#include <QString>

namespace w2r {

struct LoadResult {
	bool ok = false;
	QString error;
	Board board;
};

// Detects the boardview format of `path` and loads it. Supported today: obfuscated BRD
// (.brd) and BVRAW_FORMAT_3 (.bvr). More formats are added by vendoring further parsers.
LoadResult loadBoardFile(const QString &path);

// Returns a short format tag ("BRD", "BVR3", ...) or an empty string if unrecognised.
QString sniffBoardFormat(const QString &path);

} // namespace w2r
