/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderGrid.h"

#include "core/rendering/LayoutRepainter.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/GridCoordinate.h"
#include "platform/LengthFunctions.h"

namespace WebCore {

static const int infinity = -1;

class GridTrack {
public:
    GridTrack()
        : m_usedBreadth(0)
        , m_maxBreadth(0)
    {
    }

    void growUsedBreadth(LayoutUnit growth)
    {
        ASSERT(growth >= 0);
        m_usedBreadth += growth;
    }
    LayoutUnit usedBreadth() const { return m_usedBreadth; }

    void growMaxBreadth(LayoutUnit growth)
    {
        if (m_maxBreadth == infinity)
            m_maxBreadth = m_usedBreadth + growth;
        else
            m_maxBreadth += growth;
    }
    LayoutUnit maxBreadthIfNotInfinite() const
    {
        return (m_maxBreadth == infinity) ? m_usedBreadth : m_maxBreadth;
    }

    LayoutUnit m_usedBreadth;
    LayoutUnit m_maxBreadth;
};

struct GridTrackForNormalization {
    GridTrackForNormalization(const GridTrack& track, double flex)
        : m_track(&track)
        , m_flex(flex)
        , m_normalizedFlexValue(track.m_usedBreadth / flex)
    {
    }

    // Required by std::sort.
    GridTrackForNormalization& operator=(const GridTrackForNormalization& o)
    {
        m_track = o.m_track;
        m_flex = o.m_flex;
        m_normalizedFlexValue = o.m_normalizedFlexValue;
        return *this;
    }

    const GridTrack* m_track;
    double m_flex;
    LayoutUnit m_normalizedFlexValue;
};

class RenderGrid::GridIterator {
    WTF_MAKE_NONCOPYABLE(GridIterator);
public:
    // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
    // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd column.
    GridIterator(const GridRepresentation& grid, GridTrackSizingDirection direction, size_t fixedTrackIndex)
        : m_grid(grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? 0 : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : 0)
        , m_childIndex(0)
    {
        ASSERT(m_rowIndex < m_grid.size());
        ASSERT(m_columnIndex < m_grid[0].size());
    }

    RenderBox* nextGridItem()
    {
        ASSERT(!m_grid.isEmpty());

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const GridCell& children = m_grid[m_rowIndex][m_columnIndex];
            if (m_childIndex < children.size())
                return children[m_childIndex++];

            m_childIndex = 0;
        }
        return 0;
    }

    PassOwnPtr<GridCoordinate> nextEmptyGridArea()
    {
        ASSERT(!m_grid.isEmpty());

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const GridCell& children = m_grid[m_rowIndex][m_columnIndex];
            if (children.isEmpty()) {
                OwnPtr<GridCoordinate> result = adoptPtr(new GridCoordinate(GridSpan(m_rowIndex, m_rowIndex), GridSpan(m_columnIndex, m_columnIndex)));
                // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
                ++varyingTrackIndex;
                return result.release();
            }
        }
        return nullptr;
    }

private:
    const GridRepresentation& m_grid;
    GridTrackSizingDirection m_direction;
    size_t m_rowIndex;
    size_t m_columnIndex;
    size_t m_childIndex;
};

struct RenderGrid::GridSizingData {
    WTF_MAKE_NONCOPYABLE(GridSizingData);
public:
    GridSizingData(size_t gridColumnCount, size_t gridRowCount)
        : columnTracks(gridColumnCount)
        , rowTracks(gridRowCount)
    {
    }

    Vector<GridTrack> columnTracks;
    Vector<GridTrack> rowTracks;
    Vector<size_t> contentSizedTracksIndex;

    // Performance optimization: hold onto these Vectors until the end of Layout to avoid repeated malloc / free.
    Vector<LayoutUnit> distributeTrackVector;
    Vector<GridTrack*> filteredTracks;
};

RenderGrid::RenderGrid(Element* element)
    : RenderBlock(element)
    , m_gridIsDirty(true)
    , m_orderIterator(this)
{
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid()
{
}

void RenderGrid::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlock::addChild(newChild, beforeChild);

    if (gridIsDirty())
        return;

    if (!newChild->isBox()) {
        dirtyGrid();
        return;
    }

    if (style()->gridAutoFlow() != AutoFlowNone) {
        // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
        dirtyGrid();
        return;
    }

    RenderBox* newChildBox = toRenderBox(newChild);
    OwnPtr<GridSpan> rowPositions = resolveGridPositionsFromStyle(newChildBox, ForRows);
    OwnPtr<GridSpan> columnPositions = resolveGridPositionsFromStyle(newChildBox, ForColumns);
    if (!rowPositions || !columnPositions) {
        // The new child requires the auto-placement algorithm to run so we need to recompute the grid fully.
        dirtyGrid();
        return;
    } else {
        // Ensure that the grid is big enough to contain new grid item.
        if (gridRowCount() <= rowPositions->finalPositionIndex)
            growGrid(ForRows, rowPositions->finalPositionIndex);
        if (gridColumnCount() <= columnPositions->finalPositionIndex)
            growGrid(ForColumns, columnPositions->finalPositionIndex);

        insertItemIntoGrid(newChildBox, GridCoordinate(*rowPositions, *columnPositions));
    }
}

void RenderGrid::removeChild(RenderObject* child)
{
    RenderBlock::removeChild(child);

    if (gridIsDirty())
        return;

    ASSERT(child->isBox());

    if (style()->gridAutoFlow() != AutoFlowNone) {
        // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
        dirtyGrid();
        return;
    }

    const RenderBox* childBox = toRenderBox(child);
    GridCoordinate coordinate = m_gridItemCoordinate.take(childBox);

    for (size_t row = coordinate.rows.initialPositionIndex; row <= coordinate.rows.finalPositionIndex; ++row) {
        for (size_t column = coordinate.columns.initialPositionIndex; column <= coordinate.columns.finalPositionIndex; ++column) {
            GridCell& cell = m_grid[row][column];
            cell.remove(cell.find(childBox));
        }
    }
}

void RenderGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle)
        return;

    // FIXME: The following checks could be narrowed down if we kept track of which type of grid items we have:
    // - explicit grid size changes impact negative explicitely positioned and auto-placed grid items.
    // - named grid lines only impact grid items with named grid lines.
    // - auto-flow changes only impacts auto-placed children.

    if (explicitGridDidResize(oldStyle)
        || namedGridLinesDefinitionDidChange(oldStyle)
        || oldStyle->gridAutoFlow() != style()->gridAutoFlow())
        dirtyGrid();
}

bool RenderGrid::explicitGridDidResize(const RenderStyle* oldStyle) const
{
    return oldStyle->gridTemplateColumns().size() != style()->gridTemplateColumns().size()
        || oldStyle->gridTemplateRows().size() != style()->gridTemplateRows().size();
}

bool RenderGrid::namedGridLinesDefinitionDidChange(const RenderStyle* oldStyle) const
{
    return oldStyle->namedGridRowLines() != style()->namedGridRowLines()
        || oldStyle->namedGridColumnLines() != style()->namedGridColumnLines();
}

void RenderGrid::layoutBlock(bool relayoutChildren)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // FIXME: Much of this method is boiler plate that matches RenderBox::layoutBlock and Render*FlexibleBox::layoutBlock.
    // It would be nice to refactor some of the duplicate code.
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(*this, locationOffset());

    LayoutSize previousSize = size();

    setLogicalHeight(0);
    updateLogicalWidth();

    layoutGridItems();

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    computeRegionRangeForBlock(flowThreadContainingBlock());

    computeOverflow(oldClientAfterEdge);

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->scrollableArea()->updateAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const_cast<RenderGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    LayoutUnit availableLogicalSpace = 0;
    const_cast<RenderGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, availableLogicalSpace);

    for (size_t i = 0; i < sizingData.columnTracks.size(); ++i) {
        LayoutUnit minTrackBreadth = sizingData.columnTracks[i].m_usedBreadth;
        LayoutUnit maxTrackBreadth = sizingData.columnTracks[i].m_maxBreadth;
        maxTrackBreadth = std::max(maxTrackBreadth, minTrackBreadth);

        minLogicalWidth += minTrackBreadth;
        maxLogicalWidth += maxTrackBreadth;

        // FIXME: This should add in the scrollbarWidth (e.g. see RenderFlexibleBox).
    }
}

void RenderGrid::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    // FIXME: We don't take our own logical width into account. Once we do, we need to make sure
    // we apply (and test the interaction with) min-width / max-width.

    computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    LayoutUnit borderAndPaddingInInlineDirection = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPaddingInInlineDirection;
    m_maxPreferredLogicalWidth += borderAndPaddingInInlineDirection;

    clearPreferredLogicalWidthsDirty();
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    LayoutUnit availableLogicalSpace = (direction == ForColumns) ? availableLogicalWidth() : availableLogicalHeight(IncludeMarginBorderPadding);
    computeUsedBreadthOfGridTracks(direction, sizingData, availableLogicalSpace);
}

bool RenderGrid::gridElementIsShrinkToFit()
{
    return isFloatingOrOutOfFlowPositioned();
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<size_t> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    // 1. Initialize per Grid track variables.
    for (size_t i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.m_usedBreadth = computeUsedBreadthOfMinLength(direction, minTrackBreadth);
        track.m_maxBreadth = computeUsedBreadthOfMaxLength(direction, maxTrackBreadth, track.m_usedBreadth);

        track.m_maxBreadth = std::max(track.m_maxBreadth, track.m_usedBreadth);

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData, availableLogicalSpace);

    for (size_t i = 0; i < tracks.size(); ++i) {
        ASSERT(tracks[i].m_maxBreadth != infinity);
        availableLogicalSpace -= tracks[i].m_usedBreadth;
    }

    const bool hasUndefinedRemainingSpace = (direction == ForRows) ? style()->logicalHeight().isAuto() : gridElementIsShrinkToFit();

    if (!hasUndefinedRemainingSpace && availableLogicalSpace <= 0)
        return;

    // 3. Grow all Grid tracks in GridTracks from their UsedBreadth up to their MaxBreadth value until
    // availableLogicalSpace (RemainingSpace in the specs) is exhausted.
    const size_t tracksSize = tracks.size();
    if (!hasUndefinedRemainingSpace) {
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (size_t i = 0; i < tracksSize; ++i)
            tracksForDistribution[i] = tracks.data() + i;

        distributeSpaceToTracks(tracksForDistribution, 0, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth, sizingData, availableLogicalSpace);
    } else {
        for (size_t i = 0; i < tracksSize; ++i)
            tracks[i].m_usedBreadth = tracks[i].m_maxBreadth;
    }

    if (flexibleSizedTracksIndex.isEmpty())
        return;

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double normalizedFractionBreadth = 0;
    if (!hasUndefinedRemainingSpace) {
        normalizedFractionBreadth = computeNormalizedFractionBreadth(tracks, GridSpan(0, tracks.size() - 1), direction, availableLogicalSpace);
    } else {
        for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            const size_t trackIndex = flexibleSizedTracksIndex[i];
            const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
            normalizedFractionBreadth = std::max(normalizedFractionBreadth, tracks[trackIndex].m_usedBreadth / trackSize.maxTrackBreadth().flex());
        }

        for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (RenderBox* gridItem = iterator.nextGridItem()) {
                const GridCoordinate coordinate = cachedGridCoordinate(gridItem);
                const GridSpan span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

                // Do not include already processed items.
                if (i > 0 && span.initialPositionIndex <= flexibleSizedTracksIndex[i - 1])
                    continue;

                double itemNormalizedFlexBreadth = computeNormalizedFractionBreadth(tracks, span, direction, maxContentForChild(gridItem, direction, sizingData.columnTracks));
                normalizedFractionBreadth = std::max(normalizedFractionBreadth, itemNormalizedFlexBreadth);
            }
        }
    }

    for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
        const size_t trackIndex = flexibleSizedTracksIndex[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);

        tracks[trackIndex].m_usedBreadth = std::max<LayoutUnit>(tracks[trackIndex].m_usedBreadth, normalizedFractionBreadth * trackSize.maxTrackBreadth().flex());
    }
}

LayoutUnit RenderGrid::computeUsedBreadthOfMinLength(GridTrackSizingDirection direction, const GridLength& gridLength) const
{
    if (gridLength.isFlex())
        return 0;

    const Length& trackLength = gridLength.length();
    ASSERT(!trackLength.isAuto());
    if (trackLength.isSpecified())
        return computeUsedBreadthOfSpecifiedLength(direction, trackLength);

    ASSERT(trackLength.isMinContent() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit RenderGrid::computeUsedBreadthOfMaxLength(GridTrackSizingDirection direction, const GridLength& gridLength, LayoutUnit usedBreadth) const
{
    if (gridLength.isFlex())
        return usedBreadth;

    const Length& trackLength = gridLength.length();
    ASSERT(!trackLength.isAuto());
    if (trackLength.isSpecified()) {
        LayoutUnit computedBreadth = computeUsedBreadthOfSpecifiedLength(direction, trackLength);
        ASSERT(computedBreadth != infinity);
        return computedBreadth;
    }

    ASSERT(trackLength.isMinContent() || trackLength.isMaxContent());
    return infinity;
}

LayoutUnit RenderGrid::computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection direction, const Length& trackLength) const
{
    ASSERT(trackLength.isSpecified());
    // FIXME: The -1 here should be replaced by whatever the intrinsic height of the grid is.
    return valueForLength(trackLength, direction == ForColumns ? logicalWidth() : computeContentLogicalHeight(style()->logicalHeight(), -1));
}

static bool sortByGridNormalizedFlexValue(const GridTrackForNormalization& track1, const GridTrackForNormalization& track2)
{
    return track1.m_normalizedFlexValue < track2.m_normalizedFlexValue;
}

double RenderGrid::computeNormalizedFractionBreadth(Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit availableLogicalSpace) const
{
    // |availableLogicalSpace| already accounts for the used breadths so no need to remove it here.

    Vector<GridTrackForNormalization> tracksForNormalization;
    for (size_t i = tracksSpan.initialPositionIndex; i <= tracksSpan.finalPositionIndex; ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        if (!trackSize.maxTrackBreadth().isFlex())
            continue;

        tracksForNormalization.append(GridTrackForNormalization(tracks[i], trackSize.maxTrackBreadth().flex()));
    }

    // The function is not called if we don't have <flex> grid tracks
    ASSERT(!tracksForNormalization.isEmpty());

    std::sort(tracksForNormalization.begin(), tracksForNormalization.end(), sortByGridNormalizedFlexValue);

    // These values work together: as we walk over our grid tracks, we increase fractionValueBasedOnGridItemsRatio
    // to match a grid track's usedBreadth to <flex> ratio until the total fractions sized grid tracks wouldn't
    // fit into availableLogicalSpaceIgnoringFractionTracks.
    double accumulatedFractions = 0;
    LayoutUnit fractionValueBasedOnGridItemsRatio = 0;
    LayoutUnit availableLogicalSpaceIgnoringFractionTracks = availableLogicalSpace;

    for (size_t i = 0; i < tracksForNormalization.size(); ++i) {
        const GridTrackForNormalization& track = tracksForNormalization[i];
        if (track.m_normalizedFlexValue > fractionValueBasedOnGridItemsRatio) {
            // If the normalized flex value (we ordered |tracksForNormalization| by increasing normalized flex value)
            // will make us overflow our container, then stop. We have the previous step's ratio is the best fit.
            if (track.m_normalizedFlexValue * accumulatedFractions > availableLogicalSpaceIgnoringFractionTracks)
                break;

            fractionValueBasedOnGridItemsRatio = track.m_normalizedFlexValue;
        }

        accumulatedFractions += track.m_flex;
        // This item was processed so we re-add its used breadth to the available space to accurately count the remaining space.
        availableLogicalSpaceIgnoringFractionTracks += track.m_track->m_usedBreadth;
    }

    return availableLogicalSpaceIgnoringFractionTracks / accumulatedFractions;
}

const GridTrackSize& RenderGrid::gridTrackSize(GridTrackSizingDirection direction, size_t i) const
{
    const Vector<GridTrackSize>& trackStyles = (direction == ForColumns) ? style()->gridTemplateColumns() : style()->gridTemplateRows();
    if (i >= trackStyles.size())
        return (direction == ForColumns) ? style()->gridAutoColumns() : style()->gridAutoRows();

    const GridTrackSize& trackSize = trackStyles[i];
    // If the logical width/height of the grid container is indefinite, percentage values are treated as <auto>.
    if (trackSize.isPercentage()) {
        Length logicalSize = direction == ForColumns ? style()->logicalWidth() : style()->logicalHeight();
        if (logicalSize.isIntrinsicOrAuto()) {
            DEFINE_STATIC_LOCAL(GridTrackSize, autoTrackSize, (Length(Auto)));
            return autoTrackSize;
        }
    }

    return trackSize;
}

size_t RenderGrid::explicitGridColumnCount() const
{
    return style()->gridTemplateColumns().size();
}

size_t RenderGrid::explicitGridRowCount() const
{
    return style()->gridTemplateRows().size();
}

size_t RenderGrid::explicitGridSizeForSide(GridPositionSide side) const
{
    return (side == ColumnStartSide || side == ColumnEndSide) ? explicitGridColumnCount() : explicitGridRowCount();
}

LayoutUnit RenderGrid::logicalContentHeightForChild(RenderBox* child, Vector<GridTrack>& columnTracks)
{
    SubtreeLayoutScope layoutScope(child);
    LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
    LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, columnTracks);
    if (child->style()->logicalHeight().isPercent() || oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth)
        layoutScope.setNeedsLayout(child);

    child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
    // If |child| has a percentage logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the override logical height to -1 (no possible resolution).
    child->setOverrideContainingBlockContentLogicalHeight(-1);
    child->layoutIfNeeded();
    return child->logicalHeight();
}

LayoutUnit RenderGrid::minContentForChild(RenderBox* child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return 0;

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child->minPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

LayoutUnit RenderGrid::maxContentForChild(RenderBox* child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

void RenderGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    // FIXME: Split the grid tracks into groups that doesn't overlap a <flex> grid track (crbug.com/235258).

    // FIXME: Per step 2 of the specification, we should order the grid items by increasing span.

    for (size_t i = 0; i < sizingData.contentSizedTracksIndex.size(); ++i) {
        GridIterator iterator(m_grid, direction, sizingData.contentSizedTracksIndex[i]);
        while (RenderBox* gridItem = iterator.nextGridItem()) {
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMinOrMaxContentMinTrackBreadth, &RenderGrid::minContentForChild, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth);
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMaxContentMinTrackBreadth, &RenderGrid::maxContentForChild, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth);
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMinOrMaxContentMaxTrackBreadth, &RenderGrid::minContentForChild, &GridTrack::maxBreadthIfNotInfinite, &GridTrack::growMaxBreadth);
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMaxContentMaxTrackBreadth, &RenderGrid::maxContentForChild, &GridTrack::maxBreadthIfNotInfinite, &GridTrack::growMaxBreadth);
        }

        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[i] : sizingData.rowTracks[i];
        if (track.m_maxBreadth == infinity)
            track.m_maxBreadth = track.m_usedBreadth;
    }
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, RenderBox* gridItem, FilterFunction filterFunction, SizingFunction sizingFunction, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction)
{
    const GridCoordinate coordinate = cachedGridCoordinate(gridItem);
    const size_t initialTrackIndex = (direction == ForColumns) ? coordinate.columns.initialPositionIndex : coordinate.rows.initialPositionIndex;
    const size_t finalTrackIndex = (direction == ForColumns) ? coordinate.columns.finalPositionIndex : coordinate.rows.finalPositionIndex;

    sizingData.filteredTracks.shrink(0);
    for (size_t trackIndex = initialTrackIndex; trackIndex <= finalTrackIndex; ++trackIndex) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
        if (!(trackSize.*filterFunction)())
            continue;

        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        sizingData.filteredTracks.append(&track);
    }

    if (sizingData.filteredTracks.isEmpty())
        return;

    LayoutUnit additionalBreadthSpace = (this->*sizingFunction)(gridItem, direction, sizingData.columnTracks);
    for (size_t trackIndexForSpace = initialTrackIndex; trackIndexForSpace <= finalTrackIndex; ++trackIndexForSpace) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndexForSpace] : sizingData.rowTracks[trackIndexForSpace];
        additionalBreadthSpace -= (track.*trackGetter)();
    }

    // FIXME: We should pass different values for |tracksForGrowthAboveMaxBreadth|.
    distributeSpaceToTracks(sizingData.filteredTracks, &sizingData.filteredTracks, trackGetter, trackGrowthFunction, sizingData, additionalBreadthSpace);
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    return (track1->m_maxBreadth - track1->m_usedBreadth) < (track2->m_maxBreadth - track2->m_usedBreadth);
}

void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* tracksForGrowthAboveMaxBreadth, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

    size_t tracksSize = tracks.size();
    sizingData.distributeTrackVector.resize(tracksSize);

    for (size_t i = 0; i < tracksSize; ++i) {
        GridTrack& track = *tracks[i];
        LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
        LayoutUnit trackBreadth = (tracks[i]->*trackGetter)();
        LayoutUnit growthShare = std::min(availableLogicalSpaceShare, track.m_maxBreadth - trackBreadth);
        sizingData.distributeTrackVector[i] = trackBreadth;
        // We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function.
        if (growthShare > 0) {
            sizingData.distributeTrackVector[i] += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    if (availableLogicalSpace > 0 && tracksForGrowthAboveMaxBreadth) {
        tracksSize = tracksForGrowthAboveMaxBreadth->size();
        for (size_t i = 0; i < tracksSize; ++i) {
            LayoutUnit growthShare = availableLogicalSpace / (tracksSize - i);
            sizingData.distributeTrackVector[i] += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    for (size_t i = 0; i < tracksSize; ++i) {
        LayoutUnit growth = sizingData.distributeTrackVector[i] - (tracks[i]->*trackGetter)();
        if (growth >= 0)
            (tracks[i]->*trackGrowthFunction)(growth);
    }
}

#ifndef NDEBUG
bool RenderGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, const Vector<GridTrack>& tracks)
{
    for (size_t i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(direction, minTrackBreadth) > tracks[i].m_usedBreadth)
            return false;
    }
    return true;
}
#endif

void RenderGrid::growGrid(GridTrackSizingDirection direction, size_t maximumPositionIndex)
{
    if (direction == ForColumns) {
        ASSERT(maximumPositionIndex >= m_grid[0].size());
        for (size_t row = 0; row < m_grid.size(); ++row)
            m_grid[row].grow(maximumPositionIndex + 1);
    } else {
        ASSERT(maximumPositionIndex >= m_grid.size());
        const size_t oldRowSize = m_grid.size();
        m_grid.grow(maximumPositionIndex + 1);
        for (size_t row = oldRowSize; row < m_grid.size(); ++row)
            m_grid[row].grow(m_grid[0].size());
    }
}

void RenderGrid::insertItemIntoGrid(RenderBox* child, const GridCoordinate& coordinate)
{
    for (size_t row = coordinate.rows.initialPositionIndex; row <= coordinate.rows.finalPositionIndex; ++row) {
        for (size_t column = coordinate.columns.initialPositionIndex; column <= coordinate.columns.finalPositionIndex; ++column)
            m_grid[row][column].append(child);
    }

    m_gridItemCoordinate.set(child, coordinate);
}

void RenderGrid::insertItemIntoGrid(RenderBox* child, size_t rowTrack, size_t columnTrack)
{
    const GridSpan& rowSpan = resolveGridPositionsFromAutoPlacementPosition(child, ForRows, rowTrack);
    const GridSpan& columnSpan = resolveGridPositionsFromAutoPlacementPosition(child, ForColumns, columnTrack);
    insertItemIntoGrid(child, GridCoordinate(rowSpan, columnSpan));
}

void RenderGrid::placeItemsOnGrid()
{
    if (!gridIsDirty())
        return;

    ASSERT(m_gridItemCoordinate.isEmpty());

    populateExplicitGridAndOrderIterator();

    // We clear the dirty bit here as the grid sizes have been updated, this means
    // that we can safely call gridRowCount() / gridColumnCount().
    m_gridIsDirty = false;

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    GridAutoFlow autoFlow = style()->gridAutoFlow();
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        // FIXME: We never re-resolve positions if the grid is grown during auto-placement which may lead auto / <integer>
        // positions to not match the author's intent. The specification is unclear on what should be done in this case.
        OwnPtr<GridSpan> rowPositions = resolveGridPositionsFromStyle(child, ForRows);
        OwnPtr<GridSpan> columnPositions = resolveGridPositionsFromStyle(child, ForColumns);
        if (!rowPositions || !columnPositions) {
            GridSpan* majorAxisPositions = (autoPlacementMajorAxisDirection() == ForColumns) ? columnPositions.get() : rowPositions.get();
            if (!majorAxisPositions)
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        insertItemIntoGrid(child, GridCoordinate(*rowPositions, *columnPositions));
    }

    ASSERT(gridRowCount() >= style()->gridTemplateRows().size());
    ASSERT(gridColumnCount() >= style()->gridTemplateColumns().size());

    if (autoFlow == AutoFlowNone) {
        // If we did collect some grid items, they won't be placed thus never laid out.
        ASSERT(!autoMajorAxisAutoGridItems.size());
        ASSERT(!specifiedMajorAxisAutoGridItems.size());
        return;
    }

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);

    m_grid.shrinkToFit();
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(m_orderIterator);

    size_t maximumRowIndex = std::max<size_t>(1, explicitGridRowCount());
    size_t maximumColumnIndex = std::max<size_t>(1, explicitGridColumnCount());

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        populator.collectChild(child);

        // This function bypasses the cache (cachedGridCoordinate()) as it is used to build it.
        OwnPtr<GridSpan> rowPositions = resolveGridPositionsFromStyle(child, ForRows);
        OwnPtr<GridSpan> columnPositions = resolveGridPositionsFromStyle(child, ForColumns);

        // |positions| is 0 if we need to run the auto-placement algorithm. Our estimation ignores
        // this case as the auto-placement algorithm will grow the grid as needed.
        if (rowPositions)
            maximumRowIndex = std::max(maximumRowIndex, rowPositions->finalPositionIndex + 1);
        if (columnPositions)
            maximumColumnIndex = std::max(maximumColumnIndex, columnPositions->finalPositionIndex + 1);
    }

    m_grid.grow(maximumRowIndex);
    for (size_t i = 0; i < m_grid.size(); ++i)
        m_grid[i].grow(maximumColumnIndex);
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    for (size_t i = 0; i < autoGridItems.size(); ++i) {
        OwnPtr<GridSpan> majorAxisPositions = resolveGridPositionsFromStyle(autoGridItems[i], autoPlacementMajorAxisDirection());
        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions->initialPositionIndex);
        if (OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
            insertItemIntoGrid(autoGridItems[i], emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
            continue;
        }

        growGrid(autoPlacementMinorAxisDirection(), autoPlacementMinorAxisDirection() == ForColumns ? m_grid[0].size() : m_grid.size());
        OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea();
        ASSERT(emptyGridArea);
        insertItemIntoGrid(autoGridItems[i], emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
    }
}

void RenderGrid::placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    for (size_t i = 0; i < autoGridItems.size(); ++i)
        placeAutoMajorAxisItemOnGrid(autoGridItems[i]);
}

void RenderGrid::placeAutoMajorAxisItemOnGrid(RenderBox* gridItem)
{
    OwnPtr<GridSpan> minorAxisPositions = resolveGridPositionsFromStyle(gridItem, autoPlacementMinorAxisDirection());
    ASSERT(!resolveGridPositionsFromStyle(gridItem, autoPlacementMajorAxisDirection()));
    size_t minorAxisIndex = 0;
    if (minorAxisPositions) {
        minorAxisIndex = minorAxisPositions->initialPositionIndex;
        GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisIndex);
        if (OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
            insertItemIntoGrid(gridItem, emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
            return;
        }
    } else {
        const size_t endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
        for (size_t majorAxisIndex = 0; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex);
            if (OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
                insertItemIntoGrid(gridItem, emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
                return;
            }
        }
    }

    // We didn't find an empty grid area so we need to create an extra major axis line and insert our gridItem in it.
    const size_t columnIndex = (autoPlacementMajorAxisDirection() == ForColumns) ? m_grid[0].size() : minorAxisIndex;
    const size_t rowIndex = (autoPlacementMajorAxisDirection() == ForColumns) ? minorAxisIndex : m_grid.size();
    growGrid(autoPlacementMajorAxisDirection(), autoPlacementMajorAxisDirection() == ForColumns ? m_grid[0].size() : m_grid.size());
    insertItemIntoGrid(gridItem, rowIndex, columnIndex);
}

GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    GridAutoFlow flow = style()->gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? ForColumns : ForRows;
}

GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    GridAutoFlow flow = style()->gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? ForRows : ForColumns;
}

void RenderGrid::dirtyGrid()
{
    m_grid.resize(0);
    m_gridItemCoordinate.clear();
    m_gridIsDirty = true;
    m_gridItemsOverflowingGridArea.resize(0);
}

void RenderGrid::layoutGridItems()
{
    placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    computeUsedBreadthOfGridTracks(ForColumns, sizingData);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, sizingData.columnTracks));
    computeUsedBreadthOfGridTracks(ForRows, sizingData);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData.rowTracks));

    populateGridPositions(sizingData);
    m_gridItemsOverflowingGridArea.resize(0);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        LayoutUnit oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, sizingData.columnTracks);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChild(child, ForRows, sizingData.rowTracks);

        SubtreeLayoutScope layoutScope(child);
        if (oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth || (oldOverrideContainingBlockContentLogicalHeight != overrideContainingBlockContentLogicalHeight && child->hasRelativeLogicalHeight()))
            layoutScope.setNeedsLayout(child);

        child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
        child->setOverrideContainingBlockContentLogicalHeight(overrideContainingBlockContentLogicalHeight);

        LayoutRect oldChildRect = child->frameRect();

        // FIXME: Grid items should stretch to fill their cells. Once we
        // implement grid-{column,row}-align, we can also shrink to fit. For
        // now, just size as if we were a regular child.
        child->layoutIfNeeded();

#ifndef NDEBUG
        const GridCoordinate& coordinate = cachedGridCoordinate(child);
        ASSERT(coordinate.columns.initialPositionIndex < sizingData.columnTracks.size());
        ASSERT(coordinate.rows.initialPositionIndex < sizingData.rowTracks.size());
#endif
        child->setLogicalLocation(findChildLogicalPosition(child));

        // Keep track of children overflowing their grid area as we might need to paint them even if the grid-area is
        // not visible
        if (child->logicalHeight() > overrideContainingBlockContentLogicalHeight
            || child->logicalWidth() > overrideContainingBlockContentLogicalWidth)
            m_gridItemsOverflowingGridArea.append(child);

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRect);
    }

    for (size_t i = 0; i < sizingData.rowTracks.size(); ++i)
        setLogicalHeight(logicalHeight() + sizingData.rowTracks[i].m_usedBreadth);

    // Min / max logical height is handled by the call to updateLogicalHeight in layoutBlock.

    setLogicalHeight(logicalHeight() + borderAndPaddingLogicalHeight());
}

GridCoordinate RenderGrid::cachedGridCoordinate(const RenderBox* gridItem) const
{
    ASSERT(m_gridItemCoordinate.contains(gridItem));
    return m_gridItemCoordinate.get(gridItem);
}

GridSpan RenderGrid::resolveGridPositionsFromAutoPlacementPosition(const RenderBox*, GridTrackSizingDirection, size_t initialPosition) const
{
    // FIXME: We don't support spanning with auto positions yet. Once we do, this is wrong. Also we should make
    // sure the grid can accomodate the new item as we only grow 1 position in a given direction.
    return GridSpan(initialPosition, initialPosition);
}

PassOwnPtr<GridSpan> RenderGrid::resolveGridPositionsFromStyle(const RenderBox* gridItem, GridTrackSizingDirection direction) const
{
    GridPosition initialPosition = (direction == ForColumns) ? gridItem->style()->gridColumnStart() : gridItem->style()->gridRowStart();
    const GridPositionSide initialPositionSide = (direction == ForColumns) ? ColumnStartSide : RowStartSide;
    GridPosition finalPosition = (direction == ForColumns) ? gridItem->style()->gridColumnEnd() : gridItem->style()->gridRowEnd();
    const GridPositionSide finalPositionSide = (direction == ForColumns) ? ColumnEndSide : RowEndSide;

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    if (initialPosition.isNamedGridArea() && !style()->namedGridArea().contains(initialPosition.namedGridLine()))
        initialPosition.setAutoPosition();

    if (finalPosition.isNamedGridArea() && !style()->namedGridArea().contains(finalPosition.namedGridLine()))
        finalPosition.setAutoPosition();

    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        if (style()->gridAutoFlow() == AutoFlowNone)
            return adoptPtr(new GridSpan(0, 0));

        // We can't get our grid positions without running the auto placement algorithm.
        return nullptr;
    }

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        const size_t finalResolvedPosition = resolveGridPositionFromStyle(finalPosition, finalPositionSide);
        return resolveGridPositionAgainstOppositePosition(finalResolvedPosition, initialPosition, initialPositionSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        const size_t initialResolvedPosition = resolveGridPositionFromStyle(initialPosition, initialPositionSide);
        return resolveGridPositionAgainstOppositePosition(initialResolvedPosition, finalPosition, finalPositionSide);
    }

    size_t resolvedInitialPosition = resolveGridPositionFromStyle(initialPosition, initialPositionSide);
    size_t resolvedFinalPosition = resolveGridPositionFromStyle(finalPosition, finalPositionSide);

    // If 'grid-after' specifies a line at or before that specified by 'grid-before', it computes to 'span 1'.
    if (resolvedFinalPosition < resolvedInitialPosition)
        resolvedFinalPosition = resolvedInitialPosition;

    return adoptPtr(new GridSpan(resolvedInitialPosition, resolvedFinalPosition));
}

size_t RenderGrid::resolveNamedGridLinePositionFromStyle(const GridPosition& position, GridPositionSide side) const
{
    ASSERT(!position.namedGridLine().isNull());

    const NamedGridLinesMap& gridLinesNames = (side == ColumnStartSide || side == ColumnEndSide) ? style()->namedGridColumnLines() : style()->namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    if (it == gridLinesNames.end()) {
        if (position.isPositive())
            return 0;
        const size_t lastLine = explicitGridSizeForSide(side);
        return GridPosition::adjustGridPositionForSide(lastLine, side);
    }

    size_t namedGridLineIndex;
    if (position.isPositive())
        namedGridLineIndex = std::min<size_t>(position.integerPosition(), it->value.size()) - 1;
    else
        namedGridLineIndex = std::max<int>(it->value.size() - abs(position.integerPosition()), 0);
    return GridPosition::adjustGridPositionForSide(it->value[namedGridLineIndex], side);
}

size_t RenderGrid::resolveGridPositionFromStyle(const GridPosition& position, GridPositionSide side) const
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return GridPosition::adjustGridPositionForSide(position.integerPosition() - 1, side);

        size_t resolvedPosition = abs(position.integerPosition()) - 1;
        const size_t endOfTrack = explicitGridSizeForSide(side);

        // Per http://lists.w3.org/Archives/Public/www-style/2013Mar/0589.html, we clamp negative value to the first line.
        if (endOfTrack < resolvedPosition)
            return 0;

        return GridPosition::adjustGridPositionForSide(endOfTrack - resolvedPosition, side);
    }
    case NamedGridAreaPosition:
    {
        NamedGridAreaMap::const_iterator it = style()->namedGridArea().find(position.namedGridLine());
        // Unknown grid area should have been computed to 'auto' by now.
        ASSERT_WITH_SECURITY_IMPLICATION(it != style()->namedGridArea().end());
        const GridCoordinate& gridAreaCoordinate = it->value;
        switch (side) {
        case ColumnStartSide:
            return gridAreaCoordinate.columns.initialPositionIndex;
        case ColumnEndSide:
            return gridAreaCoordinate.columns.finalPositionIndex;
        case RowStartSide:
            return gridAreaCoordinate.rows.initialPositionIndex;
        case RowEndSide:
            return gridAreaCoordinate.rows.finalPositionIndex;
        }
        ASSERT_NOT_REACHED();
        return 0;
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

PassOwnPtr<GridSpan> RenderGrid::resolveGridPositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition& position, GridPositionSide side) const
{
    if (position.isAuto())
        return GridSpan::create(resolvedOppositePosition, resolvedOppositePosition);

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, side);
    }

    return GridSpan::createWithSpanAgainstOpposite(resolvedOppositePosition, position, side);
}

PassOwnPtr<GridSpan> RenderGrid::resolveNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition& position, GridPositionSide side) const
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    const NamedGridLinesMap& gridLinesNames = (side == ColumnStartSide || side == ColumnEndSide) ? style()->namedGridColumnLines() : style()->namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());

    // If there is no named grid line of that name, we resolve the position to 'auto' (which is equivalent to 'span 1' in this case).
    // See http://lists.w3.org/Archives/Public/www-style/2013Jun/0394.html.
    if (it == gridLinesNames.end())
        return GridSpan::create(resolvedOppositePosition, resolvedOppositePosition);

    return GridSpan::createWithNamedSpanAgainstOpposite(resolvedOppositePosition, position, side, it->value);
}

LayoutUnit RenderGrid::gridAreaBreadthForChild(const RenderBox* child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    const GridSpan& span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    LayoutUnit gridAreaBreadth = 0;
    for (size_t trackIndex = span.initialPositionIndex; trackIndex <= span.finalPositionIndex; ++trackIndex)
        gridAreaBreadth += tracks[trackIndex].m_usedBreadth;
    return gridAreaBreadth;
}

void RenderGrid::populateGridPositions(const GridSizingData& sizingData)
{
    m_columnPositions.resize(sizingData.columnTracks.size() + 1);
    m_columnPositions[0] = borderAndPaddingStart();
    for (size_t i = 0; i < m_columnPositions.size() - 1; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + sizingData.columnTracks[i].m_usedBreadth;

    m_rowPositions.resize(sizingData.rowTracks.size() + 1);
    m_rowPositions[0] = borderAndPaddingBefore();
    for (size_t i = 0; i < m_rowPositions.size() - 1; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + sizingData.rowTracks[i].m_usedBreadth;
}

LayoutUnit RenderGrid::startOfColumnForChild(const RenderBox* child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.initialPositionIndex];
    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    // FIXME: This should account for the grid item's <overflow-position>.
    return startOfColumn + marginStartForChild(child);
}

LayoutUnit RenderGrid::endOfColumnForChild(const RenderBox* child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.initialPositionIndex];
    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    LayoutUnit columnPosition = startOfColumn + marginStartForChild(child);

    LayoutUnit endOfColumn = m_columnPositions[coordinate.columns.finalPositionIndex + 1];
    // FIXME: This should account for the grid item's <overflow-position>.
    return columnPosition + std::max<LayoutUnit>(0, endOfColumn - m_columnPositions[coordinate.columns.initialPositionIndex] - child->logicalWidth());
}

LayoutUnit RenderGrid::columnPositionAlignedWithGridContainerStart(const RenderBox* child) const
{
    if (style()->isLeftToRightDirection())
        return startOfColumnForChild(child);

    return endOfColumnForChild(child);
}

LayoutUnit RenderGrid::columnPositionAlignedWithGridContainerEnd(const RenderBox* child) const
{
    if (!style()->isLeftToRightDirection())
        return startOfColumnForChild(child);

    return endOfColumnForChild(child);
}

LayoutUnit RenderGrid::centeredColumnPositionForChild(const RenderBox* child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.initialPositionIndex];
    LayoutUnit endOfColumn = m_columnPositions[coordinate.columns.finalPositionIndex + 1];
    LayoutUnit columnPosition = startOfColumn + marginStartForChild(child);
    return columnPosition + std::max<LayoutUnit>(0, endOfColumn - startOfColumn - child->logicalWidth()) / 2;
}

LayoutUnit RenderGrid::columnPositionForChild(const RenderBox* child) const
{
    ItemPosition childJustifySelf = child->style()->justifySelf();
    switch (childJustifySelf) {
    case ItemPositionSelfStart:
        // self-start is based on the child's direction. That's why we need to check against the grid container's direction.
        if (child->style()->direction() != style()->direction())
            return columnPositionAlignedWithGridContainerEnd(child);

        return columnPositionAlignedWithGridContainerStart(child);
    case ItemPositionSelfEnd:
        // self-end is based on the child's direction. That's why we need to check against the grid container's direction.
        if (child->style()->direction() != style()->direction())
            return columnPositionAlignedWithGridContainerStart(child);

        return columnPositionAlignedWithGridContainerEnd(child);

    case ItemPositionFlexStart:
    case ItemPositionFlexEnd:
        // Only used in flex layout, for other layout, it's equivalent to 'start'.
        return columnPositionAlignedWithGridContainerStart(child);

    case ItemPositionLeft:
        // If the property's axis is not parallel with the inline axis, this is equivalent to ‘start’.
        if (!isHorizontalWritingMode())
            return columnPositionAlignedWithGridContainerStart(child);

        if (style()->isLeftToRightDirection())
            return columnPositionAlignedWithGridContainerStart(child);

        return columnPositionAlignedWithGridContainerEnd(child);
    case ItemPositionRight:
        // If the property's axis is not parallel with the inline axis, this is equivalent to ‘start’.
        if (!isHorizontalWritingMode())
            return columnPositionAlignedWithGridContainerStart(child);

        if (style()->isLeftToRightDirection())
            return columnPositionAlignedWithGridContainerEnd(child);

        return columnPositionAlignedWithGridContainerStart(child);

    case ItemPositionCenter:
        return centeredColumnPositionForChild(child);
    case ItemPositionStart:
        return columnPositionAlignedWithGridContainerStart(child);
    case ItemPositionEnd:
        return columnPositionAlignedWithGridContainerEnd(child);

    case ItemPositionAuto:
    case ItemPositionStretch:
    case ItemPositionBaseline:
        // FIXME: Implement the previous values. For now, we always start align the child.
        return startOfColumnForChild(child);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderGrid::rowPositionForChild(const RenderBox* child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);

    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    LayoutUnit startOfRow = m_rowPositions[coordinate.rows.initialPositionIndex];
    LayoutUnit rowPosition = startOfRow + marginBeforeForChild(child);

    // FIXME: This function should account for 'align-self'.

    return rowPosition;
}

LayoutPoint RenderGrid::findChildLogicalPosition(const RenderBox* child) const
{
    return LayoutPoint(columnPositionForChild(child), rowPositionForChild(child));
}

static GridSpan dirtiedGridAreas(const Vector<LayoutUnit>& coordinates, LayoutUnit start, LayoutUnit end)
{
    // This function does a binary search over the coordinates.
    // FIXME: This doesn't work with grid items overflowing their grid areas and should be tested & fixed.

    size_t startGridAreaIndex = std::upper_bound(coordinates.begin(), coordinates.end() - 1, start) - coordinates.begin();
    if (startGridAreaIndex > 0)
        --startGridAreaIndex;

    size_t endGridAreaIndex = std::upper_bound(coordinates.begin() + startGridAreaIndex, coordinates.end() - 1, end) - coordinates.begin();
    return GridSpan(startGridAreaIndex, endGridAreaIndex);
}

class GridCoordinateSorter {
public:
    GridCoordinateSorter(RenderGrid* renderer) : m_renderer(renderer) { }

    bool operator()(const RenderBox* firstItem, const RenderBox* secondItem) const
    {
        GridCoordinate first = m_renderer->cachedGridCoordinate(firstItem);
        GridCoordinate second = m_renderer->cachedGridCoordinate(secondItem);

        if (first.rows.initialPositionIndex < second.rows.initialPositionIndex)
            return true;
        if (first.rows.initialPositionIndex > second.rows.initialPositionIndex)
            return false;
        return first.columns.finalPositionIndex < second.columns.finalPositionIndex;
    }
private:
    RenderGrid* m_renderer;
};

static inline bool isInSameRowBeforeDirtyArea(const GridCoordinate& coordinate, size_t row, const GridSpan& dirtiedColumns)
{
    return coordinate.rows.initialPositionIndex == row && coordinate.columns.initialPositionIndex < dirtiedColumns.initialPositionIndex;
}

static inline bool isInSameRowAfterDirtyArea(const GridCoordinate& coordinate, size_t row, const GridSpan& dirtiedColumns)
{
    return coordinate.rows.initialPositionIndex == row && coordinate.columns.initialPositionIndex >= dirtiedColumns.finalPositionIndex;
}

static inline bool rowIsBeforeDirtyArea(const GridCoordinate& coordinate, const GridSpan& dirtiedRows)
{
    return coordinate.rows.initialPositionIndex < dirtiedRows.initialPositionIndex;
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!gridIsDirty());

    LayoutRect localRepaintRect = paintInfo.rect;
    localRepaintRect.moveBy(-paintOffset);

    GridSpan dirtiedColumns = dirtiedGridAreas(m_columnPositions, localRepaintRect.x(), localRepaintRect.maxX());
    GridSpan dirtiedRows = dirtiedGridAreas(m_rowPositions, localRepaintRect.y(), localRepaintRect.maxY());

    // Sort the overflowing grid items according to their positions in the grid. We collect items during the layout
    // process following DOM's order but we have to paint following grid's.
    std::stable_sort(m_gridItemsOverflowingGridArea.begin(), m_gridItemsOverflowingGridArea.end(), GridCoordinateSorter(this));

    OrderIterator paintIterator(this);
    {
        OrderIteratorPopulator populator(paintIterator);
        Vector<RenderBox*>::const_iterator overflowIterator = m_gridItemsOverflowingGridArea.begin();
        Vector<RenderBox*>::const_iterator end = m_gridItemsOverflowingGridArea.end();

        for (; overflowIterator != end && rowIsBeforeDirtyArea(cachedGridCoordinate(*overflowIterator), dirtiedRows); ++overflowIterator) {
            if ((*overflowIterator)->frameRect().intersects(localRepaintRect))
                populator.storeChild(*overflowIterator);
        }

        for (size_t row = dirtiedRows.initialPositionIndex; row < dirtiedRows.finalPositionIndex; ++row) {

            for (; overflowIterator != end && isInSameRowBeforeDirtyArea(cachedGridCoordinate(*overflowIterator), row, dirtiedColumns); ++overflowIterator) {
                if ((*overflowIterator)->frameRect().intersects(localRepaintRect))
                    populator.storeChild(*overflowIterator);
            }

            for (size_t column = dirtiedColumns.initialPositionIndex; column < dirtiedColumns.finalPositionIndex; ++column) {
                const Vector<RenderBox*, 1>& children = m_grid[row][column];
                // FIXME: If we start adding spanning children in all grid areas they span, this
                // would make us paint them several times, which is wrong!
                for (size_t j = 0; j < children.size(); ++j) {
                    populator.storeChild(children[j]);
                    // Do not paint overflowing grid items twice.
                    if (overflowIterator != end && *overflowIterator == children[j])
                        ++overflowIterator;
                }
            }

            for (; overflowIterator != end && isInSameRowAfterDirtyArea(cachedGridCoordinate(*overflowIterator), row, dirtiedColumns); ++overflowIterator) {
                if ((*overflowIterator)->frameRect().intersects(localRepaintRect))
                    populator.storeChild(*overflowIterator);
            }
        }

        for (; overflowIterator != end; ++overflowIterator) {
            if ((*overflowIterator)->frameRect().intersects(localRepaintRect))
                populator.storeChild(*overflowIterator);
        }
    }

    for (RenderBox* child = paintIterator.first(); child; child = paintIterator.next())
        paintChild(child, paintInfo, paintOffset);
}

const char* RenderGrid::renderName() const
{
    if (isFloating())
        return "RenderGrid (floating)";
    if (isOutOfFlowPositioned())
        return "RenderGrid (positioned)";
    if (isAnonymous())
        return "RenderGrid (generated)";
    if (isRelPositioned())
        return "RenderGrid (relative positioned)";
    return "RenderGrid";
}

} // namespace WebCore
