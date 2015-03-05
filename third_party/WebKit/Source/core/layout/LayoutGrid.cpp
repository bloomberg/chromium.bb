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
#include "core/layout/LayoutGrid.h"

#include "core/layout/Layer.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/style/GridCoordinate.h"
#include "core/layout/style/LayoutStyle.h"
#include "core/paint/GridPainter.h"
#include "platform/LengthFunctions.h"

namespace blink {

static const int infinity = -1;

class GridTrack {
public:
    GridTrack()
        : m_baseSize(0)
        , m_growthLimit(0)
        , m_plannedIncrease(0)
    {
    }

    const LayoutUnit& baseSize() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return m_baseSize;
    }

    const LayoutUnit& growthLimit() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return m_growthLimit;
    }

    void setBaseSize(LayoutUnit baseSize)
    {
        m_baseSize = baseSize;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void setGrowthLimit(LayoutUnit growthLimit)
    {
        m_growthLimit = growthLimit;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void growBaseSize(LayoutUnit growth)
    {
        ASSERT(growth >= 0);
        m_baseSize += growth;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void growGrowthLimit(LayoutUnit growth)
    {
        ASSERT(growth >= 0);
        if (m_growthLimit == infinity)
            m_growthLimit = m_baseSize + growth;
        else
            m_growthLimit += growth;

        ASSERT(m_growthLimit >= m_baseSize);
    }

    bool growthLimitIsInfinite() const
    {
        return m_growthLimit == infinity;
    }

    const LayoutUnit& growthLimitIfNotInfinite() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return (m_growthLimit == infinity) ? m_baseSize : m_growthLimit;
    }

    const LayoutUnit& plannedIncrease() const { return m_plannedIncrease; }

    void growPlannedIncrease(const LayoutUnit& plannedIncrease)
    {
        ASSERT(plannedIncrease >= 0);
        m_plannedIncrease += plannedIncrease;
    }

    void updateFromPlannedIncrease(LayoutGrid::AccumulatorGrowFunction trackGrowthFunction)
    {
        if (m_plannedIncrease == 0)
            return;

        (this->*trackGrowthFunction)(m_plannedIncrease);
        m_plannedIncrease = 0;
    }

private:
    bool isGrowthLimitBiggerThanBaseSize() const { return growthLimitIsInfinite() || m_growthLimit >= m_baseSize; }

    void ensureGrowthLimitIsBiggerThanBaseSize()
    {
        if (m_growthLimit != infinity && m_growthLimit < m_baseSize)
            m_growthLimit = m_baseSize;
    }

    LayoutUnit m_baseSize;
    LayoutUnit m_growthLimit;
    LayoutUnit m_plannedIncrease;
};

struct GridTrackForNormalization {
    GridTrackForNormalization(const GridTrack& track, double flex)
        : m_track(&track)
        , m_flex(flex)
        , m_normalizedFlexValue(track.baseSize() / flex)
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

class LayoutGrid::GridIterator {
    WTF_MAKE_NONCOPYABLE(GridIterator);
public:
    // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
    // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd column.
    GridIterator(const GridRepresentation& grid, GridTrackSizingDirection direction, size_t fixedTrackIndex, size_t varyingTrackIndex = 0)
        : m_grid(grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? varyingTrackIndex : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : varyingTrackIndex)
        , m_childIndex(0)
    {
        ASSERT(m_rowIndex < m_grid.size());
        ASSERT(m_columnIndex < m_grid[0].size());
    }

    LayoutBox* nextGridItem()
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

    bool checkEmptyCells(size_t rowSpan, size_t columnSpan) const
    {
        // Ignore cells outside current grid as we will grow it later if needed.
        size_t maxRows = std::min(m_rowIndex + rowSpan, m_grid.size());
        size_t maxColumns = std::min(m_columnIndex + columnSpan, m_grid[0].size());

        // This adds a O(N^2) behavior that shouldn't be a big deal as we expect spanning areas to be small.
        for (size_t row = m_rowIndex; row < maxRows; ++row) {
            for (size_t column = m_columnIndex; column < maxColumns; ++column) {
                const GridCell& children = m_grid[row][column];
                if (!children.isEmpty())
                    return false;
            }
        }

        return true;
    }

    PassOwnPtr<GridCoordinate> nextEmptyGridArea(size_t fixedTrackSpan, size_t varyingTrackSpan)
    {
        ASSERT(!m_grid.isEmpty());
        ASSERT(fixedTrackSpan >= 1 && varyingTrackSpan >= 1);

        size_t rowSpan = (m_direction == ForColumns) ? varyingTrackSpan : fixedTrackSpan;
        size_t columnSpan = (m_direction == ForColumns) ? fixedTrackSpan : varyingTrackSpan;

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            if (checkEmptyCells(rowSpan, columnSpan)) {
                OwnPtr<GridCoordinate> result = adoptPtr(new GridCoordinate(GridSpan(m_rowIndex, m_rowIndex + rowSpan - 1), GridSpan(m_columnIndex, m_columnIndex + columnSpan - 1)));
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

struct LayoutGrid::GridSizingData {
    WTF_MAKE_NONCOPYABLE(GridSizingData);
    STACK_ALLOCATED();
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
    Vector<GridTrack*> filteredTracks;
    Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
    Vector<GridTrack*> growBeyondGrowthLimitsTracks;
};

LayoutGrid::LayoutGrid(Element* element)
    : LayoutBlock(element)
    , m_gridIsDirty(true)
    , m_orderIterator(this)
{
    ASSERT(!childrenInline());
}

LayoutGrid::~LayoutGrid()
{
}

void LayoutGrid::addChild(LayoutObject* newChild, LayoutObject* beforeChild)
{
    LayoutBlock::addChild(newChild, beforeChild);

    if (gridIsDirty())
        return;

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    dirtyGrid();
    return;
}

void LayoutGrid::removeChild(LayoutObject* child)
{
    LayoutBlock::removeChild(child);

    if (gridIsDirty())
        return;

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    dirtyGrid();
    return;
}

void LayoutGrid::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    LayoutBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle)
        return;

    // FIXME: The following checks could be narrowed down if we kept track of which type of grid items we have:
    // - explicit grid size changes impact negative explicitely positioned and auto-placed grid items.
    // - named grid lines only impact grid items with named grid lines.
    // - auto-flow changes only impacts auto-placed children.

    if (explicitGridDidResize(*oldStyle)
        || namedGridLinesDefinitionDidChange(*oldStyle)
        || oldStyle->gridAutoFlow() != styleRef().gridAutoFlow())
        dirtyGrid();
}

bool LayoutGrid::explicitGridDidResize(const LayoutStyle& oldStyle) const
{
    return oldStyle.gridTemplateColumns().size() != styleRef().gridTemplateColumns().size()
        || oldStyle.gridTemplateRows().size() != styleRef().gridTemplateRows().size();
}

bool LayoutGrid::namedGridLinesDefinitionDidChange(const LayoutStyle& oldStyle) const
{
    return oldStyle.namedGridRowLines() != styleRef().namedGridRowLines()
        || oldStyle.namedGridColumnLines() != styleRef().namedGridColumnLines();
}

void LayoutGrid::layoutBlock(bool relayoutChildren)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // FIXME: Much of this method is boiler plate that matches LayoutBox::layoutBlock and Render*FlexibleBox::layoutBlock.
    // It would be nice to refactor some of the duplicate code.
    {
        // LayoutState needs this deliberate scope to pop before updating scroll information (which
        // may trigger relayout).
        LayoutState state(*this, locationOffset());

        LayoutSize previousSize = size();

        setLogicalHeight(0);
        updateLogicalWidth();

        TextAutosizer::LayoutScope textAutosizerLayoutScope(this);

        layoutGridItems();

        LayoutUnit oldClientAfterEdge = clientLogicalBottom();
        updateLogicalHeight();

        if (size() != previousSize)
            relayoutChildren = true;

        layoutPositionedObjects(relayoutChildren || isDocumentElement());

        computeOverflow(oldClientAfterEdge);
    }

    updateLayerTransformAfterLayout();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->scrollableArea()->updateAfterLayout();

    clearNeedsLayout();
}

void LayoutGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const_cast<LayoutGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    LayoutUnit availableLogicalSpace = 0;
    const_cast<LayoutGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, availableLogicalSpace);

    for (const auto& column : sizingData.columnTracks) {
        const LayoutUnit& minTrackBreadth = column.baseSize();
        const LayoutUnit& maxTrackBreadth = column.growthLimit();

        minLogicalWidth += minTrackBreadth;
        maxLogicalWidth += maxTrackBreadth;

        LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
        maxLogicalWidth += scrollbarWidth;
        minLogicalWidth += scrollbarWidth;
    }
}

void LayoutGrid::computePreferredLogicalWidths()
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

bool LayoutGrid::gridElementIsShrinkToFit()
{
    return isFloatingOrOutOfFlowPositioned();
}

void LayoutGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& freeSpace)
{
    const LayoutUnit initialFreeSpace = freeSpace;
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<size_t> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    // 1. Initialize per Grid track variables.
    for (size_t i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        GridTrackSize trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.setBaseSize(computeUsedBreadthOfMinLength(direction, minTrackBreadth));
        track.setGrowthLimit(computeUsedBreadthOfMaxLength(direction, maxTrackBreadth, track.baseSize()));

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData, freeSpace);

    for (const auto& track: tracks) {
        ASSERT(!track.growthLimitIsInfinite());
        freeSpace -= track.baseSize();
    }

    const bool hasUndefinedRemainingSpace = (direction == ForRows) ? style()->logicalHeight().isAuto() : gridElementIsShrinkToFit();

    if (!hasUndefinedRemainingSpace && freeSpace <= 0)
        return;

    // 3. Grow all Grid tracks in GridTracks from their baseSize up to their growthLimit value until freeSpace is exhausted.
    const size_t tracksSize = tracks.size();
    if (!hasUndefinedRemainingSpace) {
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (size_t i = 0; i < tracksSize; ++i)
            tracksForDistribution[i] = tracks.data() + i;

        distributeSpaceToTracks(tracksForDistribution, nullptr, &GridTrack::baseSize, &GridTrack::growBaseSize, sizingData, freeSpace);
    } else {
        for (auto& track : tracks)
            track.setBaseSize(track.growthLimit());
    }

    if (flexibleSizedTracksIndex.isEmpty())
        return;

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double normalizedFractionBreadth = 0;
    if (!hasUndefinedRemainingSpace) {
        normalizedFractionBreadth = computeNormalizedFractionBreadth(tracks, GridSpan(0, tracks.size() - 1), direction, initialFreeSpace);
    } else {
        for (const auto& trackIndex : flexibleSizedTracksIndex) {
            GridTrackSize trackSize = gridTrackSize(direction, trackIndex);
            normalizedFractionBreadth = std::max(normalizedFractionBreadth, tracks[trackIndex].baseSize() / trackSize.maxTrackBreadth().flex());
        }

        for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (LayoutBox* gridItem = iterator.nextGridItem()) {
                const GridCoordinate coordinate = cachedGridCoordinate(*gridItem);
                const GridSpan span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

                // Do not include already processed items.
                if (i > 0 && span.resolvedInitialPosition.toInt() <= flexibleSizedTracksIndex[i - 1])
                    continue;

                double itemNormalizedFlexBreadth = computeNormalizedFractionBreadth(tracks, span, direction, maxContentForChild(*gridItem, direction, sizingData.columnTracks));
                normalizedFractionBreadth = std::max(normalizedFractionBreadth, itemNormalizedFlexBreadth);
            }
        }
    }

    for (const auto& trackIndex : flexibleSizedTracksIndex) {
        GridTrackSize trackSize = gridTrackSize(direction, trackIndex);

        LayoutUnit baseSize = std::max<LayoutUnit>(tracks[trackIndex].baseSize(), normalizedFractionBreadth * trackSize.maxTrackBreadth().flex());
        tracks[trackIndex].setBaseSize(baseSize);
        freeSpace -= baseSize;
    }

    // FIXME: Should ASSERT flexible tracks exhaust the freeSpace ? (see issue 739613002).
}

LayoutUnit LayoutGrid::computeUsedBreadthOfMinLength(GridTrackSizingDirection direction, const GridLength& gridLength) const
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

LayoutUnit LayoutGrid::computeUsedBreadthOfMaxLength(GridTrackSizingDirection direction, const GridLength& gridLength, LayoutUnit usedBreadth) const
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

LayoutUnit LayoutGrid::computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection direction, const Length& trackLength) const
{
    ASSERT(trackLength.isSpecified());
    // FIXME: The -1 here should be replaced by whatever the intrinsic height of the grid is.
    return valueForLength(trackLength, direction == ForColumns ? logicalWidth() : computeContentLogicalHeight(style()->logicalHeight(), -1));
}

static bool sortByGridNormalizedFlexValue(const GridTrackForNormalization& track1, const GridTrackForNormalization& track2)
{
    return track1.m_normalizedFlexValue < track2.m_normalizedFlexValue;
}

double LayoutGrid::computeNormalizedFractionBreadth(Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit spaceToFill) const
{
    LayoutUnit allocatedSpace;
    Vector<GridTrackForNormalization> tracksForNormalization;
    for (const auto& resolvedPosition : tracksSpan) {
        GridTrack& track = tracks[resolvedPosition.toInt()];
        allocatedSpace += track.baseSize();

        GridTrackSize trackSize = gridTrackSize(direction, resolvedPosition.toInt());
        if (!trackSize.maxTrackBreadth().isFlex())
            continue;

        tracksForNormalization.append(GridTrackForNormalization(track, trackSize.maxTrackBreadth().flex()));
    }

    // The function is not called if we don't have <flex> grid tracks
    ASSERT(!tracksForNormalization.isEmpty());

    std::sort(tracksForNormalization.begin(), tracksForNormalization.end(), sortByGridNormalizedFlexValue);

    // These values work together: as we walk over our grid tracks, we increase fractionValueBasedOnGridItemsRatio
    // to match a grid track's usedBreadth to <flex> ratio until the total fractions sized grid tracks wouldn't
    // fit into availableLogicalSpaceIgnoringFractionTracks.
    double accumulatedFractions = 0;
    LayoutUnit fractionValueBasedOnGridItemsRatio = 0;
    LayoutUnit availableLogicalSpaceIgnoringFractionTracks = spaceToFill - allocatedSpace;

    for (const auto& track : tracksForNormalization) {
        if (track.m_normalizedFlexValue > fractionValueBasedOnGridItemsRatio) {
            // If the normalized flex value (we ordered |tracksForNormalization| by increasing normalized flex value)
            // will make us overflow our container, then stop. We have the previous step's ratio is the best fit.
            if (track.m_normalizedFlexValue * accumulatedFractions > availableLogicalSpaceIgnoringFractionTracks)
                break;

            fractionValueBasedOnGridItemsRatio = track.m_normalizedFlexValue;
        }

        accumulatedFractions += track.m_flex;
        // This item was processed so we re-add its used breadth to the available space to accurately count the remaining space.
        availableLogicalSpaceIgnoringFractionTracks += track.m_track->baseSize();
    }

    return availableLogicalSpaceIgnoringFractionTracks / accumulatedFractions;
}

bool LayoutGrid::hasDefiniteLogicalSize(GridTrackSizingDirection direction) const
{
    return (direction == ForRows) ? hasDefiniteLogicalHeight() : hasDefiniteLogicalWidth();
}

GridTrackSize LayoutGrid::gridTrackSize(GridTrackSizingDirection direction, size_t i) const
{
    bool isForColumns = direction == ForColumns;
    const Vector<GridTrackSize>& trackStyles = isForColumns ? style()->gridTemplateColumns() : style()->gridTemplateRows();
    const GridTrackSize& trackSize = (i >= trackStyles.size()) ? (isForColumns ? style()->gridAutoColumns() : style()->gridAutoRows()) : trackStyles[i];

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();

    // If the logical width/height of the grid container is indefinite, percentage values are treated as <auto> (or in
    // the case of minmax() as min-content for the first position and max-content for the second).
    if (minTrackBreadth.isPercentage() || maxTrackBreadth.isPercentage()) {
        if (!hasDefiniteLogicalSize(direction)) {
            if (minTrackBreadth.isPercentage())
                minTrackBreadth = Length(MinContent);
            if (maxTrackBreadth.isPercentage())
                maxTrackBreadth = Length(MaxContent);
        }
    }

    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

LayoutUnit LayoutGrid::logicalHeightForChild(LayoutBox& child, Vector<GridTrack>& columnTracks)
{
    SubtreeLayoutScope layoutScope(child);
    LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child.hasOverrideContainingBlockLogicalWidth() ? child.overrideContainingBlockContentLogicalWidth() : LayoutUnit();
    LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, columnTracks);
    if (child.hasRelativeLogicalHeight() || oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth) {
        layoutScope.setNeedsLayout(&child);
        // We need to clear the stretched height to properly compute logical height during layout.
        child.clearOverrideLogicalContentHeight();
    }

    child.setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
    // If |child| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the override logical height to -1 (no possible resolution).
    if (child.hasRelativeLogicalHeight())
        child.setOverrideContainingBlockContentLogicalHeight(-1);
    child.layoutIfNeeded();
    // If the child was stretched we should use its intrinsic height.
    return (child.hasOverrideHeight() ? childIntrinsicHeight(child) : child.logicalHeight()) + child.marginLogicalHeight();
}

LayoutUnit LayoutGrid::minContentForChild(LayoutBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return 0;

    if (direction == ForColumns) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the override logical width to -1 (no possible resolution).
        if (child.hasRelativeLogicalWidth())
            child.setOverrideContainingBlockContentLogicalWidth(-1);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.minPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalHeightForChild(child, columnTracks);
}

LayoutUnit LayoutGrid::maxContentForChild(LayoutBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the override logical width to -1 (no possible resolution).
        if (child.hasRelativeLogicalWidth())
            child.setOverrideContainingBlockContentLogicalWidth(-1);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalHeightForChild(child, columnTracks);
}

// We're basically using a class instead of a std::pair for two reasons. First of all, accessing gridItem() or
// coordinate() is much more self-explanatory that using .first or .second members in the pair. Secondly the class
// allows us to precompute the value of the span, something which is quite convenient for the sorting. Having a
// std::pair<LayoutBox*, size_t> does not work either because we still need the GridCoordinate so we'd have to add an
// extra hash lookup for each item at the beginning of LayoutGrid::resolveContentBasedTrackSizingFunctionsForItems().
class GridItemWithSpan {
public:
    GridItemWithSpan(LayoutBox& gridItem, const GridCoordinate& coordinate, GridTrackSizingDirection direction)
        : m_gridItem(&gridItem)
        , m_coordinate(coordinate)
    {
        const GridSpan& span = (direction == ForRows) ? coordinate.rows : coordinate.columns;
        m_span = span.resolvedFinalPosition.toInt() - span.resolvedInitialPosition.toInt() + 1;
    }

    LayoutBox& gridItem() const { return *m_gridItem; }
    GridCoordinate coordinate() const { return m_coordinate; }
#if ENABLE(ASSERT)
    size_t span() const { return m_span; }
#endif

    bool operator<(const GridItemWithSpan other) const { return m_span < other.m_span; }

private:
    LayoutBox* m_gridItem;
    GridCoordinate m_coordinate;
    size_t m_span;
};

bool LayoutGrid::spanningItemCrossesFlexibleSizedTracks(const GridCoordinate& coordinate, GridTrackSizingDirection direction) const
{
    const GridResolvedPosition initialTrackPosition = (direction == ForColumns) ? coordinate.columns.resolvedInitialPosition : coordinate.rows.resolvedInitialPosition;
    const GridResolvedPosition finalTrackPosition = (direction == ForColumns) ? coordinate.columns.resolvedFinalPosition : coordinate.rows.resolvedFinalPosition;

    for (GridResolvedPosition trackPosition = initialTrackPosition; trackPosition <= finalTrackPosition; ++trackPosition) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition.toInt());
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
}

static inline size_t integerSpanForDirection(const GridCoordinate& coordinate, GridTrackSizingDirection direction)
{
    return (direction == ForRows) ? coordinate.rows.integerSpan() : coordinate.columns.integerSpan();
}

void LayoutGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    sizingData.itemsSortedByIncreasingSpan.shrink(0);
    HashSet<LayoutBox*> itemsSet;
    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridIterator iterator(m_grid, direction, trackIndex);
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        while (LayoutBox* gridItem = iterator.nextGridItem()) {
            if (itemsSet.add(gridItem).isNewEntry) {
                const GridCoordinate& coordinate = cachedGridCoordinate(*gridItem);
                if (integerSpanForDirection(coordinate, direction) == 1) {
                    resolveContentBasedTrackSizingFunctionsForNonSpanningItems(direction, coordinate, *gridItem, track, sizingData.columnTracks);
                } else if (!spanningItemCrossesFlexibleSizedTracks(coordinate, direction)) {
                    sizingData.itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, coordinate, direction));
                }
            }
        }
    }
    std::sort(sizingData.itemsSortedByIncreasingSpan.begin(), sizingData.itemsSortedByIncreasingSpan.end());

    Vector<GridItemWithSpan>::iterator end = sizingData.itemsSortedByIncreasingSpan.end();
    for (Vector<GridItemWithSpan>::iterator it = sizingData.itemsSortedByIncreasingSpan.begin(); it != end; ++it) {
        GridItemWithSpan itemWithSpan = *it;
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMinOrMaxContentMinTrackBreadth, &LayoutGrid::minContentForChild, &GridTrack::baseSize, &GridTrack::growBaseSize, &GridTrackSize::hasMinContentMinTrackBreadthAndMinOrMaxContentMaxTrackBreadth);
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMaxContentMinTrackBreadth, &LayoutGrid::maxContentForChild, &GridTrack::baseSize, &GridTrack::growBaseSize, &GridTrackSize::hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth);
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMinOrMaxContentMaxTrackBreadth, &LayoutGrid::minContentForChild, &GridTrack::growthLimitIfNotInfinite, &GridTrack::growGrowthLimit);
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMaxContentMaxTrackBreadth, &LayoutGrid::maxContentForChild, &GridTrack::growthLimitIfNotInfinite, &GridTrack::growGrowthLimit);
    }

    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        if (track.growthLimitIsInfinite())
            track.setGrowthLimit(track.baseSize());
    }
}

void LayoutGrid::resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection direction, const GridCoordinate& coordinate, LayoutBox& gridItem, GridTrack& track, Vector<GridTrack>& columnTracks)
{
    const GridResolvedPosition trackPosition = (direction == ForColumns) ? coordinate.columns.resolvedInitialPosition : coordinate.rows.resolvedInitialPosition;
    GridTrackSize trackSize = gridTrackSize(direction, trackPosition.toInt());

    if (trackSize.hasMinContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), maxContentForChild(gridItem, direction, columnTracks)));

    if (trackSize.hasMinContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), maxContentForChild(gridItem, direction, columnTracks)));
}

void LayoutGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, GridItemWithSpan& gridItemWithSpan, FilterFunction filterFunction, SizingFunction sizingFunction, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, FilterFunction growAboveMaxBreadthFilterFunction)
{
    ASSERT(gridItemWithSpan.span() > 1);
    const GridCoordinate coordinate = gridItemWithSpan.coordinate();
    const GridSpan& itemSpan = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

    sizingData.growBeyondGrowthLimitsTracks.shrink(0);
    sizingData.filteredTracks.shrink(0);
    LayoutUnit spanningTracksSize;
    for (const auto& trackPosition : itemSpan) {
        GridTrackSize trackSize = gridTrackSize(direction, trackPosition.toInt());
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackPosition.toInt()] : sizingData.rowTracks[trackPosition.toInt()];
        spanningTracksSize += (track.*trackGetter)();
        if (!(trackSize.*filterFunction)())
            continue;

        sizingData.filteredTracks.append(&track);

        if (!growAboveMaxBreadthFilterFunction || (trackSize.*growAboveMaxBreadthFilterFunction)())
            sizingData.growBeyondGrowthLimitsTracks.append(&track);
    }

    if (sizingData.filteredTracks.isEmpty())
        return;

    // Specs mandate to floor extraSpace to 0. Instead we directly avoid the function call in those cases as it will be
    // a noop in terms of track sizing.
    LayoutUnit extraSpace = (this->*sizingFunction)(gridItemWithSpan.gridItem(), direction, sizingData.columnTracks) - spanningTracksSize;
    if (extraSpace > 0) {
        Vector<GridTrack*>* tracksToGrowBeyondGrowthLimits = sizingData.growBeyondGrowthLimitsTracks.isEmpty() ? &sizingData.filteredTracks : &sizingData.growBeyondGrowthLimitsTracks;
        distributeSpaceToTracks(sizingData.filteredTracks, tracksToGrowBeyondGrowthLimits, trackGetter, trackGrowthFunction, sizingData, extraSpace);
    }
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    // This check ensures that we respect the irreflexivity property of the strict weak ordering required by std::sort
    // (forall x: NOT x < x).
    if (track1->growthLimitIsInfinite() && track2->growthLimitIsInfinite())
        return false;

    if (track1->growthLimitIsInfinite() || track2->growthLimitIsInfinite())
        return track2->growthLimitIsInfinite();

    return (track1->growthLimit() - track1->baseSize()) < (track2->growthLimit() - track2->baseSize());
}

void LayoutGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    ASSERT(availableLogicalSpace > 0);
    std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

    size_t tracksSize = tracks.size();
    for (size_t i = 0; i < tracksSize; ++i) {
        GridTrack& track = *tracks[i];
        ASSERT(track.plannedIncrease() == 0);
        LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
        const LayoutUnit& trackBreadth = (track.*trackGetter)();
        LayoutUnit growthShare = track.growthLimitIsInfinite() ? availableLogicalSpaceShare : std::min(availableLogicalSpaceShare, track.growthLimit() - trackBreadth);
        // We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function.
        if (growthShare > 0) {
            track.growPlannedIncrease(growthShare);
            availableLogicalSpace -= growthShare;
        }
    }

    if (availableLogicalSpace > 0 && growBeyondGrowthLimitsTracks) {
        size_t tracksGrowingAboveMaxBreadthSize = growBeyondGrowthLimitsTracks->size();
        for (size_t i = 0; i < tracksGrowingAboveMaxBreadthSize; ++i) {
            GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
            LayoutUnit growthShare = availableLogicalSpace / (tracksGrowingAboveMaxBreadthSize - i);
            track->growPlannedIncrease(growthShare);
            availableLogicalSpace -= growthShare;
        }
    }

    for (auto* track: tracks) {
        track->updateFromPlannedIncrease(trackGrowthFunction);
        ASSERT(track->plannedIncrease() == 0);
    }
}

#if ENABLE(ASSERT)
bool LayoutGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, const Vector<GridTrack>& tracks)
{
    for (size_t i = 0; i < tracks.size(); ++i) {
        GridTrackSize trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(direction, minTrackBreadth) > tracks[i].baseSize())
            return false;
    }
    return true;
}
#endif

void LayoutGrid::ensureGridSize(size_t maximumRowIndex, size_t maximumColumnIndex)
{
    const size_t oldRowSize = gridRowCount();
    if (maximumRowIndex >= oldRowSize) {
        m_grid.grow(maximumRowIndex + 1);
        for (size_t row = oldRowSize; row < gridRowCount(); ++row)
            m_grid[row].grow(gridColumnCount());
    }

    if (maximumColumnIndex >= gridColumnCount()) {
        for (size_t row = 0; row < gridRowCount(); ++row)
            m_grid[row].grow(maximumColumnIndex + 1);
    }
}

void LayoutGrid::insertItemIntoGrid(LayoutBox& child, const GridCoordinate& coordinate)
{
    ensureGridSize(coordinate.rows.resolvedFinalPosition.toInt(), coordinate.columns.resolvedFinalPosition.toInt());

    for (GridSpan::iterator row = coordinate.rows.begin(); row != coordinate.rows.end(); ++row) {
        for (GridSpan::iterator column = coordinate.columns.begin(); column != coordinate.columns.end(); ++column)
            m_grid[row.toInt()][column.toInt()].append(&child);
    }

    RELEASE_ASSERT(!m_gridItemCoordinate.contains(&child));
    m_gridItemCoordinate.set(&child, coordinate);
}

void LayoutGrid::placeItemsOnGrid()
{
    if (!gridIsDirty())
        return;

    ASSERT(m_gridItemCoordinate.isEmpty());

    populateExplicitGridAndOrderIterator();

    // We clear the dirty bit here as the grid sizes have been updated, this means
    // that we can safely call gridRowCount() / gridColumnCount().
    m_gridIsDirty = false;

    Vector<LayoutBox*> autoMajorAxisAutoGridItems;
    Vector<LayoutBox*> specifiedMajorAxisAutoGridItems;
    for (LayoutBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        OwnPtr<GridSpan> rowPositions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), *child, ForRows);
        OwnPtr<GridSpan> columnPositions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), *child, ForColumns);
        if (!rowPositions || !columnPositions) {
            GridSpan* majorAxisPositions = (autoPlacementMajorAxisDirection() == ForColumns) ? columnPositions.get() : rowPositions.get();
            if (!majorAxisPositions)
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        insertItemIntoGrid(*child, GridCoordinate(*rowPositions, *columnPositions));
    }

    ASSERT(gridRowCount() >= GridResolvedPosition::explicitGridRowCount(*style()));
    ASSERT(gridColumnCount() >= GridResolvedPosition::explicitGridColumnCount(*style()));

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);

    m_grid.shrinkToFit();
}

void LayoutGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(m_orderIterator);

    size_t maximumRowIndex = std::max<size_t>(1, GridResolvedPosition::explicitGridRowCount(*style()));
    size_t maximumColumnIndex = std::max<size_t>(1, GridResolvedPosition::explicitGridColumnCount(*style()));

    ASSERT(m_gridItemsIndexesMap.isEmpty());
    size_t childIndex = 0;
    for (LayoutBox* child = firstChildBox(); child; child = child->nextInFlowSiblingBox()) {
        populator.collectChild(child);
        m_gridItemsIndexesMap.set(child, childIndex++);

        // This function bypasses the cache (cachedGridCoordinate()) as it is used to build it.
        OwnPtr<GridSpan> rowPositions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), *child, ForRows);
        OwnPtr<GridSpan> columnPositions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), *child, ForColumns);

        // |positions| is 0 if we need to run the auto-placement algorithm.
        if (rowPositions) {
            maximumRowIndex = std::max<size_t>(maximumRowIndex, rowPositions->resolvedFinalPosition.next().toInt());
        } else {
            // Grow the grid for items with a definite row span, getting the largest such span.
            GridSpan positions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(*style(), *child, ForRows, GridResolvedPosition(0));
            maximumRowIndex = std::max<size_t>(maximumRowIndex, positions.resolvedFinalPosition.next().toInt());
        }

        if (columnPositions) {
            maximumColumnIndex = std::max<size_t>(maximumColumnIndex, columnPositions->resolvedFinalPosition.next().toInt());
        } else {
            // Grow the grid for items with a definite column span, getting the largest such span.
            GridSpan positions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(*style(), *child, ForColumns, GridResolvedPosition(0));
            maximumColumnIndex = std::max<size_t>(maximumColumnIndex, positions.resolvedFinalPosition.next().toInt());
        }
    }

    m_grid.grow(maximumRowIndex);
    for (auto& column : m_grid)
        column.grow(maximumColumnIndex);
}

PassOwnPtr<GridCoordinate> LayoutGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const LayoutBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == ForColumns ? ForRows : ForColumns;
    const size_t endOfCrossDirection = crossDirection == ForColumns ? gridColumnCount() : gridRowCount();
    GridSpan crossDirectionPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(*style(), gridItem, crossDirection, GridResolvedPosition(endOfCrossDirection));
    return adoptPtr(new GridCoordinate(specifiedDirection == ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == ForColumns ? specifiedPositions : crossDirectionPositions));
}

void LayoutGrid::placeSpecifiedMajorAxisItemsOnGrid(const Vector<LayoutBox*>& autoGridItems)
{
    bool isForColumns = autoPlacementMajorAxisDirection() == ForColumns;
    bool isGridAutoFlowDense = style()->isGridAutoFlowAlgorithmDense();

    // Mapping between the major axis tracks (rows or columns) and the last auto-placed item's position inserted on
    // that track. This is needed to implement "sparse" packing for items locked to a given track.
    // See http://dev.w3.org/csswg/css-grid/#auto-placement-algo
    HashMap<unsigned, unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> minorAxisCursors;

    for (const auto& autoGridItem : autoGridItems) {
        OwnPtr<GridSpan> majorAxisPositions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), *autoGridItem, autoPlacementMajorAxisDirection());
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(*style(), *autoGridItem, autoPlacementMinorAxisDirection(), GridResolvedPosition(0));
        unsigned majorAxisInitialPosition = majorAxisPositions->resolvedInitialPosition.toInt();

        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions->resolvedInitialPosition.toInt(), isGridAutoFlowDense ? 0 : minorAxisCursors.get(majorAxisInitialPosition));
        OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions->integerSpan(), minorAxisPositions.integerSpan());
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(*autoGridItem, autoPlacementMajorAxisDirection(), *majorAxisPositions);
        insertItemIntoGrid(*autoGridItem, *emptyGridArea);

        if (!isGridAutoFlowDense)
            minorAxisCursors.set(majorAxisInitialPosition, isForColumns ? emptyGridArea->rows.resolvedInitialPosition.toInt() : emptyGridArea->columns.resolvedInitialPosition.toInt());
    }
}

void LayoutGrid::placeAutoMajorAxisItemsOnGrid(const Vector<LayoutBox*>& autoGridItems)
{
    std::pair<size_t, size_t> autoPlacementCursor = std::make_pair(0, 0);
    bool isGridAutoFlowDense = style()->isGridAutoFlowAlgorithmDense();

    for (const auto& autoGridItem : autoGridItems) {
        placeAutoMajorAxisItemOnGrid(*autoGridItem, autoPlacementCursor);

        // If grid-auto-flow is dense, reset auto-placement cursor.
        if (isGridAutoFlowDense) {
            autoPlacementCursor.first = 0;
            autoPlacementCursor.second = 0;
        }
    }
}

void LayoutGrid::placeAutoMajorAxisItemOnGrid(LayoutBox& gridItem, std::pair<size_t, size_t>& autoPlacementCursor)
{
    OwnPtr<GridSpan> minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), gridItem, autoPlacementMinorAxisDirection());
    ASSERT(!GridResolvedPosition::resolveGridPositionsFromStyle(*style(), gridItem, autoPlacementMajorAxisDirection()));
    GridSpan majorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(*style(), gridItem, autoPlacementMajorAxisDirection(), GridResolvedPosition(0));

    const size_t endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
    size_t majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    size_t minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    OwnPtr<GridCoordinate> emptyGridArea;
    if (minorAxisPositions) {
        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions->resolvedInitialPosition.toInt() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisPositions->resolvedInitialPosition.toInt(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions->integerSpan(), majorAxisPositions.integerSpan());
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), *minorAxisPositions);
    } else {
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(*style(), gridItem, autoPlacementMinorAxisDirection(), GridResolvedPosition(0));

        for (size_t majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisPositions.integerSpan());

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                GridResolvedPosition minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == ForColumns ? emptyGridArea->columns.resolvedFinalPosition : emptyGridArea->rows.resolvedFinalPosition;
                const size_t endOfMinorAxis = autoPlacementMinorAxisDirection() == ForColumns ? gridColumnCount() : gridRowCount();
                if (minorAxisFinalPositionIndex.toInt() < endOfMinorAxis)
                    break;

                // Discard empty grid area as it does not fit in the minor axis direction.
                // We don't need to create a new empty grid area yet as we might find a valid one in the next iteration.
                emptyGridArea = nullptr;
            }

            // As we're moving to the next track in the major axis we should reset the auto-placement cursor in the minor axis.
            minorAxisAutoPlacementCursor = 0;
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    }

    insertItemIntoGrid(gridItem, *emptyGridArea);
    // Move auto-placement cursor to the new position.
    autoPlacementCursor.first = emptyGridArea->rows.resolvedInitialPosition.toInt();
    autoPlacementCursor.second = emptyGridArea->columns.resolvedInitialPosition.toInt();
}

GridTrackSizingDirection LayoutGrid::autoPlacementMajorAxisDirection() const
{
    return style()->isGridAutoFlowDirectionColumn() ? ForColumns : ForRows;
}

GridTrackSizingDirection LayoutGrid::autoPlacementMinorAxisDirection() const
{
    return style()->isGridAutoFlowDirectionColumn() ? ForRows : ForColumns;
}

void LayoutGrid::dirtyGrid()
{
    // Even if this could be redundant, it could be seen as a defensive strategy against
    // style changes events happening during the layout phase or even while the painting process
    // is still ongoing.
    // Forcing a new layout for the Grid render would cancel any ongoing painting and ensure
    // the grid and its children are correctly laid out according to the new style rules.
    setNeedsLayout();

    m_grid.resize(0);
    m_gridItemCoordinate.clear();
    m_gridIsDirty = true;
    m_gridItemsOverflowingGridArea.resize(0);
    m_gridItemsIndexesMap.clear();
}

void LayoutGrid::layoutGridItems()
{
    placeItemsOnGrid();

    LayoutUnit availableSpaceForColumns = availableLogicalWidth();
    LayoutUnit availableSpaceForRows = availableLogicalHeight(IncludeMarginBorderPadding);
    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    computeUsedBreadthOfGridTracks(ForColumns, sizingData, availableSpaceForColumns);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, sizingData.columnTracks));
    computeUsedBreadthOfGridTracks(ForRows, sizingData, availableSpaceForRows);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData.rowTracks));

    populateGridPositions(sizingData);
    m_gridItemsOverflowingGridArea.resize(0);

    LayoutUnit columnOffset = contentPositionAndDistributionColumnOffset(availableSpaceForColumns, style()->justifyContent(), style()->justifyContentDistribution(), style()->justifyContentOverflowAlignment(), m_columnPositions.size() - 1);
    LayoutUnit rowOffset = contentPositionAndDistributionRowOffset(availableSpaceForRows, style()->alignContent(), style()->alignContentDistribution(), style()->alignContentOverflowAlignment(), m_rowPositions.size() - 1);
    LayoutSize contentPositionOffset(columnOffset, rowOffset);

    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned()) {
            child->containingBlock()->insertPositionedObject(child);
            continue;
        }

        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        LayoutUnit oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(*child, ForColumns, sizingData.columnTracks);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChild(*child, ForRows, sizingData.rowTracks);

        SubtreeLayoutScope layoutScope(*child);
        if (oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth || (oldOverrideContainingBlockContentLogicalHeight != overrideContainingBlockContentLogicalHeight && child->hasRelativeLogicalHeight()))
            layoutScope.setNeedsLayout(child);

        child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
        child->setOverrideContainingBlockContentLogicalHeight(overrideContainingBlockContentLogicalHeight);

        // Stretching logic might force a child layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that child margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToChildIfNeeded(*child, overrideContainingBlockContentLogicalHeight);

        child->layoutIfNeeded();

#if ENABLE(ASSERT)
        const GridCoordinate& coordinate = cachedGridCoordinate(*child);
        ASSERT(coordinate.columns.resolvedInitialPosition.toInt() < sizingData.columnTracks.size());
        ASSERT(coordinate.rows.resolvedInitialPosition.toInt() < sizingData.rowTracks.size());
#endif
        child->setLogicalLocation(findChildLogicalPosition(*child, contentPositionOffset));

        // Keep track of children overflowing their grid area as we might need to paint them even if the grid-area is
        // not visible
        if (child->logicalHeight() > overrideContainingBlockContentLogicalHeight
            || child->logicalWidth() > overrideContainingBlockContentLogicalWidth)
            m_gridItemsOverflowingGridArea.append(child);
    }

    for (const auto& row : sizingData.rowTracks)
        setLogicalHeight(logicalHeight() + row.baseSize());

    LayoutUnit height = logicalHeight() + borderAndPaddingLogicalHeight();
    if (hasLineIfEmpty())
        height = std::max(height, minimumLogicalHeightForEmptyLine());

    // Min / max logical height is handled by the call to updateLogicalHeight in layoutBlock.
    setLogicalHeight(height);
}

void LayoutGrid::layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior info)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    bool containerHasHorizontalWritingMode = isHorizontalWritingMode();
    for (auto* child : *positionedDescendants) {
        bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != containerHasHorizontalWritingMode;
        if (hasOrthogonalWritingMode) {
            // FIXME: Properly support orthogonal writing mode.
            continue;
        }

        // FIXME: Detect properly if start/end is auto for inexistent named grid lines.
        bool columnStartIsAuto = child->style()->gridColumnStart().isAuto();
        LayoutUnit columnOffset = LayoutUnit(0);
        LayoutUnit columnBreadth = LayoutUnit(0);
        offsetAndBreadthForPositionedChild(*child, ForColumns, columnStartIsAuto, child->style()->gridColumnEnd().isAuto(), columnOffset, columnBreadth);
        bool rowStartIsAuto = child->style()->gridRowStart().isAuto();
        LayoutUnit rowOffset = LayoutUnit(0);
        LayoutUnit rowBreadth = LayoutUnit(0);
        offsetAndBreadthForPositionedChild(*child, ForRows, rowStartIsAuto, child->style()->gridRowEnd().isAuto(), rowOffset, rowBreadth);

        child->setOverrideContainingBlockContentLogicalWidth(columnBreadth);
        child->setOverrideContainingBlockContentLogicalHeight(rowBreadth);
        child->setExtraInlineOffset(columnOffset);
        child->setExtraBlockOffset(rowOffset);

        if (child->parent() == this) {
            // If column/row start is not auto the padding has been already computed in offsetAndBreadthForPositionedChild().
            Layer* childLayer = child->layer();
            if (columnStartIsAuto)
                childLayer->setStaticInlinePosition(borderAndPaddingStart());
            else
                childLayer->setStaticInlinePosition(borderStart() + columnOffset);
            if (rowStartIsAuto)
                childLayer->setStaticBlockPosition(borderAndPaddingBefore());
            else
                childLayer->setStaticBlockPosition(borderBefore() + rowOffset);
        }
    }

    LayoutBlock::layoutPositionedObjects(relayoutChildren, info);
}

void LayoutGrid::offsetAndBreadthForPositionedChild(const LayoutBox& child, GridTrackSizingDirection direction, bool startIsAuto, bool endIsAuto, LayoutUnit& offset, LayoutUnit& breadth)
{
    ASSERT(child.isHorizontalWritingMode() == isHorizontalWritingMode());

    OwnPtr<GridSpan> positions = GridResolvedPosition::resolveGridPositionsFromStyle(*style(), child, direction);
    if (!positions) {
        offset = LayoutUnit(0);
        breadth = (direction == ForColumns) ? clientLogicalWidth() : clientLogicalHeight();
        return;
    }

    GridResolvedPosition firstPosition = GridResolvedPosition(0);
    GridResolvedPosition initialPosition = startIsAuto ? firstPosition : positions->resolvedInitialPosition;
    GridResolvedPosition lastPosition = GridResolvedPosition((direction == ForColumns ? gridColumnCount() : gridRowCount()) - 1);
    GridResolvedPosition finalPosition = endIsAuto ? lastPosition : positions->resolvedFinalPosition;

    // Positioned children do not grow the grid, so we need to clamp the positions to avoid ending up outside of it.
    initialPosition = std::min<GridResolvedPosition>(initialPosition, lastPosition);
    finalPosition = std::min<GridResolvedPosition>(finalPosition, lastPosition);

    LayoutUnit start = startIsAuto ? LayoutUnit(0) : (direction == ForColumns) ?  m_columnPositions[initialPosition.toInt()] : m_rowPositions[initialPosition.toInt()];
    LayoutUnit end = endIsAuto ? (direction == ForColumns) ? logicalWidth() : logicalHeight() : (direction == ForColumns) ?  m_columnPositions[finalPosition.next().toInt()] : m_rowPositions[finalPosition.next().toInt()];

    breadth = end - start;

    if (startIsAuto)
        breadth -= (direction == ForColumns) ? borderStart() : borderBefore();
    else
        start -= ((direction == ForColumns) ? borderStart() : borderBefore());

    if (endIsAuto) {
        breadth -= (direction == ForColumns) ? borderEnd() : borderAfter();
        breadth -= scrollbarLogicalWidth();
    }

    offset = start;
}

GridCoordinate LayoutGrid::cachedGridCoordinate(const LayoutBox& gridItem) const
{
    ASSERT(m_gridItemCoordinate.contains(&gridItem));
    return m_gridItemCoordinate.get(&gridItem);
}

LayoutUnit LayoutGrid::gridAreaBreadthForChild(const LayoutBox& child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    const GridSpan& span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    LayoutUnit gridAreaBreadth = 0;
    for (GridSpan::iterator trackPosition = span.begin(); trackPosition != span.end(); ++trackPosition)
        gridAreaBreadth += tracks[trackPosition.toInt()].baseSize();
    return gridAreaBreadth;
}

void LayoutGrid::populateGridPositions(const GridSizingData& sizingData)
{
    unsigned numberOfColumnTracks = sizingData.columnTracks.size();
    unsigned numberOfRowTracks = sizingData.rowTracks.size();

    m_columnPositions.resize(numberOfColumnTracks + 1);
    m_columnPositions[0] = borderAndPaddingStart();
    for (unsigned i = 0; i < numberOfColumnTracks; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + sizingData.columnTracks[i].baseSize();

    m_rowPositions.resize(numberOfRowTracks + 1);
    m_rowPositions[0] = borderAndPaddingBefore();
    for (unsigned i = 0; i < numberOfRowTracks; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + sizingData.rowTracks[i].baseSize();
}

static LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit startOfTrack, LayoutUnit endOfTrack, LayoutUnit childBreadth)
{
    LayoutUnit trackBreadth = endOfTrack - startOfTrack;
    LayoutUnit offset = trackBreadth - childBreadth;

    // If overflow is 'safe', we have to make sure we don't overflow the 'start'
    // edge (potentially cause some data loss as the overflow is unreachable).
    if (overflow == OverflowAlignmentSafe)
        offset = std::max<LayoutUnit>(0, offset);

    // If we overflow our alignment container and overflow is 'true' (default), we
    // ignore the overflow and just return the value regardless (which may cause data
    // loss as we overflow the 'start' edge).
    return offset;
}

LayoutUnit LayoutGrid::startOfColumnForChild(const LayoutBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.resolvedInitialPosition.toInt()];
    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    return startOfColumn + marginStartForChild(child);
}

LayoutUnit LayoutGrid::endOfColumnForChild(const LayoutBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.resolvedInitialPosition.toInt()];
    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    LayoutUnit columnPosition = startOfColumn + marginStartForChild(child);

    LayoutUnit endOfColumn = m_columnPositions[coordinate.columns.resolvedFinalPosition.next().toInt()];
    // FIXME: This might not work as expected with orthogonal writing-modes.
    LayoutUnit offsetFromColumnPosition = computeOverflowAlignmentOffset(child.style()->justifySelfOverflowAlignment(), startOfColumn, endOfColumn, child.logicalWidth() + child.marginLogicalWidth());

    return columnPosition + offsetFromColumnPosition;
}

LayoutUnit LayoutGrid::columnPositionLeft(const LayoutBox& child) const
{
    if (style()->isLeftToRightDirection())
        return startOfColumnForChild(child);

    return endOfColumnForChild(child);
}

LayoutUnit LayoutGrid::columnPositionRight(const LayoutBox& child) const
{
    if (!style()->isLeftToRightDirection())
        return startOfColumnForChild(child);

    return endOfColumnForChild(child);
}

LayoutUnit LayoutGrid::centeredColumnPositionForChild(const LayoutBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.resolvedInitialPosition.toInt()];
    LayoutUnit endOfColumn = m_columnPositions[coordinate.columns.resolvedFinalPosition.next().toInt()];
    LayoutUnit columnPosition = startOfColumn + marginStartForChild(child);
    // FIXME: This might not work as expected with orthogonal writing-modes.
    LayoutUnit offsetFromColumnPosition = computeOverflowAlignmentOffset(child.style()->justifySelfOverflowAlignment(), startOfColumn, endOfColumn, child.logicalWidth() + child.marginLogicalWidth());

    return columnPosition + offsetFromColumnPosition / 2;
}

LayoutUnit LayoutGrid::columnPositionForChild(const LayoutBox& child) const
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();

    switch (LayoutStyle::resolveJustification(styleRef(), child.styleRef(), ItemPositionStretch)) {
    case ItemPositionSelfStart:
        // For orthogonal writing-modes, this computes to 'start'
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (hasOrthogonalWritingMode)
            return startOfColumnForChild(child);

        // self-start is based on the child's direction. That's why we need to check against the grid container's direction.
        if (child.style()->direction() != style()->direction())
            return endOfColumnForChild(child);

        return startOfColumnForChild(child);
    case ItemPositionSelfEnd:
        // For orthogonal writing-modes, this computes to 'start'
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (hasOrthogonalWritingMode)
            return endOfColumnForChild(child);

        // self-end is based on the child's direction. That's why we need to check against the grid container's direction.
        if (child.style()->direction() != style()->direction())
            return startOfColumnForChild(child);

        return endOfColumnForChild(child);
    case ItemPositionFlexStart:
        // Only used in flex layout, for other layout, it's equivalent to 'start'.
        return startOfColumnForChild(child);
    case ItemPositionFlexEnd:
        // Only used in flex layout, for other layout, it's equivalent to 'end'.
        return endOfColumnForChild(child);
    case ItemPositionLeft:
        return columnPositionLeft(child);
    case ItemPositionRight:
        return columnPositionRight(child);
    case ItemPositionCenter:
        return centeredColumnPositionForChild(child);
    case ItemPositionStart:
        return startOfColumnForChild(child);
    case ItemPositionEnd:
        return endOfColumnForChild(child);
    case ItemPositionAuto:
        break;
    case ItemPositionStretch:
        return startOfColumnForChild(child);
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return startOfColumnForChild(child);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit LayoutGrid::endOfRowForChild(const LayoutBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);

    LayoutUnit startOfRow = m_rowPositions[coordinate.rows.resolvedInitialPosition.toInt()];
    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    LayoutUnit rowPosition = startOfRow + marginBeforeForChild(child);

    LayoutUnit endOfRow = m_rowPositions[coordinate.rows.resolvedFinalPosition.next().toInt()];
    LayoutUnit offsetFromRowPosition = computeOverflowAlignmentOffset(child.style()->alignSelfOverflowAlignment(), startOfRow, endOfRow, child.logicalHeight() + child.marginLogicalHeight());

    return rowPosition + offsetFromRowPosition;
}

LayoutUnit LayoutGrid::startOfRowForChild(const LayoutBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);

    LayoutUnit startOfRow = m_rowPositions[coordinate.rows.resolvedInitialPosition.toInt()];
    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    LayoutUnit rowPosition = startOfRow + marginBeforeForChild(child);

    return rowPosition;
}

LayoutUnit LayoutGrid::centeredRowPositionForChild(const LayoutBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);

    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    LayoutUnit startOfRow = m_rowPositions[coordinate.rows.resolvedInitialPosition.toInt()];
    LayoutUnit endOfRow = m_rowPositions[coordinate.rows.resolvedFinalPosition.next().toInt()];
    LayoutUnit rowPosition = startOfRow + marginBeforeForChild(child);
    LayoutUnit offsetFromRowPosition = computeOverflowAlignmentOffset(child.style()->alignSelfOverflowAlignment(), startOfRow, endOfRow, child.logicalHeight() + child.marginLogicalHeight());

    return rowPosition + offsetFromRowPosition / 2;
}

static inline LayoutUnit constrainedChildIntrinsicContentLogicalHeight(const LayoutBox& child)
{
    LayoutUnit childIntrinsicContentLogicalHeight = child.intrinsicContentLogicalHeight();
    return child.constrainLogicalHeightByMinMax(childIntrinsicContentLogicalHeight + child.borderAndPaddingLogicalHeight(), childIntrinsicContentLogicalHeight);
}

bool LayoutGrid::allowedToStretchLogicalHeightForChild(const LayoutBox& child) const
{
    return child.style()->logicalHeight().isAuto() && !child.style()->marginBeforeUsing(style()).isAuto() && !child.style()->marginAfterUsing(style()).isAuto();
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
bool LayoutGrid::needToStretchChildLogicalHeight(const LayoutBox& child) const
{
    if (LayoutStyle::resolveAlignment(styleRef(), child.styleRef(), ItemPositionStretch) != ItemPositionStretch)
        return false;

    return isHorizontalWritingMode() && child.style()->height().isAuto();
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
LayoutUnit LayoutGrid::childIntrinsicHeight(const LayoutBox& child) const
{
    if (child.isHorizontalWritingMode() && needToStretchChildLogicalHeight(child))
        return constrainedChildIntrinsicContentLogicalHeight(child);
    return child.size().height();
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
LayoutUnit LayoutGrid::childIntrinsicWidth(const LayoutBox& child) const
{
    if (!child.isHorizontalWritingMode() && needToStretchChildLogicalHeight(child))
        return constrainedChildIntrinsicContentLogicalHeight(child);
    return child.size().width();
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
LayoutUnit LayoutGrid::intrinsicLogicalHeightForChild(const LayoutBox& child) const
{
    return isHorizontalWritingMode() ? childIntrinsicHeight(child) : childIntrinsicWidth(child);
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
LayoutUnit LayoutGrid::marginLogicalHeightForChild(const LayoutBox& child) const
{
    return isHorizontalWritingMode() ? child.marginHeight() : child.marginWidth();
}

LayoutUnit LayoutGrid::computeMarginLogicalHeightForChild(const LayoutBox& child) const
{
    LayoutUnit marginBefore;
    LayoutUnit marginAfter;
    child.computeMarginsForDirection(BlockDirection, this, child.containingBlockLogicalWidthForContent(), child.logicalHeight(), marginBefore, marginAfter,
        child.style()->marginBeforeUsing(style()),
        child.style()->marginAfterUsing(style()));

    return marginBefore + marginAfter;
}

LayoutUnit LayoutGrid::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const LayoutBox& child) const
{
    LayoutUnit childMarginLogicalHeight = marginLogicalHeightForChild(child);

    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // children are laid out, so we can't use the child cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    if (childMarginLogicalHeight == 0)
        childMarginLogicalHeight = computeMarginLogicalHeightForChild(child);

    LayoutUnit childLogicalHeight = childMarginLogicalHeight + intrinsicLogicalHeightForChild(child);
    return gridAreaBreadthForChild - childLogicalHeight;
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
void LayoutGrid::applyStretchAlignmentToChildIfNeeded(LayoutBox& child, LayoutUnit gridAreaBreadthForChild)
{
    if (LayoutStyle::resolveAlignment(styleRef(), child.styleRef(), ItemPositionStretch) != ItemPositionStretch)
        return;

    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    if (allowedToStretchLogicalHeightForChild(child)) {
        // FIXME: If the child has orthogonal flow, then it already has an override height set, so use it.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (!hasOrthogonalWritingMode) {
            LayoutUnit heightBeforeStretching = needToStretchChildLogicalHeight(child) ? constrainedChildIntrinsicContentLogicalHeight(child) : child.logicalHeight();
            LayoutUnit stretchedLogicalHeight = heightBeforeStretching + availableAlignmentSpaceForChildBeforeStretching(gridAreaBreadthForChild, child);
            LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, heightBeforeStretching - child.borderAndPaddingLogicalHeight());
            LayoutUnit desiredLogicalContentHeight = desiredLogicalHeight - child.borderAndPaddingLogicalHeight();

            // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
            if (desiredLogicalHeight != child.logicalHeight() || !child.hasOverrideHeight() || desiredLogicalContentHeight != child.overrideLogicalContentHeight()) {
                child.setOverrideLogicalContentHeight(desiredLogicalContentHeight);
                child.setLogicalHeight(0);
                child.forceChildLayout();
            }
        }
    }
}

LayoutUnit LayoutGrid::rowPositionForChild(const LayoutBox& child) const
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    switch (LayoutStyle::resolveAlignment(styleRef(), child.styleRef(), ItemPositionStretch)) {
    case ItemPositionSelfStart:
        // If orthogonal writing-modes, this computes to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (hasOrthogonalWritingMode)
            return startOfRowForChild(child);

        // self-start is based on the child's block axis direction. That's why we need to check against the grid container's block flow.
        if (child.style()->writingMode() != style()->writingMode())
            return endOfRowForChild(child);

        return startOfRowForChild(child);
    case ItemPositionSelfEnd:
        // If orthogonal writing-modes, this computes to 'end'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (hasOrthogonalWritingMode)
            return endOfRowForChild(child);

        // self-end is based on the child's block axis direction. That's why we need to check against the grid container's block flow.
        if (child.style()->writingMode() != style()->writingMode())
            return startOfRowForChild(child);

        return endOfRowForChild(child);
    case ItemPositionLeft:
        // The alignment axis (column axis) and the inline axis are parallell in
        // orthogonal writing mode.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (hasOrthogonalWritingMode)
            return startOfRowForChild(child);

        // Otherwise this this is equivalent to 'start'.
        return startOfRowForChild(child);
    case ItemPositionRight:
        // The alignment axis (column axis) and the inline axis are parallell in
        // orthogonal writing mode.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        if (hasOrthogonalWritingMode)
            return endOfRowForChild(child);

        // Otherwise this this is equivalent to 'start'.
        return startOfRowForChild(child);
    case ItemPositionCenter:
        return centeredRowPositionForChild(child);
        // Only used in flex layout, for other layout, it's equivalent to 'start'.
    case ItemPositionFlexStart:
    case ItemPositionStart:
        return startOfRowForChild(child);
        // Only used in flex layout, for other layout, it's equivalent to 'end'.
    case ItemPositionFlexEnd:
    case ItemPositionEnd:
        return endOfRowForChild(child);
    case ItemPositionStretch:
        return startOfRowForChild(child);
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: Implement the ItemPositionBaseline value. For now, we always 'start' align the child.
        return startOfRowForChild(child);
    case ItemPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ContentPosition static resolveContentDistributionFallback(ContentDistributionType distribution)
{
    switch (distribution) {
    case ContentDistributionSpaceBetween:
        return ContentPositionStart;
    case ContentDistributionSpaceAround:
        return ContentPositionCenter;
    case ContentDistributionSpaceEvenly:
        return ContentPositionCenter;
    case ContentDistributionStretch:
        return ContentPositionStart;
    case ContentDistributionDefault:
        return ContentPositionAuto;
    }

    ASSERT_NOT_REACHED();
    return ContentPositionAuto;
}

static inline LayoutUnit offsetToStartEdge(bool isLeftToRight, LayoutUnit availableSpace)
{
    return isLeftToRight ? LayoutUnit(0) : availableSpace;
}

static inline LayoutUnit offsetToEndEdge(bool isLeftToRight, LayoutUnit availableSpace)
{
    return !isLeftToRight ? LayoutUnit(0) : availableSpace;
}

LayoutUnit LayoutGrid::contentPositionAndDistributionColumnOffset(LayoutUnit availableFreeSpace, ContentPosition position, ContentDistributionType distribution, OverflowAlignment overflow, unsigned numberOfGridTracks) const
{
    if (overflow == OverflowAlignmentSafe && availableFreeSpace <= 0)
        return 0;

    // FIXME: for the time being, spec states that it will always fallback for Grids, but
    // discussion is ongoing.
    if (distribution != ContentDistributionDefault && position == ContentPositionAuto)
        position = resolveContentDistributionFallback(distribution);

    switch (position) {
    case ContentPositionLeft:
        return 0;
    case ContentPositionRight:
        return availableFreeSpace;
    case ContentPositionCenter:
        return availableFreeSpace / 2;
    case ContentPositionFlexEnd:
        // Only used in flex layout, for other layout, it's equivalent to 'end'.
    case ContentPositionEnd:
        return offsetToEndEdge(style()->isLeftToRightDirection(), availableFreeSpace);
    case ContentPositionFlexStart:
        // Only used in flex layout, for other layout, it's equivalent to 'start'.
    case ContentPositionStart:
        return offsetToStartEdge(style()->isLeftToRightDirection(), availableFreeSpace);
    case ContentPositionBaseline:
    case ContentPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align.
        // crbug.com/234191
        return offsetToStartEdge(style()->isLeftToRightDirection(), availableFreeSpace);
    case ContentPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit LayoutGrid::contentPositionAndDistributionRowOffset(LayoutUnit availableFreeSpace, ContentPosition position, ContentDistributionType distribution, OverflowAlignment overflow, unsigned numberOfGridTracks) const
{
    if (overflow == OverflowAlignmentSafe && availableFreeSpace <= 0)
        return 0;

    // FIXME: for the time being, spec states that it will always fallback for Grids, but
    // discussion is ongoing.
    if (distribution != ContentDistributionDefault && position == ContentPositionAuto)
        position = resolveContentDistributionFallback(distribution);

    switch (position) {
    case ContentPositionLeft:
        // The align-content's axis is always orthogonal to the inline-axis.
        return 0;
    case ContentPositionRight:
        // The align-content's axis is always orthogonal to the inline-axis.
        return 0;
    case ContentPositionCenter:
        return availableFreeSpace / 2;
    case ContentPositionFlexEnd:
        // Only used in flex layout, for other layout, it's equivalent to 'End'.
    case ContentPositionEnd:
        return availableFreeSpace;
    case ContentPositionFlexStart:
        // Only used in flex layout, for other layout, it's equivalent to 'Start'.
    case ContentPositionStart:
        return 0;
    case ContentPositionBaseline:
    case ContentPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always start align.
        // crbug.com/234191
        return 0;
    case ContentPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutPoint LayoutGrid::findChildLogicalPosition(const LayoutBox& child, LayoutSize contentAlignmentOffset) const
{
    LayoutUnit columnPosition = columnPositionForChild(child);
    // We stored m_columnPositions's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (!style()->isLeftToRightDirection())
        columnPosition = (m_columnPositions[m_columnPositions.size() - 1] + borderAndPaddingLogicalLeft()) - columnPosition - child.logicalWidth();

    // The Content Alignment offset accounts for the RTL to LTR flip.
    LayoutPoint childLocation(columnPosition, rowPositionForChild(child));
    childLocation.move(contentAlignmentOffset);

    return childLocation;
}

void LayoutGrid::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GridPainter(*this).paintChildren(paintInfo, paintOffset);
}

const char* LayoutGrid::name() const
{
    if (isFloating())
        return "LayoutGrid (floating)";
    if (isOutOfFlowPositioned())
        return "LayoutGrid (positioned)";
    if (isAnonymous())
        return "LayoutGrid (generated)";
    if (isRelPositioned())
        return "LayoutGrid (relative positioned)";
    return "LayoutGrid";
}

} // namespace blink
