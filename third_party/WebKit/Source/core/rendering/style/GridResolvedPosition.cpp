// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/style/GridResolvedPosition.h"

#include "core/rendering/RenderBox.h"
#include "core/rendering/style/GridCoordinate.h"

namespace WebCore {

GridSpan GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(const RenderBox&, GridTrackSizingDirection, const GridResolvedPosition& initialPosition)
{
    // FIXME: We don't support spanning with auto positions yet. Once we do, this is wrong. Also we should make
    // sure the grid can accomodate the new item as we only grow 1 position in a given direction.
    return GridSpan(initialPosition, initialPosition);
}

PassOwnPtr<GridSpan> GridResolvedPosition::resolveGridPositionsFromStyle(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition = (direction == ForColumns) ? gridItem.style()->gridColumnStart() : gridItem.style()->gridRowStart();
    const GridPositionSide initialPositionSide = (direction == ForColumns) ? ColumnStartSide : RowStartSide;
    GridPosition finalPosition = (direction == ForColumns) ? gridItem.style()->gridColumnEnd() : gridItem.style()->gridRowEnd();
    const GridPositionSide finalPositionSide = (direction == ForColumns) ? ColumnEndSide : RowEndSide;

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    if (initialPosition.isNamedGridArea() && !gridContainerStyle.namedGridArea().contains(initialPosition.namedGridLine()))
        initialPosition.setAutoPosition();

    if (finalPosition.isNamedGridArea() && !gridContainerStyle.namedGridArea().contains(finalPosition.namedGridLine()))
        finalPosition.setAutoPosition();

    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        if (gridContainerStyle.gridAutoFlow() == AutoFlowNone)
            return adoptPtr(new GridSpan(0, 0));

        // We can't get our grid positions without running the auto placement algorithm.
        return nullptr;
    }

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        GridResolvedPosition finalResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalPositionSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, finalResolvedPosition, initialPosition, initialPositionSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        GridResolvedPosition initialResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialPositionSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, initialResolvedPosition, finalPosition, finalPositionSide);
    }

    GridResolvedPosition resolvedInitialPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialPositionSide);
    GridResolvedPosition resolvedFinalPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalPositionSide);

    // If 'grid-after' specifies a line at or before that specified by 'grid-before', it computes to 'span 1'.
    if (resolvedFinalPosition < resolvedInitialPosition)
        resolvedFinalPosition = resolvedInitialPosition;

    return adoptPtr(new GridSpan(resolvedInitialPosition, resolvedFinalPosition));
}

size_t GridResolvedPosition::explicitGridColumnCount(const RenderStyle& gridContainerStyle)
{
    return gridContainerStyle.gridTemplateColumns().size();
}

size_t GridResolvedPosition::explicitGridRowCount(const RenderStyle& gridContainerStyle)
{
    return gridContainerStyle.gridTemplateRows().size();
}

size_t GridResolvedPosition::explicitGridSizeForSide(const RenderStyle& gridContainerStyle, GridPositionSide side)
{
    return (side == ColumnStartSide || side == ColumnEndSide) ? explicitGridColumnCount(gridContainerStyle) : explicitGridRowCount(gridContainerStyle);
}

GridResolvedPosition GridResolvedPosition::resolveNamedGridLinePositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    ASSERT(!position.namedGridLine().isNull());

    const NamedGridLinesMap& gridLinesNames = (side == ColumnStartSide || side == ColumnEndSide) ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    if (it == gridLinesNames.end()) {
        if (position.isPositive())
            return GridResolvedPosition(0);
        const size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side);
        return adjustGridPositionForSide(lastLine, side);
    }

    size_t namedGridLineIndex;
    if (position.isPositive())
        namedGridLineIndex = std::min<size_t>(position.integerPosition(), it->value.size()) - 1;
    else
        namedGridLineIndex = std::max<int>(it->value.size() - abs(position.integerPosition()), 0);
    return adjustGridPositionForSide(it->value[namedGridLineIndex], side);
}

GridResolvedPosition GridResolvedPosition::resolveGridPositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainerStyle, position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return adjustGridPositionForSide(position.integerPosition() - 1, side);

        size_t resolvedPosition = abs(position.integerPosition()) - 1;
        const size_t endOfTrack = explicitGridSizeForSide(gridContainerStyle, side);

        // Per http://lists.w3.org/Archives/Public/www-style/2013Mar/0589.html, we clamp negative value to the first line.
        if (endOfTrack < resolvedPosition)
            return GridResolvedPosition(0);

        return adjustGridPositionForSide(endOfTrack - resolvedPosition, side);
    }
    case NamedGridAreaPosition:
    {
        NamedGridAreaMap::const_iterator it = gridContainerStyle.namedGridArea().find(position.namedGridLine());
        // Unknown grid area should have been computed to 'auto' by now.
        ASSERT_WITH_SECURITY_IMPLICATION(it != gridContainerStyle.namedGridArea().end());
        const GridCoordinate& gridAreaCoordinate = it->value;
        switch (side) {
        case ColumnStartSide:
            return gridAreaCoordinate.columns.resolvedInitialPosition;
        case ColumnEndSide:
            return gridAreaCoordinate.columns.resolvedFinalPosition;
        case RowStartSide:
            return gridAreaCoordinate.rows.resolvedInitialPosition;
        case RowEndSide:
            return GridResolvedPosition(gridAreaCoordinate.rows.resolvedFinalPosition);
        }
        ASSERT_NOT_REACHED();
        return GridResolvedPosition(0);
    }
    case AutoPosition:
    case SpanPosition:
        // 'auto' and span depend on the opposite position for resolution (e.g. grid-row: auto / 1 or grid-column: span 3 / "myHeader").
        ASSERT_NOT_REACHED();
        return GridResolvedPosition(0);
    }
    ASSERT_NOT_REACHED();
    return GridResolvedPosition(0);
}

PassOwnPtr<GridSpan> GridResolvedPosition::resolveGridPositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    if (position.isAuto())
        return GridSpan::create(resolvedOppositePosition, resolvedOppositePosition);

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(gridContainerStyle, resolvedOppositePosition, position, side);
    }

    return GridSpan::createWithSpanAgainstOpposite(resolvedOppositePosition, position, side);
}

PassOwnPtr<GridSpan> GridResolvedPosition::resolveNamedGridLinePositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    const NamedGridLinesMap& gridLinesNames = (side == ColumnStartSide || side == ColumnEndSide) ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());

    // If there is no named grid line of that name, we resolve the position to 'auto' (which is equivalent to 'span 1' in this case).
    // See http://lists.w3.org/Archives/Public/www-style/2013Jun/0394.html.
    if (it == gridLinesNames.end())
        return GridSpan::create(resolvedOppositePosition, resolvedOppositePosition);

    return GridSpan::createWithNamedSpanAgainstOpposite(resolvedOppositePosition, position, side, it->value);
}

} // namespace WebCore
