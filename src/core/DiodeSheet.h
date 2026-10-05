// Diode-mode reading import.
//
// The boardview formats in this collection carry no diode data, so readings are imported
// from a user-supplied JSON or CSV sheet and matched to pins by (component, pin).
//
// Accepted JSON shapes:
//   { "U1001": { "A1": "0.450V", "A2": "OL" } }
//   [ { "component": "U1001", "pin": "A1", "diode": "0.450V" }, ... ]
// Accepted CSV (header optional, order: component, pin, value):
//   component,pin,diode
//   U1001,A1,0.450V
#pragma once

#include "core/Board.h"

#include <QString>

namespace w2r {

// Applies readings to `board`. Returns the number of pins updated; on failure returns -1
// and sets `error`.
int applyDiodeFile(Board &board, const QString &path, QString *error);

} // namespace w2r
