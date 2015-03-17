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

#ifndef LayoutGrid_h
#define LayoutGrid_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/OrderIterator.h"
#include "core/layout/style/GridResolvedPosition.h"

namespace blink {

struct GridCoordinate;
struct GridSpan;
class GridTrack;
class GridItemWithSpan;

class LayoutGrid final : public LayoutBlock {
public:
    LayoutGrid(Element*);
    virtual ~LayoutGrid();

    virtual const char* name() const override;

    virtual void layoutBlock(bool relayoutChildren) override;

    virtual bool canCollapseAnonymousBlockChild() const override { return false; }

    void dirtyGrid();

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    typedef Vector<LayoutBox*, 1> GridCell;
    const GridCell& gridCell(int row, int column) { return m_grid[row][column]; }
    const Vector<LayoutBox*>& itemsOverflowingGridArea() { return m_gridItemsOverflowingGridArea; }
    int paintIndexForGridItem(const LayoutBox* layoutBox) { return m_gridItemsIndexesMap.get(layoutBox); }

    bool gridIsDirty() const { return m_gridIsDirty; }

    typedef void (GridTrack::* AccumulatorGrowFunction)(LayoutUnit);
private:
    virtual bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutGrid || LayoutBlock::isOfType(type); }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;

    virtual void addChild(LayoutObject* newChild, LayoutObject* beforeChild = 0) override;
    virtual void removeChild(LayoutObject*) override;

    virtual void styleDidChange(StyleDifference, const LayoutStyle*) override;

    bool explicitGridDidResize(const LayoutStyle&) const;
    bool namedGridLinesDefinitionDidChange(const LayoutStyle&) const;

    class GridIterator;
    struct GridSizingData;
    bool gridElementIsShrinkToFit();
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);
    LayoutUnit computeUsedBreadthOfMinLength(GridTrackSizingDirection, const GridLength&) const;
    LayoutUnit computeUsedBreadthOfMaxLength(GridTrackSizingDirection, const GridLength&, LayoutUnit usedBreadth) const;
    LayoutUnit computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection, const Length&) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&);

    void ensureGridSize(size_t maximumRowIndex, size_t maximumColumnIndex);
    void insertItemIntoGrid(LayoutBox&, const GridCoordinate&);
    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    PassOwnPtr<GridCoordinate> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const LayoutBox&, GridTrackSizingDirection, const GridSpan& specifiedPositions) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<LayoutBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<LayoutBox*>&);
    void placeAutoMajorAxisItemOnGrid(LayoutBox&, std::pair<size_t, size_t>& autoPlacementCursor);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void layoutGridItems();
    void layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior = DefaultLayout);
    void offsetAndBreadthForPositionedChild(const LayoutBox&, GridTrackSizingDirection, bool startIsAuto, bool endIsAuto, LayoutUnit& offset, LayoutUnit& breadth);
    void populateGridPositions(const GridSizingData&);

    typedef LayoutUnit (LayoutGrid::* SizingFunction)(LayoutBox&, GridTrackSizingDirection, Vector<GridTrack>&);
    typedef const LayoutUnit& (GridTrack::* AccumulatorGetter)() const;
    typedef bool (GridTrackSize::* FilterFunction)() const;
    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    void resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection, const GridCoordinate&, LayoutBox& gridItem, GridTrack&, Vector<GridTrack>& columnTracks);
    void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, const GridItemsSpanGroupRange&, FilterFunction, SizingFunction, AccumulatorGetter, AccumulatorGrowFunction, FilterFunction growAboveMaxBreadthFilterFunction = nullptr);
    void distributeSpaceToTracks(Vector<GridTrack*>&, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, AccumulatorGetter, GridSizingData&, LayoutUnit& availableLogicalSpace);

    double computeNormalizedFractionBreadth(Vector<GridTrack>&, const GridSpan& tracksSpan, GridTrackSizingDirection, LayoutUnit availableLogicalSpace) const;

    GridTrackSize gridTrackSize(GridTrackSizingDirection, size_t) const;

    LayoutUnit logicalHeightForChild(LayoutBox&, Vector<GridTrack>&);
    LayoutUnit minContentForChild(LayoutBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit maxContentForChild(LayoutBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit startOfColumnForChild(const LayoutBox& child) const;
    LayoutUnit endOfColumnForChild(const LayoutBox& child) const;
    LayoutUnit columnPositionLeft(const LayoutBox&) const;
    LayoutUnit columnPositionRight(const LayoutBox&) const;
    LayoutUnit centeredColumnPositionForChild(const LayoutBox&) const;
    LayoutUnit columnPositionForChild(const LayoutBox&) const;
    LayoutUnit startOfRowForChild(const LayoutBox& child) const;
    LayoutUnit endOfRowForChild(const LayoutBox& child) const;
    LayoutUnit centeredRowPositionForChild(const LayoutBox&) const;
    LayoutUnit rowPositionForChild(const LayoutBox&) const;
    LayoutUnit contentPositionAndDistributionColumnOffset(LayoutUnit availableFreeSpace, ContentPosition, ContentDistributionType, OverflowAlignment, unsigned numberOfItems) const;
    LayoutUnit contentPositionAndDistributionRowOffset(LayoutUnit availableFreeSpace, ContentPosition, ContentDistributionType, OverflowAlignment, unsigned numberOfItems) const;
    LayoutPoint findChildLogicalPosition(const LayoutBox&, LayoutSize contentAlignmentOffset) const;
    GridCoordinate cachedGridCoordinate(const LayoutBox&) const;

    LayoutUnit gridAreaBreadthForChild(const LayoutBox& child, GridTrackSizingDirection, const Vector<GridTrack>&) const;

    virtual void paintChildren(const PaintInfo&, const LayoutPoint&) override;
    bool allowedToStretchLogicalHeightForChild(const LayoutBox& child) const;
    bool needToStretchChildLogicalHeight(const LayoutBox&) const;
    LayoutUnit childIntrinsicHeight(const LayoutBox&) const;
    LayoutUnit childIntrinsicWidth(const LayoutBox&) const;
    LayoutUnit intrinsicLogicalHeightForChild(const LayoutBox&) const;
    LayoutUnit marginLogicalHeightForChild(const LayoutBox&) const;
    LayoutUnit computeMarginLogicalHeightForChild(const LayoutBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const LayoutBox&) const;
    void applyStretchAlignmentToChildIfNeeded(LayoutBox&, LayoutUnit gridAreaBreadthForChild);

#if ENABLE(ASSERT)
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, const Vector<GridTrack>&);
#endif

    size_t gridItemSpan(const LayoutBox&, GridTrackSizingDirection);
    bool spanningItemCrossesFlexibleSizedTracks(const GridCoordinate&, GridTrackSizingDirection) const;

    size_t gridColumnCount() const
    {
        ASSERT(!gridIsDirty());
        return m_grid[0].size();
    }
    size_t gridRowCount() const
    {
        ASSERT(!gridIsDirty());
        return m_grid.size();
    }

    bool hasDefiniteLogicalSize(GridTrackSizingDirection) const;

    typedef Vector<Vector<GridCell>> GridRepresentation;
    GridRepresentation m_grid;
    bool m_gridIsDirty;
    Vector<LayoutUnit> m_rowPositions;
    Vector<LayoutUnit> m_columnPositions;
    HashMap<const LayoutBox*, GridCoordinate> m_gridItemCoordinate;
    OrderIterator m_orderIterator;
    Vector<LayoutBox*> m_gridItemsOverflowingGridArea;
    HashMap<const LayoutBox*, size_t> m_gridItemsIndexesMap;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutGrid, isLayoutGrid());

} // namespace blink

#endif // LayoutGrid_h
