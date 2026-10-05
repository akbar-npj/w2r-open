// Pin-name recovery for formats that carry no pin numbers.
//
// The obfuscated BRD (.brd) format stores only a pin's coordinates, probe index, owning
// part and net — it has no pin number/name field at all (see BRDFile.cpp). The BVRAW_FORMAT_3
// (.bvr) format, however, stores PIN_NUMBER / PIN_NAME. Board collections frequently ship both
// files for the same board, so when a BRD board is loaded we correlate its pins with a sibling
// BVR by position and copy the missing pin numbers across.
//
// The two formats use different axis conventions: a BRD coordinate (x, y) corresponds to a BVR
// coordinate (-y, x). We do not hard-code that single guess — a small set of candidate
// transforms is tried and the one with the most matches wins, which keeps this robust if a
// collection uses a different convention.
#pragma once

#include "core/Board.h"

#include <QString>

namespace w2r {

// Fills in empty pin names on `board` (loaded from `boardPath`) using a sibling .bvr file.
// Returns the number of pin names recovered (0 when nothing was needed or nothing matched).
// This is best-effort: a missing or mismatched sibling is not an error.
int recoverPinNamesFromSibling(Board &board, const QString &boardPath);

} // namespace w2r
