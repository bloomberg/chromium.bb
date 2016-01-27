// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/GridResolvedPosition.h"

#include "core/layout/LayoutBox.h"
#include "core/style/GridCoordinate.h"
#include <algorithm>

namespace blink {

static const NamedGridLinesMap& gridLinesForSide(const ComputedStyle& style, GridPositionSide side)
{
    return (side == ColumnStartSide || side == ColumnEndSide) ? style.namedGridColumnLines() : style.namedGridRowLines();
}

static inline String implicitNamedGridLineForSide(const String& lineName, GridPositionSide side)
{
    return lineName + ((side == ColumnStartSide || side == RowStartSide) ? "-start" : "-end");
}

bool GridResolvedPosition::isValidNamedLineOrArea(const String& lineName, const ComputedStyle& style, GridPositionSide side)
{
    const NamedGridLinesMap& gridLineNames = gridLinesForSide(style, side);

    return gridLineNames.contains(implicitNamedGridLineForSide(lineName, side)) || gridLineNames.contains(lineName);
}

GridPositionSide GridResolvedPosition::initialPositionSide(GridTrackSizingDirection direction)
{
    return (direction == ForColumns) ? ColumnStartSide : RowStartSide;
}

GridPositionSide GridResolvedPosition::finalPositionSide(GridTrackSizingDirection direction)
{
    return (direction == ForColumns) ? ColumnEndSide : RowEndSide;
}

static void initialAndFinalPositionsFromStyle(const ComputedStyle& gridContainerStyle, const LayoutBox& gridItem, GridTrackSizingDirection direction, GridPosition& initialPosition, GridPosition& finalPosition)
{
    initialPosition = (direction == ForColumns) ? gridItem.style()->gridColumnStart() : gridItem.style()->gridRowStart();
    finalPosition = (direction == ForColumns) ? gridItem.style()->gridColumnEnd() : gridItem.style()->gridRowEnd();

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    if (gridItem.isOutOfFlowPositioned()) {
        // Early detect the case of non existing named grid lines for positioned items.
        if (initialPosition.isNamedGridArea() && !GridResolvedPosition::isValidNamedLineOrArea(initialPosition.namedGridLine(), gridContainerStyle, GridResolvedPosition::initialPositionSide(direction)))
            initialPosition.setAutoPosition();

        if (finalPosition.isNamedGridArea() && !GridResolvedPosition::isValidNamedLineOrArea(finalPosition.namedGridLine(), gridContainerStyle, GridResolvedPosition::finalPositionSide(direction)))
            finalPosition.setAutoPosition();
    }

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (initialPosition.isAuto() && finalPosition.isSpan() && !finalPosition.namedGridLine().isNull())
        finalPosition.setSpanPosition(1, String());
    if (finalPosition.isAuto() && initialPosition.isSpan() && !initialPosition.namedGridLine().isNull())
        initialPosition.setSpanPosition(1, String());
}

static size_t lookAheadForNamedGridLine(int start, size_t numberOfLines, const Vector<size_t>* namedGridLinesIndexes, size_t gridLastLine)
{
    ASSERT(numberOfLines);

    // Only implicit lines on the search direction are assumed to have the given name, so we can start to look from first line.
    // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
    size_t end = std::max(start, 0);

    if (!namedGridLinesIndexes) {
        end = std::max(end, gridLastLine + 1);
        return end + numberOfLines - 1;
    }

    for (; numberOfLines; ++end) {
        if (end > gridLastLine || namedGridLinesIndexes->contains(end))
            numberOfLines--;
    }

    ASSERT(end);
    return end - 1;
}

static int lookBackForNamedGridLine(int end, size_t numberOfLines, const Vector<size_t>* namedGridLinesIndexes, int gridLastLine)
{
    ASSERT(numberOfLines);

    // Only implicit lines on the search direction are assumed to have the given name, so we can start to look from last line.
    // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
    int start = std::min(end, gridLastLine);

    if (!namedGridLinesIndexes) {
        start = std::min(start, -1);
        return start - numberOfLines + 1;
    }

    for (; numberOfLines; --start) {
        if (start < 0 || namedGridLinesIndexes->contains(static_cast<size_t>(start)))
            numberOfLines--;
    }

    return start + 1;
}

static GridSpan definiteGridSpanWithNamedSpanAgainstOpposite(int resolvedOppositePosition, const GridPosition& position, GridPositionSide side, const Vector<size_t>* gridLines, int lastLine)
{
    int start, end;

    if (side == RowStartSide || side == ColumnStartSide) {
        start = lookBackForNamedGridLine(resolvedOppositePosition - 1, position.spanPosition(), gridLines, lastLine);
        end = resolvedOppositePosition;
    } else {
        start = resolvedOppositePosition;
        end = lookAheadForNamedGridLine(resolvedOppositePosition + 1, position.spanPosition(), gridLines, lastLine);
    }

    return GridSpan::untranslatedDefiniteGridSpan(start, end);
}

size_t GridResolvedPosition::explicitGridColumnCount(const ComputedStyle& gridContainerStyle)
{
    return std::min<size_t>(gridContainerStyle.gridTemplateColumns().size(), kGridMaxTracks);
}

size_t GridResolvedPosition::explicitGridRowCount(const ComputedStyle& gridContainerStyle)
{
    return std::min<size_t>(gridContainerStyle.gridTemplateRows().size(), kGridMaxTracks);
}

static size_t explicitGridSizeForSide(const ComputedStyle& gridContainerStyle, GridPositionSide side)
{
    return (side == ColumnStartSide || side == ColumnEndSide) ? GridResolvedPosition::explicitGridColumnCount(gridContainerStyle) : GridResolvedPosition::explicitGridRowCount(gridContainerStyle);
}

static GridSpan resolveNamedGridLinePositionAgainstOppositePosition(const ComputedStyle& gridContainerStyle, int resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    const NamedGridLinesMap& gridLinesNames = gridLinesForSide(gridContainerStyle, side);
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    const Vector<size_t>* gridLines = it == gridLinesNames.end() ? nullptr : &it->value;
    size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side);
    return definiteGridSpanWithNamedSpanAgainstOpposite(resolvedOppositePosition, position, side, gridLines, lastLine);
}

static GridSpan definiteGridSpanWithSpanAgainstOpposite(size_t resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    size_t positionOffset = position.spanPosition();
    if (side == ColumnStartSide || side == RowStartSide)
        return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition - positionOffset, resolvedOppositePosition);

    return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedOppositePosition + positionOffset);
}

static GridSpan resolveGridPositionAgainstOppositePosition(const ComputedStyle& gridContainerStyle, int resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    if (position.isAuto()) {
        if (side == ColumnStartSide || side == RowStartSide)
            return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition - 1, resolvedOppositePosition);
        return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedOppositePosition + 1);
    }

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(gridContainerStyle, resolvedOppositePosition, position, side);
    }

    return definiteGridSpanWithSpanAgainstOpposite(resolvedOppositePosition, position, side);
}

size_t GridResolvedPosition::spanSizeForAutoPlacedItem(const ComputedStyle& gridContainerStyle, const LayoutBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    initialAndFinalPositionsFromStyle(gridContainerStyle, gridItem, direction, initialPosition, finalPosition);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition());

    if (initialPosition.isAuto() && finalPosition.isAuto())
        return 1;

    GridPosition position = initialPosition.isSpan() ? initialPosition : finalPosition;
    ASSERT(position.isSpan());
    ASSERT(position.spanPosition());
    return position.spanPosition();
}

static int resolveNamedGridLinePositionFromStyle(const ComputedStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    ASSERT(!position.namedGridLine().isNull());

    const NamedGridLinesMap& gridLinesNames = gridLinesForSide(gridContainerStyle, side);
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    const Vector<size_t>* gridLines = it == gridLinesNames.end() ? nullptr : &it->value;
    size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side);
    if (position.isPositive())
        return lookAheadForNamedGridLine(0, abs(position.integerPosition()), gridLines, lastLine);
    else
        return lookBackForNamedGridLine(lastLine, abs(position.integerPosition()), gridLines, lastLine);
}

static int resolveGridPositionFromStyle(const ComputedStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainerStyle, position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return position.integerPosition() - 1;

        size_t resolvedPosition = abs(position.integerPosition()) - 1;
        size_t endOfTrack = explicitGridSizeForSide(gridContainerStyle, side);

        return endOfTrack - resolvedPosition;
    }
    case NamedGridAreaPosition:
    {
        // First attempt to match the grid area's edge to a named grid area: if there is a named line with the name
        // ''<custom-ident>-start (for grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the first such
        // line to the grid item's placement.
        String namedGridLine = position.namedGridLine();
        ASSERT(!position.namedGridLine().isNull());

        const NamedGridLinesMap& gridLineNames = gridLinesForSide(gridContainerStyle, side);
        NamedGridLinesMap::const_iterator implicitLineIter = gridLineNames.find(implicitNamedGridLineForSide(namedGridLine, side));
        if (implicitLineIter != gridLineNames.end())
            return implicitLineIter->value[0];

        // Otherwise, if there is a named line with the specified name, contributes the first such line to the grid
        // item's placement.
        NamedGridLinesMap::const_iterator explicitLineIter = gridLineNames.find(namedGridLine);
        if (explicitLineIter != gridLineNames.end())
            return explicitLineIter->value[0];

        ASSERT(!GridResolvedPosition::isValidNamedLineOrArea(namedGridLine, gridContainerStyle, side));
        // If none of the above works specs mandate to assume that all the lines in the implicit grid have this name.
        size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side);
        return lastLine + 1;
    }
    case AutoPosition:
    case SpanPosition:
        // 'auto' and span depend on the opposite position for resolution (e.g. grid-row: auto / 1 or grid-column: span 3 / "myHeader").
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

GridSpan GridResolvedPosition::resolveGridPositionsFromStyle(const ComputedStyle& gridContainerStyle, const LayoutBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    initialAndFinalPositionsFromStyle(gridContainerStyle, gridItem, direction, initialPosition, finalPosition);

    GridPositionSide initialSide = initialPositionSide(direction);
    GridPositionSide finalSide = finalPositionSide(direction);

    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // We can't get our grid positions without running the auto placement algorithm.
        return GridSpan::indefiniteGridSpan();
    }

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        int finalResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, finalResolvedPosition, initialPosition, initialSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        int initialResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, initialResolvedPosition, finalPosition, finalSide);
    }

    int resolvedInitialPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialSide);
    int resolvedFinalPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalSide);

    if (resolvedFinalPosition < resolvedInitialPosition)
        std::swap(resolvedFinalPosition, resolvedInitialPosition);
    else if (resolvedFinalPosition == resolvedInitialPosition)
        resolvedFinalPosition = resolvedInitialPosition + 1;

    return GridSpan::untranslatedDefiniteGridSpan(resolvedInitialPosition, resolvedFinalPosition);
}

} // namespace blink
