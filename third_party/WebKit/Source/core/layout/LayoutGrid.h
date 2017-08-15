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

#include <memory>
#include "core/layout/BaselineAlignment.h"
#include "core/layout/Grid.h"
#include "core/layout/GridTrackSizingAlgorithm.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/OrderIterator.h"
#include "core/style/GridPositionsResolver.h"

namespace blink {

struct ContentAlignmentData;
struct GridArea;
struct GridSpan;

enum GridAxisPosition { kGridAxisStart, kGridAxisEnd, kGridAxisCenter };
enum GridAxis { kGridRowAxis, kGridColumnAxis };

class LayoutGrid final : public LayoutBlock {
 public:
  explicit LayoutGrid(Element*);
  ~LayoutGrid() override;

  static LayoutGrid* CreateAnonymous(Document*);
  const char* GetName() const override { return "LayoutGrid"; }

  void UpdateBlockLayout(bool relayout_children) override;

  void DirtyGrid();

  Vector<LayoutUnit> TrackSizesForComputedStyle(GridTrackSizingDirection) const;

  const Vector<LayoutUnit>& ColumnPositions() const {
    DCHECK(!grid_.NeedsItemsPlacement());
    return column_positions_;
  }

  const Vector<LayoutUnit>& RowPositions() const {
    DCHECK(!grid_.NeedsItemsPlacement());
    return row_positions_;
  }

  const GridCell& GetGridCell(int row, int column) const {
    SECURITY_DCHECK(!grid_.NeedsItemsPlacement());
    return grid_.Cell(row, column);
  }

  const Vector<LayoutBox*>& ItemsOverflowingGridArea() const {
    SECURITY_DCHECK(!grid_.NeedsItemsPlacement());
    return grid_items_overflowing_grid_area_;
  }

  int PaintIndexForGridItem(const LayoutBox* layout_box) const {
    SECURITY_DCHECK(!grid_.NeedsItemsPlacement());
    return grid_.GridItemPaintOrder(*layout_box);
  }

  size_t AutoRepeatCountForDirection(GridTrackSizingDirection direction) const {
    return grid_.AutoRepeatTracks(direction);
  }

  LayoutUnit TranslateRTLCoordinate(LayoutUnit) const;

  // TODO(svillar): We need these for the GridTrackSizingAlgorithm. Let's figure
  // it out how to remove this dependency.
  LayoutUnit GuttersSize(const Grid&,
                         GridTrackSizingDirection,
                         size_t start_line,
                         size_t span,
                         Optional<LayoutUnit> available_size) const;
  bool CachedHasDefiniteLogicalHeight() const;
  bool IsOrthogonalChild(const LayoutBox&) const;
  bool IsBaselineContextComputed(GridAxis) const;
  bool IsBaselineAlignmentForChild(const LayoutBox&,
                                   GridAxis = kGridColumnAxis) const;
  const BaselineGroup& GetBaselineGroupForChild(const LayoutBox&,
                                                GridAxis) const;

  LayoutUnit GridGap(GridTrackSizingDirection) const;

 protected:
  ItemPosition SelfAlignmentNormalBehavior(
      const LayoutBox* child = nullptr) const override {
    DCHECK(child);
    return child->IsLayoutReplaced() ? kItemPositionStart
                                     : kItemPositionStretch;
  }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutGrid || LayoutBlock::IsOfType(type);
  }
  void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const override;

  LayoutUnit ComputeIntrinsicLogicalContentHeightUsing(
      const Length& logical_height_length,
      LayoutUnit intrinsic_content_height,
      LayoutUnit border_and_padding) const override;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;

  bool SelfAlignmentChangedSize(GridAxis,
                                const ComputedStyle& old_style,
                                const ComputedStyle& new_style,
                                const LayoutBox&) const;
  bool DefaultAlignmentChangedSize(GridAxis,
                                   const ComputedStyle& old_style,
                                   const ComputedStyle& new_style) const;
  void StyleDidChange(StyleDifference, const ComputedStyle*) override;

  Optional<LayoutUnit> AvailableSpaceForGutters(GridTrackSizingDirection) const;

  bool ExplicitGridDidResize(const ComputedStyle&) const;
  bool NamedGridLinesDefinitionDidChange(const ComputedStyle&) const;

  size_t ComputeAutoRepeatTracksCount(
      GridTrackSizingDirection,
      Optional<LayoutUnit> available_size) const;
  size_t ClampAutoRepeatTracks(GridTrackSizingDirection,
                               size_t auto_repeat_tracks) const;

  std::unique_ptr<OrderedTrackIndexSet> ComputeEmptyTracksForAutoRepeat(
      Grid&,
      GridTrackSizingDirection) const;

  void PlaceItemsOnGrid(Grid&,
                        Optional<LayoutUnit> available_logical_width) const;
  void PopulateExplicitGridAndOrderIterator(Grid&) const;
  std::unique_ptr<GridArea> CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
      const Grid&,
      const LayoutBox&,
      GridTrackSizingDirection,
      const GridSpan& specified_positions) const;
  void PlaceSpecifiedMajorAxisItemsOnGrid(Grid&,
                                          const Vector<LayoutBox*>&) const;
  void PlaceAutoMajorAxisItemsOnGrid(Grid&, const Vector<LayoutBox*>&) const;
  void PlaceAutoMajorAxisItemOnGrid(
      Grid&,
      LayoutBox&,
      std::pair<size_t, size_t>& auto_placement_cursor) const;
  GridTrackSizingDirection AutoPlacementMajorAxisDirection() const;
  GridTrackSizingDirection AutoPlacementMinorAxisDirection() const;

  void ComputeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&,
                                          GridTrackSizingDirection,
                                          Grid&,
                                          LayoutUnit& min_intrinsic_size,
                                          LayoutUnit& max_intrinsic_size) const;
  LayoutUnit ComputeTrackBasedLogicalHeight() const;
  void ComputeTrackSizesForDefiniteSize(GridTrackSizingDirection,
                                        LayoutUnit free_space);

  void RepeatTracksSizingIfNeeded(LayoutUnit available_space_for_columns,
                                  LayoutUnit available_space_for_rows);

  void LayoutGridItems();
  void PrepareChildForPositionedLayout(LayoutBox&);
  void LayoutPositionedObjects(
      bool relayout_children,
      PositionedLayoutBehavior = kDefaultLayout) override;
  void OffsetAndBreadthForPositionedChild(const LayoutBox&,
                                          GridTrackSizingDirection,
                                          LayoutUnit& offset,
                                          LayoutUnit& breadth);
  void PopulateGridPositionsForDirection(GridTrackSizingDirection);

  GridAxisPosition ColumnAxisPositionForChild(const LayoutBox&) const;
  GridAxisPosition RowAxisPositionForChild(const LayoutBox&) const;
  LayoutUnit RowAxisOffsetForChild(const LayoutBox&) const;
  LayoutUnit ColumnAxisOffsetForChild(const LayoutBox&) const;
  ContentAlignmentData ComputeContentPositionAndDistributionOffset(
      GridTrackSizingDirection,
      const LayoutUnit& available_free_space,
      unsigned number_of_grid_tracks) const;
  LayoutPoint GridAreaLogicalPosition(const GridArea&) const;
  LayoutPoint FindChildLogicalPosition(const LayoutBox&) const;

  LayoutUnit GridAreaBreadthForChildIncludingAlignmentOffsets(
      const LayoutBox&,
      GridTrackSizingDirection) const;

  void ApplyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection);

  void PaintChildren(const PaintInfo&, const LayoutPoint&) const override;

  LayoutUnit AvailableAlignmentSpaceForChildBeforeStretching(
      LayoutUnit grid_area_breadth_for_child,
      const LayoutBox&) const;
  StyleSelfAlignmentData JustifySelfForChild(
      const LayoutBox&,
      const ComputedStyle* = nullptr) const;
  StyleSelfAlignmentData AlignSelfForChild(
      const LayoutBox&,
      const ComputedStyle* = nullptr) const;
  StyleSelfAlignmentData SelfAlignmentForChild(
      GridAxis,
      const LayoutBox& child,
      const ComputedStyle* = nullptr) const;
  StyleSelfAlignmentData DefaultAlignmentForChild(GridAxis,
                                                  const ComputedStyle&) const;
  void ApplyStretchAlignmentToChildIfNeeded(LayoutBox&);
  bool HasAutoSizeInColumnAxis(const LayoutBox& child) const {
    return IsHorizontalWritingMode() ? child.StyleRef().Height().IsAuto()
                                     : child.StyleRef().Width().IsAuto();
  }
  bool HasAutoSizeInRowAxis(const LayoutBox& child) const {
    return IsHorizontalWritingMode() ? child.StyleRef().Width().IsAuto()
                                     : child.StyleRef().Height().IsAuto();
  }
  bool AllowedToStretchChildAlongColumnAxis(const LayoutBox& child) const {
    return AlignSelfForChild(child).GetPosition() == kItemPositionStretch &&
           HasAutoSizeInColumnAxis(child) && !HasAutoMarginsInColumnAxis(child);
  }
  bool AllowedToStretchChildAlongRowAxis(const LayoutBox& child) const {
    return JustifySelfForChild(child).GetPosition() == kItemPositionStretch &&
           HasAutoSizeInRowAxis(child) && !HasAutoMarginsInRowAxis(child);
  }
  bool HasAutoMarginsInColumnAxis(const LayoutBox&) const;
  bool HasAutoMarginsInRowAxis(const LayoutBox&) const;
  void UpdateAutoMarginsInColumnAxisIfNeeded(LayoutBox&);
  void UpdateAutoMarginsInRowAxisIfNeeded(LayoutBox&);

  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  bool IsHorizontalGridAxis(GridAxis) const;
  bool IsParallelToBlockAxisForChild(const LayoutBox&, GridAxis) const;
  bool IsDescentBaselineForChild(const LayoutBox&, GridAxis) const;

  LayoutUnit MarginOverForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit MarginUnderForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit LogicalAscentForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit AscentForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit DescentForChild(const LayoutBox&,
                             LayoutUnit ascent,
                             GridAxis) const;

  bool BaselineMayAffectIntrinsicSize(GridTrackSizingDirection) const;
  void ComputeBaselineAlignmentContext();
  void UpdateBaselineAlignmentContextIfNeeded(LayoutBox&, GridAxis);

  LayoutUnit ColumnAxisBaselineOffsetForChild(const LayoutBox&) const;
  LayoutUnit RowAxisBaselineOffsetForChild(const LayoutBox&) const;

  LayoutUnit GridGap(GridTrackSizingDirection,
                     Optional<LayoutUnit> available_size) const;

  size_t GridItemSpan(const LayoutBox&, GridTrackSizingDirection);

  GridTrackSizingDirection FlowAwareDirectionForChild(
      const LayoutBox&,
      GridTrackSizingDirection) const;

  size_t NumTracks(GridTrackSizingDirection, const Grid&) const;

  static LayoutUnit OverrideContainingBlockContentSizeForChild(
      const LayoutBox& child,
      GridTrackSizingDirection);
  static LayoutUnit SynthesizedBaselineFromContentBox(const LayoutBox&,
                                                      LineDirectionMode);
  static LayoutUnit SynthesizedBaselineFromBorderBox(const LayoutBox&,
                                                     LineDirectionMode);
  static const StyleContentAlignmentData& ContentAlignmentNormalBehavior();

  typedef HashMap<unsigned,
                  std::unique_ptr<BaselineContext>,
                  DefaultHash<unsigned>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      BaselineContextsMap;

  BaselineContextsMap row_axis_alignment_context_;
  BaselineContextsMap col_axis_alignment_context_;

  Grid grid_;
  GridTrackSizingAlgorithm track_sizing_algorithm_;

  Vector<LayoutUnit> row_positions_;
  Vector<LayoutUnit> column_positions_;
  LayoutUnit offset_between_columns_;
  LayoutUnit offset_between_rows_;
  Vector<LayoutBox*> grid_items_overflowing_grid_area_;

  LayoutUnit min_content_height_{-1};
  LayoutUnit max_content_height_{-1};

  Optional<bool> has_definite_logical_height_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutGrid, IsLayoutGrid());

}  // namespace blink

#endif  // LayoutGrid_h
