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

// This class represents a line index into one of the dimensions of the grid array.
// Wraps a size_t integer just for the purpose of knowing what we manipulate in the grid code.
class GridResolvedPosition {
    DISALLOW_NEW();
public:

    GridResolvedPosition(size_t position)
        : m_integerPosition(position)
    {
    }

    GridResolvedPosition& operator*()
    {
        return *this;
    }

    GridResolvedPosition& operator++()
    {
        m_integerPosition++;
        return *this;
    }

    bool operator==(const GridResolvedPosition& other) const
    {
        return m_integerPosition == other.m_integerPosition;
    }

    bool operator!=(const GridResolvedPosition& other) const
    {
        return m_integerPosition != other.m_integerPosition;
    }

    bool operator<(const GridResolvedPosition& other) const
    {
        return m_integerPosition < other.m_integerPosition;
    }

    bool operator>(const GridResolvedPosition& other) const
    {
        return m_integerPosition > other.m_integerPosition;
    }

    bool operator<=(const GridResolvedPosition& other) const
    {
        return m_integerPosition <= other.m_integerPosition;
    }

    bool operator>=(const GridResolvedPosition& other) const
    {
        return m_integerPosition >= other.m_integerPosition;
    }

    size_t toInt() const
    {
        return m_integerPosition;
    }

    GridResolvedPosition next() const
    {
        return GridResolvedPosition(m_integerPosition + 1);
    }

    GridResolvedPosition prev() const
    {
        return GridResolvedPosition(m_integerPosition > 0 ? m_integerPosition - 1 : 0);
    }

    static size_t explicitGridColumnCount(const ComputedStyle&);
    static size_t explicitGridRowCount(const ComputedStyle&);

    static bool isValidNamedLineOrArea(const String& lineName, const ComputedStyle&, GridPositionSide);

    static GridPositionSide initialPositionSide(GridTrackSizingDirection);
    static GridPositionSide finalPositionSide(GridTrackSizingDirection);

    static GridSpan resolveGridPositionsFromAutoPlacementPosition(const ComputedStyle&, const LayoutBox&, GridTrackSizingDirection, const GridResolvedPosition&);
    static GridSpan resolveGridPositionsFromStyle(const ComputedStyle&, const LayoutBox&, GridTrackSizingDirection);

private:

    size_t m_integerPosition;
};

} // namespace blink

#endif // GridResolvedPosition_h
