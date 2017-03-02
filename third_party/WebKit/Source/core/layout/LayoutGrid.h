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

#include "core/layout/Grid.h"
#include "core/layout/GridTrackSizingAlgorithm.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/OrderIterator.h"
#include "core/style/GridPositionsResolver.h"
#include <memory>

namespace blink {

struct ContentAlignmentData;
struct GridArea;
struct GridSpan;

enum GridAxisPosition { GridAxisStart, GridAxisEnd, GridAxisCenter };

class LayoutGrid final : public LayoutBlock {
 public:
  explicit LayoutGrid(Element*);
  ~LayoutGrid() override;

  static LayoutGrid* createAnonymous(Document*);
  const char* name() const override { return "LayoutGrid"; }

  void layoutBlock(bool relayoutChildren) override;

  void dirtyGrid();

  Vector<LayoutUnit> trackSizesForComputedStyle(GridTrackSizingDirection) const;

  const Vector<LayoutUnit>& columnPositions() const {
    DCHECK(!m_grid.needsItemsPlacement());
    return m_columnPositions;
  }

  const Vector<LayoutUnit>& rowPositions() const {
    DCHECK(!m_grid.needsItemsPlacement());
    return m_rowPositions;
  }

  const GridCell& gridCell(int row, int column) const {
    SECURITY_DCHECK(!m_grid.needsItemsPlacement());
    return m_grid.cell(row, column);
  }

  const Vector<LayoutBox*>& itemsOverflowingGridArea() const {
    SECURITY_DCHECK(!m_grid.needsItemsPlacement());
    return m_gridItemsOverflowingGridArea;
  }

  int paintIndexForGridItem(const LayoutBox* layoutBox) const {
    SECURITY_DCHECK(!m_grid.needsItemsPlacement());
    return m_grid.gridItemPaintOrder(*layoutBox);
  }

  size_t autoRepeatCountForDirection(GridTrackSizingDirection direction) const {
    return m_grid.autoRepeatTracks(direction);
  }

  LayoutUnit translateRTLCoordinate(LayoutUnit) const;

  // TODO(svillar): We need these for the GridTrackSizingAlgorithm. Let's figure
  // it out how to remove this dependency.
  LayoutUnit guttersSize(const Grid&,
                         GridTrackSizingDirection,
                         size_t startLine,
                         size_t span,
                         SizingOperation) const;
  bool cachedHasDefiniteLogicalHeight() const;
  bool isOrthogonalChild(const LayoutBox&) const;

 protected:
  ItemPosition selfAlignmentNormalBehavior(
      const LayoutBox* child = nullptr) const override {
    DCHECK(child);
    return child->isLayoutReplaced() ? ItemPositionStart : ItemPositionStretch;
  }

 private:
  bool isOfType(LayoutObjectType type) const override {
    return type == LayoutObjectLayoutGrid || LayoutBlock::isOfType(type);
  }
  void computeIntrinsicLogicalWidths(
      LayoutUnit& minLogicalWidth,
      LayoutUnit& maxLogicalWidth) const override;

  LayoutUnit computeIntrinsicLogicalContentHeightUsing(
      const Length& logicalHeightLength,
      LayoutUnit intrinsicContentHeight,
      LayoutUnit borderAndPadding) const override;

  void addChild(LayoutObject* newChild,
                LayoutObject* beforeChild = nullptr) override;
  void removeChild(LayoutObject*) override;

  void styleDidChange(StyleDifference, const ComputedStyle*) override;

  bool explicitGridDidResize(const ComputedStyle&) const;
  bool namedGridLinesDefinitionDidChange(const ComputedStyle&) const;

  size_t computeAutoRepeatTracksCount(GridTrackSizingDirection,
                                      SizingOperation) const;
  size_t clampAutoRepeatTracks(GridTrackSizingDirection,
                               size_t autoRepeatTracks) const;

  std::unique_ptr<OrderedTrackIndexSet> computeEmptyTracksForAutoRepeat(
      Grid&,
      GridTrackSizingDirection) const;

  void placeItemsOnGrid(Grid&, SizingOperation) const;
  void populateExplicitGridAndOrderIterator(Grid&) const;
  std::unique_ptr<GridArea> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
      const Grid&,
      const LayoutBox&,
      GridTrackSizingDirection,
      const GridSpan& specifiedPositions) const;
  void placeSpecifiedMajorAxisItemsOnGrid(Grid&,
                                          const Vector<LayoutBox*>&) const;
  void placeAutoMajorAxisItemsOnGrid(Grid&, const Vector<LayoutBox*>&) const;
  void placeAutoMajorAxisItemOnGrid(
      Grid&,
      LayoutBox&,
      std::pair<size_t, size_t>& autoPlacementCursor) const;
  GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
  GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

  void computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&,
                                          GridTrackSizingDirection,
                                          Grid&,
                                          LayoutUnit& minIntrinsicSize,
                                          LayoutUnit& maxIntrinsicSize) const;
  LayoutUnit computeTrackBasedLogicalHeight() const;
  void computeTrackSizesForDefiniteSize(GridTrackSizingDirection,
                                        LayoutUnit freeSpace);

  void repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns,
                                  LayoutUnit availableSpaceForRows);

  void layoutGridItems();
  void prepareChildForPositionedLayout(LayoutBox&);
  void layoutPositionedObjects(
      bool relayoutChildren,
      PositionedLayoutBehavior = DefaultLayout) override;
  void offsetAndBreadthForPositionedChild(const LayoutBox&,
                                          GridTrackSizingDirection,
                                          LayoutUnit& offset,
                                          LayoutUnit& breadth);
  void populateGridPositionsForDirection(GridTrackSizingDirection);

  GridAxisPosition columnAxisPositionForChild(const LayoutBox&) const;
  GridAxisPosition rowAxisPositionForChild(const LayoutBox&) const;
  LayoutUnit rowAxisOffsetForChild(const LayoutBox&) const;
  LayoutUnit columnAxisOffsetForChild(const LayoutBox&) const;
  ContentAlignmentData computeContentPositionAndDistributionOffset(
      GridTrackSizingDirection,
      const LayoutUnit& availableFreeSpace,
      unsigned numberOfGridTracks) const;
  LayoutPoint gridAreaLogicalPosition(const GridArea&) const;
  LayoutPoint findChildLogicalPosition(const LayoutBox&) const;

  LayoutUnit gridAreaBreadthForChildIncludingAlignmentOffsets(
      const LayoutBox&,
      GridTrackSizingDirection) const;

  void applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection);

  void paintChildren(const PaintInfo&, const LayoutPoint&) const override;

  LayoutUnit marginLogicalHeightForChild(const LayoutBox&) const;
  LayoutUnit computeMarginLogicalSizeForChild(MarginDirection,
                                              const LayoutBox&) const;
  LayoutUnit availableAlignmentSpaceForChildBeforeStretching(
      LayoutUnit gridAreaBreadthForChild,
      const LayoutBox&) const;
  StyleSelfAlignmentData justifySelfForChild(const LayoutBox&) const;
  StyleSelfAlignmentData alignSelfForChild(const LayoutBox&) const;
  void applyStretchAlignmentToChildIfNeeded(LayoutBox&);
  bool hasAutoSizeInColumnAxis(const LayoutBox& child) const {
    return isHorizontalWritingMode() ? child.styleRef().height().isAuto()
                                     : child.styleRef().width().isAuto();
  }
  bool hasAutoSizeInRowAxis(const LayoutBox& child) const {
    return isHorizontalWritingMode() ? child.styleRef().width().isAuto()
                                     : child.styleRef().height().isAuto();
  }
  bool allowedToStretchChildAlongColumnAxis(const LayoutBox& child) const {
    return alignSelfForChild(child).position() == ItemPositionStretch &&
           hasAutoSizeInColumnAxis(child) && !hasAutoMarginsInColumnAxis(child);
  }
  bool allowedToStretchChildAlongRowAxis(const LayoutBox& child) const {
    return justifySelfForChild(child).position() == ItemPositionStretch &&
           hasAutoSizeInRowAxis(child) && !hasAutoMarginsInRowAxis(child);
  }
  bool hasAutoMarginsInColumnAxis(const LayoutBox&) const;
  bool hasAutoMarginsInRowAxis(const LayoutBox&) const;
  void updateAutoMarginsInColumnAxisIfNeeded(LayoutBox&);
  void updateAutoMarginsInRowAxisIfNeeded(LayoutBox&);

  int baselinePosition(
      FontBaseline,
      bool firstLine,
      LineDirectionMode,
      LinePositionMode = PositionOnContainingLine) const override;
  int firstLineBoxBaseline() const override;
  int inlineBlockBaseline(LineDirectionMode) const override;
  bool isInlineBaselineAlignedChild(const LayoutBox* child) const;

  LayoutUnit gridGapForDirection(GridTrackSizingDirection,
                                 SizingOperation) const;

  size_t gridItemSpan(const LayoutBox&, GridTrackSizingDirection);

  GridTrackSizingDirection flowAwareDirectionForChild(
      const LayoutBox&,
      GridTrackSizingDirection) const;

  size_t numTracks(GridTrackSizingDirection, const Grid&) const;

  Grid m_grid;
  GridTrackSizingAlgorithm m_trackSizingAlgorithm;

  Vector<LayoutUnit> m_rowPositions;
  Vector<LayoutUnit> m_columnPositions;
  LayoutUnit m_offsetBetweenColumns;
  LayoutUnit m_offsetBetweenRows;
  Vector<LayoutBox*> m_gridItemsOverflowingGridArea;

  LayoutUnit m_minContentHeight{-1};
  LayoutUnit m_maxContentHeight{-1};

  Optional<bool> m_hasDefiniteLogicalHeight;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutGrid, isLayoutGrid());

}  // namespace blink

#endif  // LayoutGrid_h
