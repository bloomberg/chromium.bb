// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridResolvedPosition_h
#define GridResolvedPosition_h

#include "core/style/GridPosition.h"
#include "wtf/Allocator.h"

namespace blink {

struct GridSpan;
class LayoutBox;
class ComputedStyle;

enum GridPositionSide {
    ColumnStartSide,
    ColumnEndSide,
    RowStartSide,
    RowEndSide
};

enum GridTrackSizingDirection {
    ForColumns,
    ForRows
};

// This is a utility class with all the code related to grid items positions resolution.
// TODO(rego): Rename class to GridPositionsResolver.
class GridResolvedPosition {
    DISALLOW_NEW();
public:

    static size_t explicitGridColumnCount(const ComputedStyle&);
    static size_t explicitGridRowCount(const ComputedStyle&);

    static bool isValidNamedLineOrArea(const String& lineName, const ComputedStyle&, GridPositionSide);

    static GridPositionSide initialPositionSide(GridTrackSizingDirection);
    static GridPositionSide finalPositionSide(GridTrackSizingDirection);

    static size_t spanSizeForAutoPlacedItem(const ComputedStyle&, const LayoutBox&, GridTrackSizingDirection);
    static GridSpan resolveGridPositionsFromStyle(const ComputedStyle&, const LayoutBox&, GridTrackSizingDirection);

};

} // namespace blink

#endif // GridResolvedPosition_h
