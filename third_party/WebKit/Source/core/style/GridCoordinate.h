/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GridCoordinate_h
#define GridCoordinate_h

#include "core/style/GridResolvedPosition.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace blink {

// Recommended maximum size for both explicit and implicit grids.
const size_t kGridMaxTracks = 1000000;

// A span in a single direction (either rows or columns). Note that |resolvedInitialPosition|
// and |resolvedFinalPosition| are grid lines' indexes.
// Iterating over the span shouldn't include |resolvedFinalPosition| to be correct.
struct GridSpan {
    USING_FAST_MALLOC(GridSpan);
public:

    static GridSpan definiteGridSpan(const GridResolvedPosition& resolvedInitialPosition, const GridResolvedPosition& resolvedFinalPosition)
    {
        return GridSpan(resolvedInitialPosition, resolvedFinalPosition, Definite);
    }

    static GridSpan indefiniteGridSpan()
    {
        return GridSpan(0, 1, Indefinite);
    }

    static GridSpan definiteGridSpanWithSpanAgainstOpposite(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
    {
        size_t positionOffset = position.spanPosition();
        if (side == ColumnStartSide || side == RowStartSide) {
            if (resolvedOppositePosition == 0)
                return GridSpan::definiteGridSpan(resolvedOppositePosition, resolvedOppositePosition.next());

            GridResolvedPosition initialResolvedPosition = GridResolvedPosition(std::max<int>(0, resolvedOppositePosition.toInt() - positionOffset));
            return GridSpan::definiteGridSpan(initialResolvedPosition, resolvedOppositePosition);
        }

        return GridSpan::definiteGridSpan(resolvedOppositePosition, GridResolvedPosition(resolvedOppositePosition.toInt() + positionOffset));
    }

    static GridSpan definiteGridSpanWithNamedSpanAgainstOpposite(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side, const Vector<size_t>& gridLines)
    {
        if (side == RowStartSide || side == ColumnStartSide)
            return definiteGridSpanWithInitialNamedSpanAgainstOpposite(resolvedOppositePosition, position, gridLines);

        return definiteGridSpanWithFinalNamedSpanAgainstOpposite(resolvedOppositePosition, position, gridLines);
    }

    static GridSpan definiteGridSpanWithInitialNamedSpanAgainstOpposite(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, const Vector<size_t>& gridLines)
    {
        if (resolvedOppositePosition == 0)
            return GridSpan::definiteGridSpan(resolvedOppositePosition, resolvedOppositePosition.next());

        size_t firstLineBeforeOppositePositionIndex = 0;
        const size_t* firstLineBeforeOppositePosition = std::lower_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition.toInt());
        if (firstLineBeforeOppositePosition != gridLines.end())
            firstLineBeforeOppositePositionIndex = firstLineBeforeOppositePosition - gridLines.begin();
        size_t gridLineIndex = std::max<int>(0, firstLineBeforeOppositePositionIndex - position.spanPosition());
        GridResolvedPosition resolvedGridLinePosition = GridResolvedPosition(gridLines[gridLineIndex]);
        if (resolvedGridLinePosition >= resolvedOppositePosition)
            resolvedGridLinePosition = resolvedOppositePosition.prev();
        return GridSpan::definiteGridSpan(resolvedGridLinePosition, resolvedOppositePosition);
    }

    static GridSpan definiteGridSpanWithFinalNamedSpanAgainstOpposite(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, const Vector<size_t>& gridLines)
    {
        size_t firstLineAfterOppositePositionIndex = gridLines.size() - 1;
        const size_t* firstLineAfterOppositePosition = std::upper_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition.toInt());
        if (firstLineAfterOppositePosition != gridLines.end())
            firstLineAfterOppositePositionIndex = firstLineAfterOppositePosition - gridLines.begin();
        size_t gridLineIndex = std::min(gridLines.size() - 1, firstLineAfterOppositePositionIndex + position.spanPosition() - 1);
        GridResolvedPosition resolvedGridLinePosition = gridLines[gridLineIndex];
        if (resolvedGridLinePosition <= resolvedOppositePosition)
            resolvedGridLinePosition = resolvedOppositePosition.next();

        return GridSpan::definiteGridSpan(resolvedOppositePosition, resolvedGridLinePosition);
    }

    bool operator==(const GridSpan& o) const
    {
        return m_type == o.m_type && m_resolvedInitialPosition == o.m_resolvedInitialPosition && m_resolvedFinalPosition == o.m_resolvedFinalPosition;
    }

    size_t integerSpan() const
    {
        ASSERT(isDefinite());
        return m_resolvedFinalPosition.toInt() - m_resolvedInitialPosition.toInt();
    }

    const GridResolvedPosition& resolvedInitialPosition() const
    {
        ASSERT(isDefinite());
        return m_resolvedInitialPosition;
    }

    const GridResolvedPosition& resolvedFinalPosition() const
    {
        ASSERT(isDefinite());
        return m_resolvedFinalPosition;
    }

    typedef GridResolvedPosition iterator;

    iterator begin() const
    {
        ASSERT(isDefinite());
        return m_resolvedInitialPosition;
    }

    iterator end() const
    {
        ASSERT(isDefinite());
        return m_resolvedFinalPosition;
    }

    bool isDefinite() const
    {
        return m_type == Definite;
    }

private:

    enum GridSpanType {Definite, Indefinite};

    GridSpan(const GridResolvedPosition& resolvedInitialPosition, const GridResolvedPosition& resolvedFinalPosition, GridSpanType type)
        : m_resolvedInitialPosition(std::min(resolvedInitialPosition.toInt(), kGridMaxTracks - 1))
        , m_resolvedFinalPosition(std::min(resolvedFinalPosition.toInt(), kGridMaxTracks))
        , m_type(type)
    {
        ASSERT(resolvedInitialPosition < resolvedFinalPosition);
    }

    GridResolvedPosition m_resolvedInitialPosition;
    GridResolvedPosition m_resolvedFinalPosition;
    GridSpanType m_type;
};

// This represents a grid area that spans in both rows' and columns' direction.
struct GridCoordinate {
    USING_FAST_MALLOC(GridCoordinate);
public:
    // HashMap requires a default constuctor.
    GridCoordinate()
        : columns(GridSpan::indefiniteGridSpan())
        , rows(GridSpan::indefiniteGridSpan())
    {
    }

    GridCoordinate(const GridSpan& r, const GridSpan& c)
        : columns(c)
        , rows(r)
    {
    }

    bool operator==(const GridCoordinate& o) const
    {
        return columns == o.columns && rows == o.rows;
    }

    bool operator!=(const GridCoordinate& o) const
    {
        return !(*this == o);
    }

    GridResolvedPosition positionForSide(GridPositionSide side) const
    {
        switch (side) {
        case ColumnStartSide:
            return columns.resolvedInitialPosition();
        case ColumnEndSide:
            return columns.resolvedFinalPosition();
        case RowStartSide:
            return rows.resolvedInitialPosition();
        case RowEndSide:
            return rows.resolvedFinalPosition();
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    GridSpan columns;
    GridSpan rows;
};

typedef HashMap<String, GridCoordinate> NamedGridAreaMap;

} // namespace blink

#endif // GridCoordinate_h
