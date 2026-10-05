// Adapter: OpenBoardView parser output (BRDFileBase) -> our Board model.
//
// The vendored OpenBoardView parsers all produce a BRDFileBase with parts/pins/nails and an
// outline, using mils. This translates that into the w2r::Board used by the rest of the app,
// keeping the vendored (MIT-licensed) OpenBoardView code isolated from our own model.
#pragma once

#include "core/Board.h"

#include <QString>

class BRDFileBase;

namespace w2r {

// Adapts `ob` into a Board. `name` is the display name, `formatTag` a short label ("BRD", "BVR3"...).
// `includeNails` adds standalone test points (nails) as pins owned by a synthetic part.
Board adaptObBoard(BRDFileBase &ob, const QString &name, const QString &formatTag, bool includeNails = false);

} // namespace w2r
