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

#include "core/layout/LayoutGrid.h"

#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/GridPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "core/style/GridArea.h"
#include "platform/LengthFunctions.h"
#include <algorithm>

namespace blink {

static const int infinity = -1;

class GridItemWithSpan;

class GridTrack {
public:
    GridTrack()
        : m_baseSize(0)
        , m_growthLimit(0)
        , m_plannedSize(0)
        , m_sizeDuringDistribution(0)
        , m_infinitelyGrowable(false)
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

    bool growthLimitIsInfinite() const
    {
        return m_growthLimit == infinity;
    }

    bool infiniteGrowthPotential() const
    {
        return growthLimitIsInfinite() || m_infinitelyGrowable;
    }

    const LayoutUnit& plannedSize() const { return m_plannedSize; }

    void setPlannedSize(const LayoutUnit& plannedSize)
    {
        ASSERT(plannedSize >= 0 || plannedSize == infinity);
        m_plannedSize = plannedSize;
    }

    const LayoutUnit& sizeDuringDistribution() { return m_sizeDuringDistribution; }

    void setSizeDuringDistribution(const LayoutUnit& sizeDuringDistribution)
    {
        ASSERT(sizeDuringDistribution >= 0);
        m_sizeDuringDistribution = sizeDuringDistribution;
    }

    void growSizeDuringDistribution(const LayoutUnit& sizeDuringDistribution)
    {
        ASSERT(sizeDuringDistribution >= 0);
        m_sizeDuringDistribution += sizeDuringDistribution;
    }

    bool infinitelyGrowable() const { return m_infinitelyGrowable; }
    void setInfinitelyGrowable(bool infinitelyGrowable) { m_infinitelyGrowable = infinitelyGrowable; }

private:
    bool isGrowthLimitBiggerThanBaseSize() const { return growthLimitIsInfinite() || m_growthLimit >= m_baseSize; }

    void ensureGrowthLimitIsBiggerThanBaseSize()
    {
        if (m_growthLimit != infinity && m_growthLimit < m_baseSize)
            m_growthLimit = m_baseSize;
    }

    LayoutUnit m_baseSize;
    LayoutUnit m_growthLimit;
    LayoutUnit m_plannedSize;
    LayoutUnit m_sizeDuringDistribution;
    bool m_infinitelyGrowable;
};

struct ContentAlignmentData {
    STACK_ALLOCATED();
public:
    ContentAlignmentData() {};
    ContentAlignmentData(LayoutUnit position, LayoutUnit distribution)
        : positionOffset(position)
        , distributionOffset(distribution)
    {
    }

    bool isValid() { return positionOffset >= 0 && distributionOffset >= 0; }

    LayoutUnit positionOffset = LayoutUnit(-1);
    LayoutUnit distributionOffset = LayoutUnit(-1);
};

enum TrackSizeRestriction {
    AllowInfinity,
    ForbidInfinity,
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
        return nullptr;
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

    PassOwnPtr<GridArea> nextEmptyGridArea(size_t fixedTrackSpan, size_t varyingTrackSpan)
    {
        ASSERT(!m_grid.isEmpty());
        ASSERT(fixedTrackSpan >= 1 && varyingTrackSpan >= 1);

        size_t rowSpan = (m_direction == ForColumns) ? varyingTrackSpan : fixedTrackSpan;
        size_t columnSpan = (m_direction == ForColumns) ? fixedTrackSpan : varyingTrackSpan;

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            if (checkEmptyCells(rowSpan, columnSpan)) {
                OwnPtr<GridArea> result = adoptPtr(new GridArea(GridSpan::translatedDefiniteGridSpan(m_rowIndex, m_rowIndex + rowSpan), GridSpan::translatedDefiniteGridSpan(m_columnIndex, m_columnIndex + columnSpan)));
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

    LayoutUnit& freeSpaceForDirection(GridTrackSizingDirection direction) { return direction == ForColumns ? freeSpaceForColumns : freeSpaceForRows; }

private:
    LayoutUnit freeSpaceForColumns { };
    LayoutUnit freeSpaceForRows { };
};

struct GridItemsSpanGroupRange {
    Vector<GridItemWithSpan>::iterator rangeStart;
    Vector<GridItemWithSpan>::iterator rangeEnd;
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

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    dirtyGrid();
}

void LayoutGrid::removeChild(LayoutObject* child)
{
    LayoutBlock::removeChild(child);

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    dirtyGrid();
}

void LayoutGrid::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
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

bool LayoutGrid::explicitGridDidResize(const ComputedStyle& oldStyle) const
{
    return oldStyle.gridTemplateColumns().size() != styleRef().gridTemplateColumns().size()
        || oldStyle.gridTemplateRows().size() != styleRef().gridTemplateRows().size();
}

bool LayoutGrid::namedGridLinesDefinitionDidChange(const ComputedStyle& oldStyle) const
{
    return oldStyle.namedGridRowLines() != styleRef().namedGridRowLines()
        || oldStyle.namedGridColumnLines() != styleRef().namedGridColumnLines();
}

LayoutUnit LayoutGrid::computeTrackBasedLogicalHeight(const GridSizingData& sizingData) const
{
    LayoutUnit logicalHeight;

    for (const auto& row : sizingData.rowTracks)
        logicalHeight += row.baseSize();

    logicalHeight += guttersSize(ForRows, sizingData.rowTracks.size());

    return logicalHeight;
}

void LayoutGrid::computeTrackSizesForDirection(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit freeSpace)
{
    ASSERT(freeSpace >= 0);
    sizingData.freeSpaceForDirection(direction) = freeSpace - guttersSize(direction, direction == ForRows ? gridRowCount() : gridColumnCount());

    LayoutUnit baseSizes, growthLimits;
    computeUsedBreadthOfGridTracks(direction, sizingData, baseSizes, growthLimits, AvailableSpaceDefinite);
    ASSERT(tracksAreWiderThanMinTrackBreadth(direction, sizingData));
}

void LayoutGrid::layoutBlock(bool relayoutChildren)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    {
        // LayoutState needs this deliberate scope to pop before updating scroll information (which
        // may trigger relayout).
        LayoutState state(*this, locationOffset());

        LayoutSize previousSize = size();

        updateLogicalWidth();
        bool logicalHeightWasIndefinite = computeContentLogicalHeight(MainOrPreferredSize, style()->logicalHeight(), LayoutUnit(-1)) == LayoutUnit(-1);

        TextAutosizer::LayoutScope textAutosizerLayoutScope(this);

        placeItemsOnGrid();

        GridSizingData sizingData(gridColumnCount(), gridRowCount());

        // At this point the logical width is always definite as the above call to updateLogicalWidth()
        // properly resolves intrinsic sizes. We cannot do the same for heights though because many code
        // paths inside updateLogicalHeight() require a previous call to setLogicalHeight() to resolve
        // heights properly (like for positioned items for example).
        computeTrackSizesForDirection(ForColumns, sizingData, availableLogicalWidth());

        if (logicalHeightWasIndefinite)
            computeIntrinsicLogicalHeight(sizingData);
        else
            computeTrackSizesForDirection(ForRows, sizingData, availableLogicalHeight(ExcludeMarginBorderPadding));
        setLogicalHeight(computeTrackBasedLogicalHeight(sizingData) + borderAndPaddingLogicalHeight());

        LayoutUnit oldClientAfterEdge = clientLogicalBottom();
        updateLogicalHeight();

        // The above call might have changed the grid's logical height depending on min|max height restrictions.
        // Update the sizes of the rows whose size depends on the logical height (also on definite|indefinite sizes).
        if (logicalHeightWasIndefinite)
            computeTrackSizesForDirection(ForRows, sizingData, contentLogicalHeight());

        // Grid container should have the minimum height of a line if it's editable. That doesn't affect track sizing though.
        if (hasLineIfEmpty())
            setLogicalHeight(std::max(logicalHeight(), minimumLogicalHeightForEmptyLine()));

        applyStretchAlignmentToTracksIfNeeded(ForColumns, sizingData);
        applyStretchAlignmentToTracksIfNeeded(ForRows, sizingData);

        layoutGridItems(sizingData);

        if (size() != previousSize)
            relayoutChildren = true;

        layoutPositionedObjects(relayoutChildren || isDocumentElement());

        computeOverflow(oldClientAfterEdge);
    }

    updateLayerTransformAfterLayout();
    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

LayoutUnit LayoutGrid::guttersSize(GridTrackSizingDirection direction, size_t span) const
{
    ASSERT(span >= 1);

    if (span == 1)
        return LayoutUnit();

    const Length& trackGap = direction == ForColumns ? styleRef().gridColumnGap() : styleRef().gridRowGap();
    return valueForLength(trackGap, LayoutUnit()) * (span - 1);
}

void LayoutGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const_cast<LayoutGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    sizingData.freeSpaceForDirection(ForColumns) = LayoutUnit();
    const_cast<LayoutGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, minLogicalWidth, maxLogicalWidth, AvailableSpaceIndefinite);

    LayoutUnit totalGuttersSize = guttersSize(ForColumns, sizingData.columnTracks.size());
    minLogicalWidth += totalGuttersSize;
    maxLogicalWidth += totalGuttersSize;

    LayoutUnit scrollbarWidth = LayoutUnit(intrinsicScrollbarLogicalWidth());
    minLogicalWidth += scrollbarWidth;
    maxLogicalWidth += scrollbarWidth;
}

void LayoutGrid::computeIntrinsicLogicalHeight(GridSizingData& sizingData)
{
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, sizingData));
    sizingData.freeSpaceForDirection(ForRows) = LayoutUnit();
    computeUsedBreadthOfGridTracks(ForRows, sizingData, m_minContentHeight, m_maxContentHeight, AvailableSpaceIndefinite);

    LayoutUnit totalGuttersSize = guttersSize(ForRows, gridRowCount());
    m_minContentHeight += totalGuttersSize;
    m_maxContentHeight += totalGuttersSize;

    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData));
}

LayoutUnit LayoutGrid::computeIntrinsicLogicalContentHeightUsing(const Length& logicalHeightLength, LayoutUnit intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    if (logicalHeightLength.isMinContent())
        return m_minContentHeight;

    if (logicalHeightLength.isMaxContent())
        return m_maxContentHeight;

    if (logicalHeightLength.isFitContent()) {
        if (m_minContentHeight == -1 || m_maxContentHeight == -1)
            return LayoutUnit(-1);
        LayoutUnit fillAvailableExtent = containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding);
        return std::min<LayoutUnit>(m_maxContentHeight, std::max(m_minContentHeight, fillAvailableExtent));
    }

    if (logicalHeightLength.isFillAvailable())
        return containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding) - borderAndPadding;
    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

static inline double normalizedFlexFraction(const GridTrack& track, double flexFactor)
{
    return track.baseSize() / std::max<double>(1, flexFactor);
}

void LayoutGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& baseSizesWithoutMaximization, LayoutUnit& growthLimitsWithoutMaximization, AvailableSpaceType availableSpaceType)
{
    LayoutUnit& freeSpace = sizingData.freeSpaceForDirection(direction);
    const LayoutUnit initialFreeSpace = freeSpace;
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<size_t> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    const LayoutUnit maxSize = std::max(LayoutUnit(), initialFreeSpace);
    // 1. Initialize per Grid track variables.
    for (size_t i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        GridTrackSize trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.setBaseSize(computeUsedBreadthOfMinLength(minTrackBreadth, maxSize));
        track.setGrowthLimit(computeUsedBreadthOfMaxLength(maxTrackBreadth, track.baseSize(), maxSize));
        track.setInfinitelyGrowable(false);

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData);

    baseSizesWithoutMaximization = growthLimitsWithoutMaximization = LayoutUnit();

    for (const auto& track: tracks) {
        ASSERT(!track.infiniteGrowthPotential());
        baseSizesWithoutMaximization += track.baseSize();
        growthLimitsWithoutMaximization += track.growthLimit();
    }
    freeSpace = initialFreeSpace - baseSizesWithoutMaximization;

    bool hasDefiniteFreeSpace = availableSpaceType == AvailableSpaceDefinite;
    if (hasDefiniteFreeSpace && freeSpace <= 0)
        return;

    // 3. Grow all Grid tracks in GridTracks from their baseSize up to their growthLimit value until freeSpace is exhausted.
    const size_t tracksSize = tracks.size();
    if (hasDefiniteFreeSpace) {
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (size_t i = 0; i < tracksSize; ++i) {
            tracksForDistribution[i] = tracks.data() + i;
            tracksForDistribution[i]->setPlannedSize(tracksForDistribution[i]->baseSize());
        }

        distributeSpaceToTracks<MaximizeTracks>(tracksForDistribution, nullptr, sizingData, freeSpace);

        for (auto* track : tracksForDistribution)
            track->setBaseSize(track->plannedSize());
    } else {
        for (auto& track : tracks)
            track.setBaseSize(track.growthLimit());
    }

    if (flexibleSizedTracksIndex.isEmpty())
        return;

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double flexFraction = 0;
    if (hasDefiniteFreeSpace) {
        flexFraction = findFlexFactorUnitSize(tracks, GridSpan::translatedDefiniteGridSpan(0, tracks.size()), direction, initialFreeSpace);
    } else {
        for (const auto& trackIndex : flexibleSizedTracksIndex)
            flexFraction = std::max(flexFraction, normalizedFlexFraction(tracks[trackIndex], gridTrackSize(direction, trackIndex).maxTrackBreadth().flex()));

        for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (LayoutBox* gridItem = iterator.nextGridItem()) {
                const GridSpan span = cachedGridSpan(*gridItem, direction);

                // Do not include already processed items.
                if (i > 0 && span.resolvedInitialPosition() <= flexibleSizedTracksIndex[i - 1])
                    continue;

                flexFraction = std::max(flexFraction, findFlexFactorUnitSize(tracks, span, direction, maxContentForChild(*gridItem, direction, sizingData.columnTracks)));
            }
        }
    }

    for (const auto& trackIndex : flexibleSizedTracksIndex) {
        GridTrackSize trackSize = gridTrackSize(direction, trackIndex);

        LayoutUnit oldBaseSize = tracks[trackIndex].baseSize();
        LayoutUnit baseSize = std::max<LayoutUnit>(oldBaseSize, flexFraction * trackSize.maxTrackBreadth().flex());
        if (LayoutUnit increment = baseSize - oldBaseSize) {
            tracks[trackIndex].setBaseSize(baseSize);
            freeSpace -= increment;

            baseSizesWithoutMaximization += increment;
            growthLimitsWithoutMaximization += increment;
        }
    }
}

LayoutUnit LayoutGrid::computeUsedBreadthOfMinLength(const GridLength& gridLength, LayoutUnit maxSize) const
{
    if (gridLength.isFlex())
        return LayoutUnit();

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, maxSize);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return LayoutUnit();
}

LayoutUnit LayoutGrid::computeUsedBreadthOfMaxLength(const GridLength& gridLength, LayoutUnit usedBreadth, LayoutUnit maxSize) const
{
    if (gridLength.isFlex())
        return usedBreadth;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, maxSize);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return LayoutUnit(infinity);
}

double LayoutGrid::computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, GridTrackSizingDirection direction, double flexFactorSum, LayoutUnit& leftOverSpace, const Vector<size_t, 8>& flexibleTracksIndexes, PassOwnPtr<TrackIndexSet> tracksToTreatAsInflexible) const
{
    // We want to avoid the effect of flex factors sum below 1 making the factor unit size to grow exponentially.
    double hypotheticalFactorUnitSize = leftOverSpace / std::max<double>(1, flexFactorSum);

    // product of the hypothetical "flex factor unit" and any flexible track's "flex factor" must be grater than such track's "base size".
    OwnPtr<TrackIndexSet> additionalTracksToTreatAsInflexible = tracksToTreatAsInflexible;
    bool validFlexFactorUnit = true;
    for (auto index : flexibleTracksIndexes) {
        if (additionalTracksToTreatAsInflexible && additionalTracksToTreatAsInflexible->contains(index))
            continue;
        LayoutUnit baseSize = tracks[index].baseSize();
        double flexFactor = gridTrackSize(direction, index).maxTrackBreadth().flex();
        // treating all such tracks as inflexible.
        if (baseSize > hypotheticalFactorUnitSize * flexFactor) {
            leftOverSpace -= baseSize;
            flexFactorSum -= flexFactor;
            if (!additionalTracksToTreatAsInflexible)
                additionalTracksToTreatAsInflexible = adoptPtr(new TrackIndexSet());
            additionalTracksToTreatAsInflexible->add(index);
            validFlexFactorUnit = false;
        }
    }
    if (!validFlexFactorUnit)
        return computeFlexFactorUnitSize(tracks, direction, flexFactorSum, leftOverSpace, flexibleTracksIndexes, additionalTracksToTreatAsInflexible.release());
    return hypotheticalFactorUnitSize;
}

double LayoutGrid::findFlexFactorUnitSize(const Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit leftOverSpace) const
{
    if (leftOverSpace <= 0)
        return 0;

    double flexFactorSum = 0;
    Vector<size_t, 8> flexibleTracksIndexes;
    for (const auto& trackIndex : tracksSpan) {
        GridTrackSize trackSize = gridTrackSize(direction, trackIndex);
        if (!trackSize.maxTrackBreadth().isFlex()) {
            leftOverSpace -= tracks[trackIndex].baseSize();
        } else {
            flexibleTracksIndexes.append(trackIndex);
            flexFactorSum += trackSize.maxTrackBreadth().flex();
        }
    }

    // The function is not called if we don't have <flex> grid tracks
    ASSERT(!flexibleTracksIndexes.isEmpty());

    return computeFlexFactorUnitSize(tracks, direction, flexFactorSum, leftOverSpace, flexibleTracksIndexes);
}

bool LayoutGrid::hasDefiniteLogicalSize(GridTrackSizingDirection direction) const
{
    return (direction == ForRows) ? hasDefiniteLogicalHeight() : hasDefiniteLogicalWidth();
}

GridTrackSize LayoutGrid::gridTrackSize(GridTrackSizingDirection direction, size_t i) const
{
    bool isForColumns = direction == ForColumns;
    const Vector<GridTrackSize>& trackStyles = isForColumns ? style()->gridTemplateColumns() : style()->gridTemplateRows();
    const GridTrackSize& autoTrackSize = isForColumns ? style()->gridAutoColumns() : style()->gridAutoRows();
    int translatedIndex = i + (isForColumns ? m_smallestColumnStart : m_smallestRowStart);
    const GridTrackSize& trackSize = (translatedIndex < 0 || translatedIndex >= static_cast<int>(trackStyles.size())) ? autoTrackSize : trackStyles[translatedIndex];

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();

    // If the logical width/height of the grid container is indefinite, percentage values are treated as <auto>
    if (minTrackBreadth.hasPercentage() || maxTrackBreadth.hasPercentage()) {
        if (!hasDefiniteLogicalSize(direction)) {
            if (minTrackBreadth.hasPercentage())
                minTrackBreadth = Length(Auto);
            if (maxTrackBreadth.hasPercentage())
                maxTrackBreadth = Length(Auto);
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
        layoutScope.setNeedsLayout(&child, LayoutInvalidationReason::GridChanged);
    }

    bool hasOverrideHeight = child.hasOverrideLogicalContentHeight();
    // We need to clear the stretched height to properly compute logical height during layout.
    if (hasOverrideHeight && child.needsLayout())
        child.clearOverrideLogicalContentHeight();

    child.setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
    // If |child| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the override logical height to -1 (no possible resolution).
    if (child.hasRelativeLogicalHeight())
        child.setOverrideContainingBlockContentLogicalHeight(LayoutUnit(-1));
    child.layoutIfNeeded();
    // If the child was stretched we should use its intrinsic height.
    return (hasOverrideHeight ? childIntrinsicHeight(child) : child.logicalHeight()) + child.marginLogicalHeight();
}

LayoutUnit LayoutGrid::minSizeForChild(LayoutBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // TODO(svillar): Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    const Length& childMinSize = direction == ForColumns ? child.style()->logicalMinWidth() : child.style()->logicalMinHeight();
    if (childMinSize.isAuto()) {
        // TODO(svillar): Implement intrinsic aspect ratio support (transferred size in specs).
        return minContentForChild(child, direction, columnTracks);
    }

    if (direction == ForColumns)
        return child.computeLogicalWidthUsing(MinSize, childMinSize, contentLogicalWidth(), this);

    return child.computeContentLogicalHeight(MinSize, childMinSize, child.logicalHeight()) + child.scrollbarLogicalHeight();
}

LayoutUnit LayoutGrid::minContentForChild(LayoutBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the override logical width to -1 (no possible resolution).
        if (child.hasRelativeLogicalWidth())
            child.setOverrideContainingBlockContentLogicalWidth(LayoutUnit(-1));

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
            child.setOverrideContainingBlockContentLogicalWidth(LayoutUnit(-1));

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalHeightForChild(child, columnTracks);
}

// We're basically using a class instead of a std::pair because of accessing gridItem() or gridSpan() is much more
// self-explanatory that using .first or .second members in the pair. Having a std::pair<LayoutBox*, size_t>
// does not work either because we still need the GridSpan so we'd have to add an extra hash lookup for each item
// at the beginning of LayoutGrid::resolveContentBasedTrackSizingFunctionsForItems().
class GridItemWithSpan {
public:
    GridItemWithSpan(LayoutBox& gridItem, const GridSpan& gridSpan)
        : m_gridItem(&gridItem)
        , m_gridSpan(gridSpan)
    {
    }

    LayoutBox& gridItem() const { return *m_gridItem; }
    GridSpan gridSpan() const { return m_gridSpan; }

    bool operator<(const GridItemWithSpan other) const { return m_gridSpan.integerSpan() < other.m_gridSpan.integerSpan(); }

private:
    LayoutBox* m_gridItem;
    GridSpan m_gridSpan;
};

bool LayoutGrid::spanningItemCrossesFlexibleSizedTracks(const GridSpan& span, GridTrackSizingDirection direction) const
{
    for (const auto& trackPosition : span) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition);
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
}

void LayoutGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    sizingData.itemsSortedByIncreasingSpan.shrink(0);
    HashSet<LayoutBox*> itemsSet;
    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridIterator iterator(m_grid, direction, trackIndex);
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        while (LayoutBox* gridItem = iterator.nextGridItem()) {
            if (itemsSet.add(gridItem).isNewEntry) {
                const GridSpan& span = cachedGridSpan(*gridItem, direction);
                if (span.integerSpan() == 1) {
                    resolveContentBasedTrackSizingFunctionsForNonSpanningItems(direction, span, *gridItem, track, sizingData.columnTracks);
                } else if (!spanningItemCrossesFlexibleSizedTracks(span, direction)) {
                    sizingData.itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, span));
                }
            }
        }
    }
    std::sort(sizingData.itemsSortedByIncreasingSpan.begin(), sizingData.itemsSortedByIncreasingSpan.end());

    auto it = sizingData.itemsSortedByIncreasingSpan.begin();
    auto end = sizingData.itemsSortedByIncreasingSpan.end();
    while (it != end) {
        GridItemsSpanGroupRange spanGroupRange = { it, std::upper_bound(it, end, *it) };
        resolveContentBasedTrackSizingFunctionsForItems<ResolveIntrinsicMinimums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveContentBasedMinimums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveMaxContentMinimums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveIntrinsicMaximums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveMaxContentMaximums>(direction, sizingData, spanGroupRange);
        it = spanGroupRange.rangeEnd;
    }

    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        if (track.growthLimitIsInfinite())
            track.setGrowthLimit(track.baseSize());
    }
}

void LayoutGrid::resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection direction, const GridSpan& span, LayoutBox& gridItem, GridTrack& track, Vector<GridTrack>& columnTracks)
{
    const size_t trackPosition = span.resolvedInitialPosition();
    GridTrackSize trackSize = gridTrackSize(direction, trackPosition);

    if (trackSize.hasMinContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), maxContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasAutoMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minSizeForChild(gridItem, direction, columnTracks)));

    if (trackSize.hasMinContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), maxContentForChild(gridItem, direction, columnTracks)));
}

static const LayoutUnit& trackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrack& track, TrackSizeRestriction restriction)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
    case MaximizeTracks:
        return track.baseSize();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        const LayoutUnit& growthLimit = track.growthLimit();
        if (restriction == AllowInfinity)
            return growthLimit;
        return growthLimit == infinity ? track.baseSize() : growthLimit;
    }

    ASSERT_NOT_REACHED();
    return track.baseSize();
}

static bool shouldProcessTrackForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
        return trackSize.hasIntrinsicMinTrackBreadth();
    case ResolveContentBasedMinimums:
        return trackSize.hasMinOrMaxContentMinTrackBreadth();
    case ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadth();
    case ResolveIntrinsicMaximums:
        return trackSize.hasMinOrMaxContentMaxTrackBreadth();
    case ResolveMaxContentMaximums:
        return trackSize.hasMaxContentOrAutoMaxTrackBreadth();
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
        return trackSize.hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        return true;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static void markAsInfinitelyGrowableForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
        return;
    case ResolveIntrinsicMaximums:
        if (trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity) == infinity  && track.plannedSize() != infinity)
            track.setInfinitelyGrowable(true);
        return;
    case ResolveMaxContentMaximums:
        if (track.infinitelyGrowable())
            track.setInfinitelyGrowable(false);
        return;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

static void updateTrackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
        track.setBaseSize(track.plannedSize());
        return;
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        track.setGrowthLimit(track.plannedSize());
        return;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

LayoutUnit LayoutGrid::currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, LayoutBox& gridItem, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
        return minSizeForChild(gridItem, direction, columnTracks);
    case ResolveContentBasedMinimums:
    case ResolveIntrinsicMaximums:
        return minContentForChild(gridItem, direction, columnTracks);
    case ResolveMaxContentMinimums:
    case ResolveMaxContentMaximums:
        return maxContentForChild(gridItem, direction, columnTracks);
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return LayoutUnit();
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

template <TrackSizeComputationPhase phase>
void LayoutGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, const GridItemsSpanGroupRange& gridItemsWithSpan)
{
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = tracks[trackIndex];
        track.setPlannedSize(trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity));
    }

    for (auto it = gridItemsWithSpan.rangeStart; it != gridItemsWithSpan.rangeEnd; ++it) {
        GridItemWithSpan& gridItemWithSpan = *it;
        ASSERT(gridItemWithSpan.gridSpan().integerSpan() > 1);
        const GridSpan& itemSpan = gridItemWithSpan.gridSpan();

        sizingData.growBeyondGrowthLimitsTracks.shrink(0);
        sizingData.filteredTracks.shrink(0);
        LayoutUnit spanningTracksSize;
        for (const auto& trackPosition : itemSpan) {
            GridTrackSize trackSize = gridTrackSize(direction, trackPosition);
            GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackPosition] : sizingData.rowTracks[trackPosition];
            spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                continue;

            sizingData.filteredTracks.append(&track);

            if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                sizingData.growBeyondGrowthLimitsTracks.append(&track);
        }

        if (sizingData.filteredTracks.isEmpty())
            continue;

        spanningTracksSize += guttersSize(direction, itemSpan.integerSpan());

        LayoutUnit extraSpace = currentItemSizeForTrackSizeComputationPhase(phase, gridItemWithSpan.gridItem(), direction, sizingData.columnTracks) - spanningTracksSize;
        extraSpace = extraSpace.clampNegativeToZero();
        auto& tracksToGrowBeyondGrowthLimits = sizingData.growBeyondGrowthLimitsTracks.isEmpty() ? sizingData.filteredTracks : sizingData.growBeyondGrowthLimitsTracks;
        distributeSpaceToTracks<phase>(sizingData.filteredTracks, &tracksToGrowBeyondGrowthLimits, sizingData, extraSpace);
    }

    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = tracks[trackIndex];
        markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
        updateTrackSizeForTrackSizeComputationPhase(phase, track);
    }
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    // This check ensures that we respect the irreflexivity property of the strict weak ordering required by std::sort
    // (forall x: NOT x < x).
    if (track1->infiniteGrowthPotential() && track2->infiniteGrowthPotential())
        return false;

    if (track1->infiniteGrowthPotential() || track2->infiniteGrowthPotential())
        return track2->infiniteGrowthPotential();

    return (track1->growthLimit() - track1->baseSize()) < (track2->growthLimit() - track2->baseSize());
}

template <TrackSizeComputationPhase phase>
void LayoutGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    ASSERT(availableLogicalSpace >= 0);

    for (auto* track : tracks)
        track->setSizeDuringDistribution(trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity));

    if (availableLogicalSpace > 0) {
        std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

        size_t tracksSize = tracks.size();
        for (size_t i = 0; i < tracksSize; ++i) {
            GridTrack& track = *tracks[i];
            LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
            const LayoutUnit& trackBreadth = trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            LayoutUnit growthShare = track.infiniteGrowthPotential() ? availableLogicalSpaceShare : std::min(availableLogicalSpaceShare, track.growthLimit() - trackBreadth);
            ASSERT_WITH_MESSAGE(growthShare >= 0, "We must never shrink any grid track or else we can't guarantee we abide by our min-sizing function.");
            track.growSizeDuringDistribution(growthShare);
            availableLogicalSpace -= growthShare;
        }
    }

    if (availableLogicalSpace > 0 && growBeyondGrowthLimitsTracks) {
        size_t tracksGrowingAboveMaxBreadthSize = growBeyondGrowthLimitsTracks->size();
        for (size_t i = 0; i < tracksGrowingAboveMaxBreadthSize; ++i) {
            GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
            LayoutUnit growthShare = availableLogicalSpace / (tracksGrowingAboveMaxBreadthSize - i);
            track->growSizeDuringDistribution(growthShare);
            availableLogicalSpace -= growthShare;
        }
    }

    for (auto* track : tracks)
        track->setPlannedSize(track->plannedSize() == infinity ? track->sizeDuringDistribution() : std::max(track->plannedSize(), track->sizeDuringDistribution()));
}

#if ENABLE(ASSERT)
bool LayoutGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    const Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    LayoutUnit& maxSize = sizingData.freeSpaceForDirection(direction);
    for (size_t i = 0; i < tracks.size(); ++i) {
        GridTrackSize trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(minTrackBreadth, maxSize) > tracks[i].baseSize())
            return false;
    }
    return true;
}
#endif

void LayoutGrid::ensureGridSize(size_t maximumRowSize, size_t maximumColumnSize)
{
    const size_t oldRowSize = gridRowCount();
    if (maximumRowSize > oldRowSize) {
        m_grid.grow(maximumRowSize);
        for (size_t row = oldRowSize; row < gridRowCount(); ++row)
            m_grid[row].grow(gridColumnCount());
    }

    if (maximumColumnSize > gridColumnCount()) {
        for (size_t row = 0; row < gridRowCount(); ++row)
            m_grid[row].grow(maximumColumnSize);
    }
}

void LayoutGrid::insertItemIntoGrid(LayoutBox& child, const GridArea& area)
{
    RELEASE_ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    ensureGridSize(area.rows.resolvedFinalPosition(), area.columns.resolvedFinalPosition());

    for (const auto& row : area.rows) {
        for (const auto& column: area.columns)
            m_grid[row][column].append(&child);
    }
}

void LayoutGrid::placeItemsOnGrid()
{
    if (!m_gridIsDirty)
        return;

    ASSERT(m_gridItemArea.isEmpty());

    populateExplicitGridAndOrderIterator();

    // We clear the dirty bit here as the grid sizes have been updated.
    m_gridIsDirty = false;

    Vector<LayoutBox*> autoMajorAxisAutoGridItems;
    Vector<LayoutBox*> specifiedMajorAxisAutoGridItems;
    for (LayoutBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        GridArea area = cachedGridArea(*child);
        if (!area.rows.isIndefinite())
            area.rows.translate(abs(m_smallestRowStart));
        if (!area.columns.isIndefinite())
            area.columns.translate(abs(m_smallestColumnStart));
        m_gridItemArea.set(child, area);

        if (area.rows.isIndefinite() || area.columns.isIndefinite()) {
            GridSpan majorAxisPositions = (autoPlacementMajorAxisDirection() == ForColumns) ? area.columns : area.rows;
            if (majorAxisPositions.isIndefinite())
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        insertItemIntoGrid(*child, area);
    }

    ASSERT(gridRowCount() >= GridPositionsResolver::explicitGridRowCount(*style()));
    ASSERT(gridColumnCount() >= GridPositionsResolver::explicitGridColumnCount(*style()));

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);

    m_grid.shrinkToFit();

#if ENABLE(ASSERT)
    for (LayoutBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        GridArea area = cachedGridArea(*child);
        ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    }
#endif
}

void LayoutGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(m_orderIterator);

    m_smallestRowStart = m_smallestColumnStart = 0;

    size_t maximumRowIndex = std::max<size_t>(1, GridPositionsResolver::explicitGridRowCount(*style()));
    size_t maximumColumnIndex = std::max<size_t>(1, GridPositionsResolver::explicitGridColumnCount(*style()));

    ASSERT(m_gridItemsIndexesMap.isEmpty());
    size_t childIndex = 0;
    for (LayoutBox* child = firstChildBox(); child; child = child->nextInFlowSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        populator.collectChild(child);
        m_gridItemsIndexesMap.set(child, childIndex++);

        // This function bypasses the cache (cachedGridArea()) as it is used to build it.
        GridSpan rowPositions = GridPositionsResolver::resolveGridPositionsFromStyle(*style(), *child, ForRows);
        GridSpan columnPositions = GridPositionsResolver::resolveGridPositionsFromStyle(*style(), *child, ForColumns);
        m_gridItemArea.set(child, GridArea(rowPositions, columnPositions));

        // |positions| is 0 if we need to run the auto-placement algorithm.
        if (!rowPositions.isIndefinite()) {
            m_smallestRowStart = std::min(m_smallestRowStart, rowPositions.untranslatedResolvedInitialPosition());
            maximumRowIndex = std::max<int>(maximumRowIndex, rowPositions.untranslatedResolvedFinalPosition());
        } else {
            // Grow the grid for items with a definite row span, getting the largest such span.
            size_t spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), *child, ForRows);
            maximumRowIndex = std::max(maximumRowIndex, spanSize);
        }

        if (!columnPositions.isIndefinite()) {
            m_smallestColumnStart = std::min(m_smallestColumnStart, columnPositions.untranslatedResolvedInitialPosition());
            maximumColumnIndex = std::max<int>(maximumColumnIndex, columnPositions.untranslatedResolvedFinalPosition());
        } else {
            // Grow the grid for items with a definite column span, getting the largest such span.
            size_t spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), *child, ForColumns);
            maximumColumnIndex = std::max(maximumColumnIndex, spanSize);
        }
    }

    m_grid.grow(maximumRowIndex + abs(m_smallestRowStart));
    for (auto& column : m_grid)
        column.grow(maximumColumnIndex + abs(m_smallestColumnStart));
}

PassOwnPtr<GridArea> LayoutGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const LayoutBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == ForColumns ? ForRows : ForColumns;
    const size_t endOfCrossDirection = crossDirection == ForColumns ? gridColumnCount() : gridRowCount();
    size_t crossDirectionSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), gridItem, crossDirection);
    GridSpan crossDirectionPositions = GridSpan::translatedDefiniteGridSpan(endOfCrossDirection, endOfCrossDirection + crossDirectionSpanSize);
    return adoptPtr(new GridArea(specifiedDirection == ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == ForColumns ? specifiedPositions : crossDirectionPositions));
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
        GridSpan majorAxisPositions = cachedGridSpan(*autoGridItem, autoPlacementMajorAxisDirection());
        ASSERT(majorAxisPositions.isTranslatedDefinite());
        ASSERT(!cachedGridSpan(*autoGridItem, autoPlacementMinorAxisDirection()).isTranslatedDefinite());
        size_t minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), *autoGridItem, autoPlacementMinorAxisDirection());
        unsigned majorAxisInitialPosition = majorAxisPositions.resolvedInitialPosition();

        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions.resolvedInitialPosition(), isGridAutoFlowDense ? 0 : minorAxisCursors.get(majorAxisInitialPosition));
        OwnPtr<GridArea> emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisSpanSize);
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(*autoGridItem, autoPlacementMajorAxisDirection(), majorAxisPositions);

        m_gridItemArea.set(autoGridItem, *emptyGridArea);
        insertItemIntoGrid(*autoGridItem, *emptyGridArea);

        if (!isGridAutoFlowDense)
            minorAxisCursors.set(majorAxisInitialPosition, isForColumns ? emptyGridArea->rows.resolvedInitialPosition() : emptyGridArea->columns.resolvedInitialPosition());
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
    GridSpan minorAxisPositions = cachedGridSpan(gridItem, autoPlacementMinorAxisDirection());
    ASSERT(!cachedGridSpan(gridItem, autoPlacementMajorAxisDirection()).isTranslatedDefinite());
    size_t majorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), gridItem, autoPlacementMajorAxisDirection());

    const size_t endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
    size_t majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    size_t minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    OwnPtr<GridArea> emptyGridArea;
    if (minorAxisPositions.isTranslatedDefinite()) {
        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions.resolvedInitialPosition() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisPositions.resolvedInitialPosition(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions.integerSpan(), majorAxisSpanSize);
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    } else {
        size_t minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), gridItem, autoPlacementMinorAxisDirection());

        for (size_t majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisSpanSize, minorAxisSpanSize);

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                size_t minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == ForColumns ? emptyGridArea->columns.resolvedFinalPosition() : emptyGridArea->rows.resolvedFinalPosition();
                const size_t endOfMinorAxis = autoPlacementMinorAxisDirection() == ForColumns ? gridColumnCount() : gridRowCount();
                if (minorAxisFinalPositionIndex <= endOfMinorAxis)
                    break;

                // Discard empty grid area as it does not fit in the minor axis direction.
                // We don't need to create a new empty grid area yet as we might find a valid one in the next iteration.
                emptyGridArea = nullptr;
            }

            // As we're moving to the next track in the major axis we should reset the auto-placement cursor in the minor axis.
            minorAxisAutoPlacementCursor = 0;
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), GridSpan::translatedDefiniteGridSpan(0, minorAxisSpanSize));
    }

    m_gridItemArea.set(&gridItem, *emptyGridArea);
    insertItemIntoGrid(gridItem, *emptyGridArea);
    // Move auto-placement cursor to the new position.
    autoPlacementCursor.first = emptyGridArea->rows.resolvedInitialPosition();
    autoPlacementCursor.second = emptyGridArea->columns.resolvedInitialPosition();
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
    if (m_gridIsDirty)
        return;

    // Even if this could be redundant, it could be seen as a defensive strategy against
    // style changes events happening during the layout phase or even while the painting process
    // is still ongoing.
    // Forcing a new layout for the Grid layout would cancel any ongoing painting and ensure
    // the grid and its children are correctly laid out according to the new style rules.
    setNeedsLayout(LayoutInvalidationReason::GridChanged);

    m_grid.resize(0);
    m_gridItemArea.clear();
    m_gridItemsOverflowingGridArea.resize(0);
    m_gridItemsIndexesMap.clear();
    m_gridIsDirty = true;
}

void LayoutGrid::applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    LayoutUnit& availableSpace = sizingData.freeSpaceForDirection(direction);
    if (availableSpace <= 0
        || (direction == ForColumns && styleRef().justifyContentDistribution() != ContentDistributionStretch)
        || (direction == ForRows && styleRef().alignContentDistribution() != ContentDistributionStretch))
        return;

    // Spec defines auto-sized tracks as the ones with an 'auto' max-sizing function.
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> autoSizedTracksIndex;
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        // If there is some flexible-sized track, they should have exhausted available space during sizing algorithm.
        ASSERT(!trackSize.maxTrackBreadth().isFlex());
        if (trackSize.hasAutoMaxTrackBreadth())
            autoSizedTracksIndex.append(i);
    }

    unsigned numberOfAutoSizedTracks = autoSizedTracksIndex.size();
    if (numberOfAutoSizedTracks < 1)
        return;

    LayoutUnit sizeToIncrease = availableSpace / numberOfAutoSizedTracks;
    for (const auto& trackIndex : autoSizedTracksIndex) {
        GridTrack* track = tracks.data() + trackIndex;
        LayoutUnit baseSize = track->baseSize() + sizeToIncrease;
        track->setBaseSize(baseSize);
    }
    availableSpace = LayoutUnit();
}

void LayoutGrid::layoutGridItems(GridSizingData& sizingData)
{
    populateGridPositions(sizingData);
    m_gridItemsOverflowingGridArea.resize(0);

    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned()) {
            prepareChildForPositionedLayout(*child);
            continue;
        }

        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        LayoutUnit oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForColumns, sizingData);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForRows, sizingData);

        SubtreeLayoutScope layoutScope(*child);
        if (oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth || (oldOverrideContainingBlockContentLogicalHeight != overrideContainingBlockContentLogicalHeight && child->hasRelativeLogicalHeight()))
            layoutScope.setNeedsLayout(child, LayoutInvalidationReason::GridChanged);

        child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
        child->setOverrideContainingBlockContentLogicalHeight(overrideContainingBlockContentLogicalHeight);

        // Stretching logic might force a child layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that child margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToChildIfNeeded(*child);

        child->layoutIfNeeded();

        // We need pending layouts to be done in order to compute auto-margins properly.
        updateAutoMarginsInColumnAxisIfNeeded(*child);
        updateAutoMarginsInRowAxisIfNeeded(*child);

#if ENABLE(ASSERT)
        const GridArea& area = cachedGridArea(*child);
        ASSERT(area.columns.resolvedInitialPosition() < sizingData.columnTracks.size());
        ASSERT(area.rows.resolvedInitialPosition() < sizingData.rowTracks.size());
#endif
        child->setLogicalLocation(findChildLogicalPosition(*child, sizingData));

        // Keep track of children overflowing their grid area as we might need to paint them even if the grid-area is
        // not visible
        if (child->logicalHeight() > overrideContainingBlockContentLogicalHeight
            || child->logicalWidth() > overrideContainingBlockContentLogicalWidth)
            m_gridItemsOverflowingGridArea.append(child);
    }
}

void LayoutGrid::prepareChildForPositionedLayout(LayoutBox& child)
{
    ASSERT(child.isOutOfFlowPositioned());
    child.containingBlock()->insertPositionedObject(&child);

    PaintLayer* childLayer = child.layer();
    childLayer->setStaticInlinePosition(borderAndPaddingStart());
    childLayer->setStaticBlockPosition(borderAndPaddingBefore());
}

void LayoutGrid::layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior info)
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    bool containerHasHorizontalWritingMode = isHorizontalWritingMode();
    for (auto* child : *positionedDescendants) {
        bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != containerHasHorizontalWritingMode;
        if (hasOrthogonalWritingMode) {
            // FIXME: Properly support orthogonal writing mode.
            continue;
        }

        LayoutUnit columnOffset = LayoutUnit();
        LayoutUnit columnBreadth = LayoutUnit();
        offsetAndBreadthForPositionedChild(*child, ForColumns, columnOffset, columnBreadth);
        LayoutUnit rowOffset = LayoutUnit();
        LayoutUnit rowBreadth = LayoutUnit();
        offsetAndBreadthForPositionedChild(*child, ForRows, rowOffset, rowBreadth);

        child->setOverrideContainingBlockContentLogicalWidth(columnBreadth);
        child->setOverrideContainingBlockContentLogicalHeight(rowBreadth);
        child->setExtraInlineOffset(columnOffset);
        child->setExtraBlockOffset(rowOffset);
    }

    LayoutBlock::layoutPositionedObjects(relayoutChildren, info);
}

void LayoutGrid::offsetAndBreadthForPositionedChild(const LayoutBox& child, GridTrackSizingDirection direction, LayoutUnit& offset, LayoutUnit& breadth)
{
    ASSERT(child.isHorizontalWritingMode() == isHorizontalWritingMode());
    bool isForColumns = direction == ForColumns;

    GridSpan positions = GridPositionsResolver::resolveGridPositionsFromStyle(*style(), child, direction);
    if (positions.isIndefinite()) {
        offset = LayoutUnit();
        breadth = isForColumns ? clientLogicalWidth() : clientLogicalHeight();
        return;
    }

    // For positioned items we cannot use GridSpan::translate(). Because we could end up with negative values, as the positioned items do not create implicit tracks per spec.
    int smallestStart = abs(isForColumns ? m_smallestColumnStart : m_smallestRowStart);
    int resolvedInitialPosition = positions.untranslatedResolvedInitialPosition() + smallestStart;
    int resolvedFinalPosition = positions.untranslatedResolvedFinalPosition() + smallestStart;

    GridPosition startPosition = isForColumns ? child.style()->gridColumnStart() : child.style()->gridRowStart();
    GridPosition endPosition = isForColumns ? child.style()->gridColumnEnd() : child.style()->gridRowEnd();
    int firstExplicitLine = smallestStart;
    int lastExplicitLine = (isForColumns ? GridPositionsResolver::explicitGridColumnCount(styleRef()) : GridPositionsResolver::explicitGridRowCount(styleRef())) + smallestStart;

    bool startIsAuto = startPosition.isAuto()
        || (startPosition.isNamedGridArea() && !GridPositionsResolver::isValidNamedLineOrArea(startPosition.namedGridLine(), styleRef(), GridPositionsResolver::initialPositionSide(direction)))
        || (resolvedInitialPosition < firstExplicitLine)
        || (resolvedInitialPosition > lastExplicitLine);
    bool endIsAuto = endPosition.isAuto()
        || (endPosition.isNamedGridArea() && !GridPositionsResolver::isValidNamedLineOrArea(endPosition.namedGridLine(), styleRef(), GridPositionsResolver::finalPositionSide(direction)))
        || (resolvedFinalPosition < firstExplicitLine)
        || (resolvedFinalPosition > lastExplicitLine);

    size_t initialPosition = startIsAuto ? 0 : resolvedInitialPosition;
    size_t finalPosition = endIsAuto ? lastExplicitLine : resolvedFinalPosition;

    LayoutUnit start = startIsAuto ? LayoutUnit() : isForColumns ?  m_columnPositions[initialPosition] : m_rowPositions[initialPosition];
    LayoutUnit end = endIsAuto ? isForColumns ? logicalWidth() : logicalHeight() : isForColumns ?  m_columnPositions[finalPosition] : m_rowPositions[finalPosition];

    breadth = end - start;

    if (startIsAuto)
        breadth -= isForColumns ? borderStart() : borderBefore();
    else
        start -= isForColumns ? borderStart() : borderBefore();

    if (endIsAuto) {
        breadth -= isForColumns ? borderEnd() : borderAfter();
        breadth -= scrollbarLogicalWidth();
    }

    offset = start;

    if (child.parent() == this && !startIsAuto) {
        // If column/row start is "auto" the static position has been already set in prepareChildForPositionedLayout().
        PaintLayer* childLayer = child.layer();
        if (isForColumns)
            childLayer->setStaticInlinePosition(borderStart() + offset);
        else
            childLayer->setStaticBlockPosition(borderBefore() + offset);
    }
}

GridArea LayoutGrid::cachedGridArea(const LayoutBox& gridItem) const
{
    ASSERT(m_gridItemArea.contains(&gridItem));
    return m_gridItemArea.get(&gridItem);
}

GridSpan LayoutGrid::cachedGridSpan(const LayoutBox& gridItem, GridTrackSizingDirection direction) const
{
    GridArea area = cachedGridArea(gridItem);
    return direction == ForColumns ? area.columns : area.rows;
}

LayoutUnit LayoutGrid::gridAreaBreadthForChild(const LayoutBox& child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridSpan& span = cachedGridSpan(child, direction);
    LayoutUnit gridAreaBreadth;
    for (const auto& trackPosition : span)
        gridAreaBreadth += tracks[trackPosition].baseSize();

    gridAreaBreadth += guttersSize(direction, span.integerSpan());

    return gridAreaBreadth;
}

LayoutUnit LayoutGrid::gridAreaBreadthForChildIncludingAlignmentOffsets(const LayoutBox& child, GridTrackSizingDirection direction, const GridSizingData& sizingData) const
{
    // We need the cached value when available because Content Distribution alignment properties
    // may have some influence in the final grid area breadth.
    const Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    const GridSpan& span = cachedGridSpan(child, direction);
    const Vector<LayoutUnit>& linePositions = (direction == ForColumns) ? m_columnPositions : m_rowPositions;
    LayoutUnit initialTrackPosition = linePositions[span.resolvedInitialPosition()];
    LayoutUnit finalTrackPosition = linePositions[span.resolvedFinalPosition() - 1];
    // Track Positions vector stores the 'start' grid line of each track, so w have to add last track's baseSize.
    return finalTrackPosition - initialTrackPosition + tracks[span.resolvedFinalPosition() - 1].baseSize();
}

void LayoutGrid::populateGridPositions(GridSizingData& sizingData)
{
    // Since we add alignment offsets and track gutters, grid lines are not always adjacent. Hence we will have to
    // assume from now on that we just store positions of the initial grid lines of each track,
    // except the last one, which is the only one considered as a final grid line of a track.
    // FIXME: This will affect the computed style value of grid tracks size, since we are
    // using these positions to compute them.

    unsigned numberOfTracks = sizingData.columnTracks.size();
    unsigned numberOfLines = numberOfTracks + 1;
    unsigned lastLine = numberOfLines - 1;
    unsigned nextToLastLine = numberOfLines - 2;
    ContentAlignmentData offset = computeContentPositionAndDistributionOffset(ForColumns, sizingData.freeSpaceForDirection(ForColumns), numberOfTracks);
    LayoutUnit trackGap = guttersSize(ForColumns, 2);
    m_columnPositions.resize(numberOfLines);
    m_columnPositions[0] = borderAndPaddingStart() + offset.positionOffset;
    for (unsigned i = 0; i < lastLine; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + offset.distributionOffset + sizingData.columnTracks[i].baseSize() + trackGap;
    m_columnPositions[lastLine] = m_columnPositions[nextToLastLine] + sizingData.columnTracks[nextToLastLine].baseSize();

    numberOfTracks = sizingData.rowTracks.size();
    numberOfLines = numberOfTracks + 1;
    lastLine = numberOfLines - 1;
    nextToLastLine = numberOfLines - 2;
    offset = computeContentPositionAndDistributionOffset(ForRows, sizingData.freeSpaceForDirection(ForRows), numberOfTracks);
    trackGap = guttersSize(ForRows, 2);
    m_rowPositions.resize(numberOfLines);
    m_rowPositions[0] = borderAndPaddingBefore() + offset.positionOffset;
    for (unsigned i = 0; i < lastLine; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + offset.distributionOffset + sizingData.rowTracks[i].baseSize() + trackGap;
    m_rowPositions[lastLine] = m_rowPositions[nextToLastLine] + sizingData.rowTracks[nextToLastLine].baseSize();
}

static LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit trackBreadth, LayoutUnit childBreadth)
{
    LayoutUnit offset = trackBreadth - childBreadth;
    switch (overflow) {
    case OverflowAlignmentSafe:
        // If overflow is 'safe', we have to make sure we don't overflow the 'start'
        // edge (potentially cause some data loss as the overflow is unreachable).
        return offset.clampNegativeToZero();
    case OverflowAlignmentUnsafe:
    case OverflowAlignmentDefault:
        // If we overflow our alignment container and overflow is 'true' (default), we
        // ignore the overflow and just return the value regardless (which may cause data
        // loss as we overflow the 'start' edge).
        return offset;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

static inline LayoutUnit constrainedChildIntrinsicContentLogicalHeight(const LayoutBox& child)
{
    LayoutUnit childIntrinsicContentLogicalHeight = child.intrinsicContentLogicalHeight();
    return child.constrainLogicalHeightByMinMax(childIntrinsicContentLogicalHeight + child.borderAndPaddingLogicalHeight(), childIntrinsicContentLogicalHeight);
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
bool LayoutGrid::needToStretchChildLogicalHeight(const LayoutBox& child) const
{
    if (ComputedStyle::resolveAlignment(styleRef(), child.styleRef(), ItemPositionStretch) != ItemPositionStretch)
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
    if (!child.styleRef().hasMargin())
        return LayoutUnit();

    LayoutUnit marginBefore;
    LayoutUnit marginAfter;
    child.computeMarginsForDirection(BlockDirection, this, child.containingBlockLogicalWidthForContent(), child.logicalHeight(), marginBefore, marginAfter,
        child.style()->marginBeforeUsing(style()),
        child.style()->marginAfterUsing(style()));

    return marginBefore + marginAfter;
}

LayoutUnit LayoutGrid::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const LayoutBox& child) const
{
    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // children are laid out, so we can't use the child cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    return gridAreaBreadthForChild - (child.needsLayout() ? computeMarginLogicalHeightForChild(child) : marginLogicalHeightForChild(child));
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
void LayoutGrid::applyStretchAlignmentToChildIfNeeded(LayoutBox& child)
{
    // We clear height override values because we will decide now whether it's allowed or
    // not, evaluating the conditions which might have changed since the old values were set.
    child.clearOverrideLogicalContentHeight();

    auto& childStyle = child.styleRef();
    bool isHorizontalMode = isHorizontalWritingMode();
    bool hasAutoSizeInColumnAxis = isHorizontalMode ? childStyle.height().isAuto() : childStyle.width().isAuto();
    bool allowedToStretchChildAlongColumnAxis = hasAutoSizeInColumnAxis && !childStyle.marginBeforeUsing(style()).isAuto() && !childStyle.marginAfterUsing(style()).isAuto();
    if (allowedToStretchChildAlongColumnAxis && ComputedStyle::resolveAlignment(styleRef(), childStyle, ItemPositionStretch) == ItemPositionStretch) {
        // TODO (lajava): If the child has orthogonal flow, then it already has an override height set, so use it.
        // TODO (lajava): grid track sizing and positioning do not support orthogonal modes yet.
        if (child.isHorizontalWritingMode() == isHorizontalMode) {
            LayoutUnit stretchedLogicalHeight = availableAlignmentSpaceForChildBeforeStretching(child.overrideContainingBlockContentLogicalHeight(), child);
            LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, LayoutUnit(-1));
            child.setOverrideLogicalContentHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
            if (desiredLogicalHeight != child.logicalHeight()) {
                // TODO (lajava): Can avoid laying out here in some cases. See https://webkit.org/b/87905.
                child.setLogicalHeight(0);
                child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
            }
        }
    }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
bool LayoutGrid::hasAutoMarginsInColumnAxis(const LayoutBox& child) const
{
    if (isHorizontalWritingMode())
        return child.style()->marginTop().isAuto() || child.style()->marginBottom().isAuto();
    return child.style()->marginLeft().isAuto() || child.style()->marginRight().isAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
bool LayoutGrid::hasAutoMarginsInRowAxis(const LayoutBox& child) const
{
    if (isHorizontalWritingMode())
        return child.style()->marginLeft().isAuto() || child.style()->marginRight().isAuto();
    return child.style()->marginTop().isAuto() || child.style()->marginBottom().isAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
void LayoutGrid::updateAutoMarginsInRowAxisIfNeeded(LayoutBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());

    LayoutUnit availableAlignmentSpace = child.overrideContainingBlockContentLogicalWidth() - child.logicalWidth() - child.marginLogicalWidth();
    if (availableAlignmentSpace <= 0)
        return;

    Length marginStart = child.style()->marginStartUsing(style());
    Length marginEnd = child.style()->marginEndUsing(style());
    if (marginStart.isAuto() && marginEnd.isAuto()) {
        child.setMarginStart(availableAlignmentSpace / 2, style());
        child.setMarginEnd(availableAlignmentSpace / 2, style());
    } else if (marginStart.isAuto()) {
        child.setMarginStart(availableAlignmentSpace, style());
    } else if (marginEnd.isAuto()) {
        child.setMarginEnd(availableAlignmentSpace, style());
    }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be moved to LayoutBox.
void LayoutGrid::updateAutoMarginsInColumnAxisIfNeeded(LayoutBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());

    LayoutUnit availableAlignmentSpace = child.overrideContainingBlockContentLogicalHeight() - child.logicalHeight() - child.marginLogicalHeight();
    if (availableAlignmentSpace <= 0)
        return;

    Length marginBefore = child.style()->marginBeforeUsing(style());
    Length marginAfter = child.style()->marginAfterUsing(style());
    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        child.setMarginBefore(availableAlignmentSpace / 2, style());
        child.setMarginAfter(availableAlignmentSpace / 2, style());
    } else if (marginBefore.isAuto()) {
        child.setMarginBefore(availableAlignmentSpace, style());
    } else if (marginAfter.isAuto()) {
        child.setMarginAfter(availableAlignmentSpace, style());
    }
}

GridAxisPosition LayoutGrid::columnAxisPositionForChild(const LayoutBox& child) const
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    bool hasSameWritingMode = child.styleRef().writingMode() == styleRef().writingMode();

    switch (ComputedStyle::resolveAlignment(styleRef(), child.styleRef(), ItemPositionStretch)) {
    case ItemPositionSelfStart:
        // If orthogonal writing-modes, this computes to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        // self-start is based on the child's block axis direction. That's why we need to check against the grid container's block flow.
        return (hasOrthogonalWritingMode || hasSameWritingMode) ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
        // If orthogonal writing-modes, this computes to 'end'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        // self-end is based on the child's block axis direction. That's why we need to check against the grid container's block flow.
        return (hasOrthogonalWritingMode || hasSameWritingMode) ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
        // The alignment axis (column axis) and the inline axis are parallell in
        // orthogonal writing mode. Otherwise this this is equivalent to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        return GridAxisStart;
    case ItemPositionRight:
        // The alignment axis (column axis) and the inline axis are parallell in
        // orthogonal writing mode. Otherwise this this is equivalent to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        return hasOrthogonalWritingMode ? GridAxisEnd : GridAxisStart;
    case ItemPositionCenter:
        return GridAxisCenter;
    case ItemPositionFlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPositionStart:
        return GridAxisStart;
    case ItemPositionFlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPositionEnd:
        return GridAxisEnd;
    case ItemPositionStretch:
        return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: These two require implementing Baseline Alignment. For now, we always 'start' align the child.
        // crbug.com/234191
        return GridAxisStart;
    case ItemPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

GridAxisPosition LayoutGrid::rowAxisPositionForChild(const LayoutBox& child) const
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    bool hasSameDirection = child.styleRef().direction() == styleRef().direction();
    bool isLTR = styleRef().isLeftToRightDirection();

    switch (ComputedStyle::resolveJustification(styleRef(), child.styleRef(), ItemPositionStretch)) {
    case ItemPositionSelfStart:
        // For orthogonal writing-modes, this computes to 'start'
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        // self-start is based on the child's direction. That's why we need to check against the grid container's direction.
        return (hasOrthogonalWritingMode || hasSameDirection) ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
        // For orthogonal writing-modes, this computes to 'start'
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        return (hasOrthogonalWritingMode || hasSameDirection) ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
        return isLTR ? GridAxisStart : GridAxisEnd;
    case ItemPositionRight:
        return isLTR ? GridAxisEnd : GridAxisStart;
    case ItemPositionCenter:
        return GridAxisCenter;
    case ItemPositionFlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPositionStart:
        return GridAxisStart;
    case ItemPositionFlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPositionEnd:
        return GridAxisEnd;
    case ItemPositionStretch:
        return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: These two require implementing Baseline Alignment. For now, we always 'start' align the child.
        // crbug.com/234191
        return GridAxisStart;
    case ItemPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

static inline LayoutUnit offsetBetweenTracks(ContentDistributionType distribution, const Vector<LayoutUnit>& trackPositions, const LayoutUnit& childBreadth)
{
    return (distribution == ContentDistributionStretch || ContentDistributionStretch == ContentDistributionDefault) ? LayoutUnit() : trackPositions[1] - trackPositions[0] - childBreadth;

}

LayoutUnit LayoutGrid::columnAxisOffsetForChild(const LayoutBox& child) const
{
    const GridSpan& rowsSpan = cachedGridSpan(child, ForRows);
    size_t childStartLine = rowsSpan.resolvedInitialPosition();
    LayoutUnit startOfRow = m_rowPositions[childStartLine];
    LayoutUnit startPosition = startOfRow + marginBeforeForChild(child);
    if (hasAutoMarginsInColumnAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = columnAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition;
    case GridAxisEnd:
    case GridAxisCenter: {
        size_t childEndLine = rowsSpan.resolvedFinalPosition();
        LayoutUnit endOfRow = m_rowPositions[childEndLine];
        // m_rowPositions include gutters so we need to subtract them to get the actual end position for a given
        // row (this does not have to be done for the last track as there are no more m_rowPositions after it)
        if (childEndLine < m_rowPositions.size() - 1)
            endOfRow -= guttersSize(ForRows, 2);
        LayoutUnit childBreadth = child.logicalHeight() + child.marginLogicalHeight();
        if (childEndLine - childStartLine > 1 && childEndLine < m_rowPositions.size() - 1)
            endOfRow -= offsetBetweenTracks(styleRef().alignContentDistribution(), m_rowPositions, childBreadth);
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(child.styleRef().alignSelfOverflowAlignment(), endOfRow - startOfRow, childBreadth);
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

LayoutUnit LayoutGrid::rowAxisOffsetForChild(const LayoutBox& child) const
{
    const GridSpan& columnsSpan = cachedGridSpan(child, ForColumns);
    size_t childStartLine = columnsSpan.resolvedInitialPosition();
    LayoutUnit startOfColumn = m_columnPositions[childStartLine];
    LayoutUnit startPosition = startOfColumn + marginStartForChild(child);
    if (hasAutoMarginsInRowAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = rowAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition;
    case GridAxisEnd:
    case GridAxisCenter: {
        size_t childEndLine = columnsSpan.resolvedFinalPosition();
        LayoutUnit endOfColumn = m_columnPositions[childEndLine];
        // m_columnPositions include gutters so we need to subtract them to get the actual end position for a given
        // column (this does not have to be done for the last track as there are no more m_columnPositions after it)
        if (childEndLine < m_columnPositions.size() - 1)
            endOfColumn -= guttersSize(ForRows, 2);
        LayoutUnit childBreadth = child.logicalWidth() + child.marginLogicalWidth();
        if (childEndLine - childStartLine > 1 && childEndLine < m_columnPositions.size() - 1)
            endOfColumn -= offsetBetweenTracks(styleRef().justifyContentDistribution(), m_columnPositions, childBreadth);
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(child.styleRef().justifySelfOverflowAlignment(), endOfColumn - startOfColumn, childBreadth);
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
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
    return isLeftToRight ? LayoutUnit() : availableSpace;
}

static inline LayoutUnit offsetToEndEdge(bool isLeftToRight, LayoutUnit availableSpace)
{
    return !isLeftToRight ? LayoutUnit() : availableSpace;
}


static ContentAlignmentData contentDistributionOffset(const LayoutUnit& availableFreeSpace, ContentPosition& fallbackPosition, ContentDistributionType distribution, unsigned numberOfGridTracks)
{
    if (distribution != ContentDistributionDefault && fallbackPosition == ContentPositionAuto)
        fallbackPosition = resolveContentDistributionFallback(distribution);

    if (availableFreeSpace <= 0)
        return {};

    LayoutUnit distributionOffset;
    switch (distribution) {
    case ContentDistributionSpaceBetween:
        if (numberOfGridTracks < 2)
            return {};
        return {LayoutUnit(), availableFreeSpace / (numberOfGridTracks - 1)};
    case ContentDistributionSpaceAround:
        if (numberOfGridTracks < 1)
            return {};
        distributionOffset = availableFreeSpace / numberOfGridTracks;
        return {distributionOffset / 2, distributionOffset};
    case ContentDistributionSpaceEvenly:
        distributionOffset = availableFreeSpace / (numberOfGridTracks + 1);
        return {distributionOffset, distributionOffset};
    case ContentDistributionStretch:
        return {LayoutUnit(), LayoutUnit()};
    case ContentDistributionDefault:
        return {};
    }

    ASSERT_NOT_REACHED();
    return {};
}

ContentAlignmentData LayoutGrid::computeContentPositionAndDistributionOffset(GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const
{
    bool isRowAxis = direction == ForColumns;
    ContentPosition position = isRowAxis ? styleRef().justifyContentPosition() : styleRef().alignContentPosition();
    ContentDistributionType distribution = isRowAxis ? styleRef().justifyContentDistribution() : styleRef().alignContentDistribution();
    // If <content-distribution> value can't be applied, 'position' will become the associated
    // <content-position> fallback value.
    ContentAlignmentData contentAlignment = contentDistributionOffset(availableFreeSpace, position, distribution, numberOfGridTracks);
    if (contentAlignment.isValid())
        return contentAlignment;

    OverflowAlignment overflow = isRowAxis ? styleRef().justifyContentOverflowAlignment() : styleRef().alignContentOverflowAlignment();
    if (availableFreeSpace <= 0 && overflow == OverflowAlignmentSafe)
        return {LayoutUnit(), LayoutUnit()};

    switch (position) {
    case ContentPositionLeft:
        // The align-content's axis is always orthogonal to the inline-axis.
        return {LayoutUnit(), LayoutUnit()};
    case ContentPositionRight:
        if (isRowAxis)
            return {availableFreeSpace, LayoutUnit()};
        // The align-content's axis is always orthogonal to the inline-axis.
        return {LayoutUnit(), LayoutUnit()};
    case ContentPositionCenter:
        return {availableFreeSpace / 2, LayoutUnit()};
    case ContentPositionFlexEnd: // Only used in flex layout, for other layout, it's equivalent to 'End'.
    case ContentPositionEnd:
        if (isRowAxis)
            return {offsetToEndEdge(styleRef().isLeftToRightDirection(), availableFreeSpace), LayoutUnit()};
        return {availableFreeSpace, LayoutUnit()};
    case ContentPositionFlexStart: // Only used in flex layout, for other layout, it's equivalent to 'Start'.
    case ContentPositionStart:
        if (isRowAxis)
            return {offsetToStartEdge(styleRef().isLeftToRightDirection(), availableFreeSpace), LayoutUnit()};
        return {LayoutUnit(), LayoutUnit()};
    case ContentPositionBaseline:
    case ContentPositionLastBaseline:
        // FIXME: These two require implementing Baseline Alignment. For now, we always 'start' align the child.
        // crbug.com/234191
        if (isRowAxis)
            return {offsetToStartEdge(styleRef().isLeftToRightDirection(), availableFreeSpace), LayoutUnit()};
        return {LayoutUnit(), LayoutUnit()};
    case ContentPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return {LayoutUnit(), LayoutUnit()};
}

LayoutPoint LayoutGrid::findChildLogicalPosition(const LayoutBox& child, GridSizingData& sizingData) const
{
    LayoutUnit rowAxisOffset = rowAxisOffsetForChild(child);
    // We stored m_columnPosition s's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (!style()->isLeftToRightDirection()) {
        LayoutUnit alignmentOffset =  m_columnPositions[0] - borderAndPaddingStart();
        LayoutUnit rightGridEdgePosition = m_columnPositions[m_columnPositions.size() - 1] + alignmentOffset + borderAndPaddingLogicalLeft();
        rowAxisOffset = rightGridEdgePosition - (rowAxisOffset + child.logicalWidth());
    }

    return LayoutPoint(rowAxisOffset, columnAxisOffsetForChild(child));
}

void LayoutGrid::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    GridPainter(*this).paintChildren(paintInfo, paintOffset);
}

} // namespace blink
