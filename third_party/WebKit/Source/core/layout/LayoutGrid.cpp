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

#include <algorithm>
#include <memory>
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutState.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/GridPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "core/style/GridArea.h"
#include "platform/LengthFunctions.h"
#include "platform/text/WritingMode.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

struct ContentAlignmentData {
  STACK_ALLOCATED();

 public:
  ContentAlignmentData(){};
  ContentAlignmentData(LayoutUnit position, LayoutUnit distribution)
      : position_offset(position), distribution_offset(distribution) {}

  bool IsValid() { return position_offset >= 0 && distribution_offset >= 0; }

  LayoutUnit position_offset = LayoutUnit(-1);
  LayoutUnit distribution_offset = LayoutUnit(-1);
};

LayoutGrid::LayoutGrid(Element* element)
    : LayoutBlock(element), grid_(this), track_sizing_algorithm_(this, grid_) {
  DCHECK(!ChildrenInline());
  if (!IsAnonymous())
    UseCounter::Count(GetDocument(), WebFeature::kCSSGridLayout);
}

LayoutGrid::~LayoutGrid() {}

LayoutGrid* LayoutGrid::CreateAnonymous(Document* document) {
  LayoutGrid* layout_grid = new LayoutGrid(nullptr);
  layout_grid->SetDocumentForAnonymous(document);
  return layout_grid;
}

void LayoutGrid::AddChild(LayoutObject* new_child, LayoutObject* before_child) {
  LayoutBlock::AddChild(new_child, before_child);

  // Positioned grid items do not take up space or otherwise participate in the
  // layout of the grid, for that reason we don't need to mark the grid as dirty
  // when they are added.
  if (new_child->IsOutOfFlowPositioned())
    return;

  // The grid needs to be recomputed as it might contain auto-placed items that
  // will change their position.
  DirtyGrid();
}

void LayoutGrid::RemoveChild(LayoutObject* child) {
  LayoutBlock::RemoveChild(child);

  // Positioned grid items do not take up space or otherwise participate in the
  // layout of the grid, for that reason we don't need to mark the grid as dirty
  // when they are removed.
  if (child->IsOutOfFlowPositioned())
    return;

  // The grid needs to be recomputed as it might contain auto-placed items that
  // will change their position.
  DirtyGrid();
}

StyleSelfAlignmentData LayoutGrid::SelfAlignmentForChild(
    GridAxis axis,
    const LayoutBox& child,
    const ComputedStyle* style) const {
  return axis == kGridRowAxis ? JustifySelfForChild(child, style)
                              : AlignSelfForChild(child, style);
}

StyleSelfAlignmentData LayoutGrid::DefaultAlignmentForChild(
    GridAxis axis,
    const ComputedStyle& style) const {
  return axis == kGridRowAxis
             ? style.ResolvedJustifyItems(SelfAlignmentNormalBehavior(this))
             : style.ResolvedAlignItems(SelfAlignmentNormalBehavior(this));
}

bool LayoutGrid::SelfAlignmentChangedToStretch(GridAxis axis,
                                               const ComputedStyle& old_style,
                                               const ComputedStyle& new_style,
                                               const LayoutBox& child) const {
  return SelfAlignmentForChild(axis, child, &old_style).GetPosition() !=
             kItemPositionStretch &&
         SelfAlignmentForChild(axis, child, &new_style).GetPosition() ==
             kItemPositionStretch;
}

bool LayoutGrid::SelfAlignmentChangedFromStretch(GridAxis axis,
                                                 const ComputedStyle& old_style,
                                                 const ComputedStyle& new_style,
                                                 const LayoutBox& child) const {
  return SelfAlignmentForChild(axis, child, &old_style).GetPosition() ==
             kItemPositionStretch &&
         SelfAlignmentForChild(axis, child, &new_style).GetPosition() !=
             kItemPositionStretch;
}

bool LayoutGrid::DefaultAlignmentChangedFromStretch(
    GridAxis axis,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) const {
  return DefaultAlignmentForChild(axis, old_style).GetPosition() ==
             kItemPositionStretch &&
         DefaultAlignmentForChild(axis, new_style).GetPosition() !=
             kItemPositionStretch;
}

bool LayoutGrid::DefaultAlignmentChangedToStretch(
    GridAxis axis,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) const {
  return DefaultAlignmentForChild(axis, old_style).GetPosition() !=
             kItemPositionStretch &&
         DefaultAlignmentForChild(axis, new_style).GetPosition() ==
             kItemPositionStretch;
}

void LayoutGrid::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);
  if (!old_style)
    return;

  const ComputedStyle& new_style = StyleRef();
  if (diff.NeedsFullLayout() &&
      (DefaultAlignmentChangedToStretch(kGridRowAxis, *old_style, new_style) ||
       DefaultAlignmentChangedToStretch(kGridColumnAxis, *old_style,
                                        new_style) ||
       DefaultAlignmentChangedFromStretch(kGridRowAxis, *old_style,
                                          new_style) ||
       DefaultAlignmentChangedFromStretch(kGridColumnAxis, *old_style,
                                          new_style))) {
    // Style changes on the grid container implying stretching (to-stretch) or
    // shrinking (from-stretch) require the affected items to be laid out again.
    // These logic only applies to 'stretch' since the rest of the alignment
    // values don't change the size of the box.
    // In any case, the items' overrideSize will be cleared and recomputed (if
    // necessary)  as part of the Grid layout logic, triggered by this style
    // change.
    for (LayoutBox* child = FirstInFlowChildBox(); child;
         child = child->NextInFlowSiblingBox()) {
      if (SelfAlignmentChangedToStretch(kGridRowAxis, *old_style, new_style,
                                        *child) ||
          SelfAlignmentChangedFromStretch(kGridRowAxis, *old_style, new_style,
                                          *child) ||
          SelfAlignmentChangedToStretch(kGridColumnAxis, *old_style, new_style,
                                        *child) ||
          SelfAlignmentChangedFromStretch(kGridColumnAxis, *old_style,
                                          new_style, *child)) {
        child->SetNeedsLayout(LayoutInvalidationReason::kGridChanged);
      }
    }
  }

  // FIXME: The following checks could be narrowed down if we kept track of
  // which type of grid items we have:
  // - explicit grid size changes impact negative explicitely positioned and
  //   auto-placed grid items.
  // - named grid lines only impact grid items with named grid lines.
  // - auto-flow changes only impacts auto-placed children.

  if (ExplicitGridDidResize(*old_style) ||
      NamedGridLinesDefinitionDidChange(*old_style) ||
      old_style->GetGridAutoFlow() != StyleRef().GetGridAutoFlow() ||
      (diff.NeedsLayout() && (StyleRef().GridAutoRepeatColumns().size() ||
                              StyleRef().GridAutoRepeatRows().size())))
    DirtyGrid();
}

bool LayoutGrid::ExplicitGridDidResize(const ComputedStyle& old_style) const {
  return old_style.GridTemplateColumns().size() !=
             StyleRef().GridTemplateColumns().size() ||
         old_style.GridTemplateRows().size() !=
             StyleRef().GridTemplateRows().size() ||
         old_style.NamedGridAreaColumnCount() !=
             StyleRef().NamedGridAreaColumnCount() ||
         old_style.NamedGridAreaRowCount() !=
             StyleRef().NamedGridAreaRowCount() ||
         old_style.GridAutoRepeatColumns().size() !=
             StyleRef().GridAutoRepeatColumns().size() ||
         old_style.GridAutoRepeatRows().size() !=
             StyleRef().GridAutoRepeatRows().size();
}

bool LayoutGrid::NamedGridLinesDefinitionDidChange(
    const ComputedStyle& old_style) const {
  return old_style.NamedGridRowLines() != StyleRef().NamedGridRowLines() ||
         old_style.NamedGridColumnLines() != StyleRef().NamedGridColumnLines();
}

// This method optimizes the gutters computation by skiping the available size
// call if gaps are fixed size (it's only needed for percentages).
Optional<LayoutUnit> LayoutGrid::AvailableSpaceForGutters(
    GridTrackSizingDirection direction) const {
  bool is_row_axis = direction == kForColumns;
  const Length& gap =
      is_row_axis ? StyleRef().GridColumnGap() : StyleRef().GridRowGap();
  if (!gap.IsPercent())
    return WTF::nullopt;

  return is_row_axis ? AvailableLogicalWidth()
                     : AvailableLogicalHeightForPercentageComputation();
}

LayoutUnit LayoutGrid::ComputeTrackBasedLogicalHeight() const {
  LayoutUnit logical_height;

  const Vector<GridTrack>& all_rows = track_sizing_algorithm_.Tracks(kForRows);
  for (const auto& row : all_rows)
    logical_height += row.BaseSize();

  logical_height += GuttersSize(grid_, kForRows, 0, all_rows.size(),
                                AvailableSpaceForGutters(kForRows));

  return logical_height;
}

void LayoutGrid::ComputeTrackSizesForDefiniteSize(
    GridTrackSizingDirection direction,
    LayoutUnit available_space) {
  LayoutUnit free_space =
      available_space - GuttersSize(grid_, direction, 0,
                                    grid_.NumTracks(direction),
                                    available_space);
  track_sizing_algorithm_.Setup(direction, NumTracks(direction, grid_),
                                available_space, free_space);
  track_sizing_algorithm_.Run();

#if DCHECK_IS_ON()
  DCHECK(track_sizing_algorithm_.TracksAreWiderThanMinTrackBreadth());
#endif
}

void LayoutGrid::RepeatTracksSizingIfNeeded(
    LayoutUnit available_space_for_columns,
    LayoutUnit available_space_for_rows) {
  // Baseline alignment may change item's intrinsic size, hence changing its
  // min-content contribution.
  // https://drafts.csswg.org/css-align-3/#baseline-align-content
  // https://drafts.csswg.org/css-align-3/#baseline-align-self
  bool baseline_affect_intrinsic_width = BaselineMayAffectIntrinsicWidth();
  bool baseline_affect_intrinsic_height = BaselineMayAffectIntrinsicHeight();

  // In orthogonal flow cases column track's size is determined by using the
  // computed row track's size, which it was estimated during the first cycle of
  // the sizing algorithm.
  bool has_any_orthogonal =
      track_sizing_algorithm_.GetGrid().HasAnyOrthogonalGridItem();

  // TODO (lajava): these are just some of the cases which may require
  // a new cycle of the sizing algorithm; there may be more. In addition, not
  // all the cases with orthogonal flows require this extra cycle; we need a
  // more specific condition to detect whether child's min-content contribution
  // has changed or not.
  if (!baseline_affect_intrinsic_width && !baseline_affect_intrinsic_height &&
      !has_any_orthogonal)
    return;

  // TODO (lajava): Whenever the min-content contribution of a grid item changes
  // we may need to update the grid container's intrinsic width. The new
  // intrinsic width may also affect the extra Track Sizing algorithm cycles we
  // are about to execute.
  // https://crbug.com/704713
  // https://github.com/w3c/csswg-drafts/issues/1039

  // Hence we need to repeat computeUsedBreadthOfGridTracks for both, columns
  // and rows, to determine the final values.
  ComputeTrackSizesForDefiniteSize(kForColumns, available_space_for_columns);
  ComputeTrackSizesForDefiniteSize(kForRows, available_space_for_rows);

  if (baseline_affect_intrinsic_height) {
    SetLogicalHeight(ComputeTrackBasedLogicalHeight() +
                     BorderAndPaddingLogicalHeight() +
                     ScrollbarLogicalHeight());
  }
}

void LayoutGrid::UpdateBlockLayout(bool relayout_children) {
  DCHECK(NeedsLayout());

  // We cannot perform a simplifiedLayout() on a dirty grid that
  // has positioned items to be laid out.
  if (!relayout_children &&
      (!grid_.NeedsItemsPlacement() || !PosChildNeedsLayout()) &&
      SimplifiedLayout())
    return;

  row_axis_alignment_context_.clear();
  col_axis_alignment_context_.clear();

  SubtreeLayoutScope layout_scope(*this);

  {
    // LayoutState needs this deliberate scope to pop before updating scroll
    // information (which may trigger relayout).
    LayoutState state(*this);

    LayoutSize previous_size = Size();
    has_definite_logical_height_ = HasDefiniteLogicalHeight();

    // Grid's layout logic controls the grid item's override size, hence
    // we need to clear any override size set previously, so it doesn't
    // interfere in current layout execution.
    for (auto* child = FirstInFlowChildBox(); child;
         child = child->NextInFlowSiblingBox())
      child->ClearOverrideSize();

    UpdateLogicalWidth();

    TextAutosizer::LayoutScope text_autosizer_layout_scope(this, &layout_scope);

    LayoutUnit available_space_for_columns = AvailableLogicalWidth();
    PlaceItemsOnGrid(grid_, available_space_for_columns);

    // 1- First, the track sizing algorithm is used to resolve the sizes of the
    // grid columns.
    // At this point the logical width is always definite as the above call to
    // updateLogicalWidth() properly resolves intrinsic sizes. We cannot do the
    // same for heights though because many code paths inside
    // updateLogicalHeight() require a previous call to setLogicalHeight() to
    // resolve heights properly (like for positioned items for example).
    ComputeTrackSizesForDefiniteSize(kForColumns, available_space_for_columns);

    // We take the chance to store the intrinsic sizes as they are just an
    // intermediate result of the track sizing algorithm. Apart from eventually
    // saving an algorithm execution (if {min|max}PreferredLogicalWidth() are
    // called later), it fixes the use case of computing the preferred logical
    // widths *after* the layout process. Although not very common, this happens
    // in the Mac (content::RenderViewImpl::didUpdateLayout()) or under some
    // circumstances when grids are also flex items (crbug.com/708159).
    if (PreferredLogicalWidthsDirty()) {
      LayoutUnit scrollbar_width = LayoutUnit(ScrollbarLogicalWidth());
      min_preferred_logical_width_ =
          track_sizing_algorithm_.MinContentSize() + scrollbar_width;
      max_preferred_logical_width_ =
          track_sizing_algorithm_.MaxContentSize() + scrollbar_width;
      ClearPreferredLogicalWidthsDirty();
    }

    // 2- Next, the track sizing algorithm resolves the sizes of the grid rows,
    // using the grid column sizes calculated in the previous step.
    if (CachedHasDefiniteLogicalHeight()) {
      ComputeTrackSizesForDefiniteSize(
          kForRows, AvailableLogicalHeight(kExcludeMarginBorderPadding));
    } else {
      ComputeTrackSizesForIndefiniteSize(track_sizing_algorithm_, kForRows,
                                         grid_, min_content_height_,
                                         max_content_height_);
    }
    LayoutUnit track_based_logical_height = ComputeTrackBasedLogicalHeight() +
                                            BorderAndPaddingLogicalHeight() +
                                            ScrollbarLogicalHeight();
    SetLogicalHeight(track_based_logical_height);

    LayoutUnit old_client_after_edge = ClientLogicalBottom();
    UpdateLogicalHeight();

    // Once grid's indefinite height is resolved, we can compute the
    // available free space for Content Alignment.
    if (!CachedHasDefiniteLogicalHeight()) {
      track_sizing_algorithm_.SetFreeSpace(
          kForRows, LogicalHeight() - track_based_logical_height);
    }

    // TODO (lajava): We need to compute baselines after step 2 so
    // items with a relative size (percentages) can resolve it before
    // determining its baseline. However, we only set item's grid area
    // (via override sizes) as part of the content-sized tracks sizing
    // logic. Hence, items located at fixed or flexible tracks can't
    // resolve correctly their size at this stage, which may lead to
    // an incorrect computation of their shared context's baseline.
    ComputeBaselineAlignmentContext();

    // 3- If the min-content contribution of any grid items have changed based
    // on the row sizes calculated in step 2, steps 1 and 2 are repeated with
    // the new min-content contribution (once only).
    RepeatTracksSizingIfNeeded(available_space_for_columns,
                               ContentLogicalHeight());

    // Grid container should have the minimum height of a line if it's editable.
    // That doesn't affect track sizing though.
    if (HasLineIfEmpty())
      SetLogicalHeight(
          std::max(LogicalHeight(), MinimumLogicalHeightForEmptyLine()));

    ApplyStretchAlignmentToTracksIfNeeded(kForColumns);
    ApplyStretchAlignmentToTracksIfNeeded(kForRows);

    LayoutGridItems();
    track_sizing_algorithm_.Reset();

    if (Size() != previous_size)
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());

    ComputeOverflow(old_client_after_edge);
  }

  UpdateAfterLayout();

  ClearNeedsLayout();
}

LayoutUnit LayoutGrid::GridGap(GridTrackSizingDirection direction,
                               Optional<LayoutUnit> available_size) const {
  const Length& gap = direction == kForColumns ? StyleRef().GridColumnGap()
                                               : StyleRef().GridRowGap();
  return ValueForLength(gap, available_size.value_or(LayoutUnit()));
}

LayoutUnit LayoutGrid::GridGap(GridTrackSizingDirection direction) const {
  LayoutUnit available_size;
  bool is_row_axis = direction == kForColumns;
  const Length& gap =
      is_row_axis ? StyleRef().GridColumnGap() : StyleRef().GridRowGap();
  if (gap.IsPercent())
    available_size = is_row_axis
                         ? AvailableLogicalWidth()
                         : AvailableLogicalHeightForPercentageComputation();

  // TODO(rego): Maybe we could cache the computed percentage as a performance
  // improvement.
  return ValueForLength(gap, available_size);
}

LayoutUnit LayoutGrid::GuttersSize(const Grid& grid,
                                   GridTrackSizingDirection direction,
                                   size_t start_line,
                                   size_t span,
                                   Optional<LayoutUnit> available_size) const {
  if (span <= 1)
    return LayoutUnit();

  LayoutUnit gap = GridGap(direction, available_size);

  // Fast path, no collapsing tracks.
  if (!grid.HasAutoRepeatEmptyTracks(direction))
    return gap * (span - 1);

  // If there are collapsing tracks we need to be sure that gutters are properly
  // collapsed. Apart from that, if we have a collapsed track in the edges of
  // the span we're considering, we need to move forward (or backwards) in order
  // to know whether the collapsed tracks reach the end of the grid (so the gap
  // becomes 0) or there is a non empty track before that.

  LayoutUnit gap_accumulator;
  size_t end_line = start_line + span;

  for (size_t line = start_line; line < end_line - 1; ++line) {
    if (!grid.IsEmptyAutoRepeatTrack(direction, line))
      gap_accumulator += gap;
  }

  // The above loop adds one extra gap for trailing collapsed tracks.
  if (gap_accumulator && grid.IsEmptyAutoRepeatTrack(direction, end_line - 1)) {
    DCHECK_GE(gap_accumulator, gap);
    gap_accumulator -= gap;
  }

  // If the startLine is the start line of a collapsed track we need to go
  // backwards till we reach a non collapsed track. If we find a non collapsed
  // track we need to add that gap.
  if (start_line && grid.IsEmptyAutoRepeatTrack(direction, start_line)) {
    size_t non_empty_tracks_before_start_line = start_line;
    auto begin = grid.AutoRepeatEmptyTracks(direction)->begin();
    for (auto it = begin; *it != start_line; ++it) {
      DCHECK(non_empty_tracks_before_start_line);
      --non_empty_tracks_before_start_line;
    }
    if (non_empty_tracks_before_start_line)
      gap_accumulator += gap;
  }

  // If the endLine is the end line of a collapsed track we need to go forward
  // till we reach a non collapsed track. If we find a non collapsed track we
  // need to add that gap.
  if (grid.IsEmptyAutoRepeatTrack(direction, end_line - 1)) {
    size_t non_empty_tracks_after_end_line =
        grid.NumTracks(direction) - end_line;
    auto current_empty_track =
        grid.AutoRepeatEmptyTracks(direction)->find(end_line - 1);
    auto end_empty_track = grid.AutoRepeatEmptyTracks(direction)->end();
    // HashSet iterators do not implement operator- so we have to manually
    // iterate to know the number of remaining empty tracks.
    for (auto it = ++current_empty_track; it != end_empty_track; ++it) {
      DCHECK(non_empty_tracks_after_end_line);
      --non_empty_tracks_after_end_line;
    }
    if (non_empty_tracks_after_end_line)
      gap_accumulator += gap;
  }

  return gap_accumulator;
}

void LayoutGrid::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  Grid grid(this);
  PlaceItemsOnGrid(grid, WTF::nullopt);

  GridTrackSizingAlgorithm algorithm(this, grid);
  ComputeTrackSizesForIndefiniteSize(algorithm, kForColumns, grid,
                                     min_logical_width, max_logical_width);

  LayoutUnit scrollbar_width = LayoutUnit(ScrollbarLogicalWidth());
  min_logical_width += scrollbar_width;
  max_logical_width += scrollbar_width;
}

void LayoutGrid::ComputeTrackSizesForIndefiniteSize(
    GridTrackSizingAlgorithm& algo,
    GridTrackSizingDirection direction,
    Grid& grid,
    LayoutUnit& min_intrinsic_size,
    LayoutUnit& max_intrinsic_size) const {
  algo.Setup(direction, NumTracks(direction, grid), WTF::nullopt, WTF::nullopt);
  algo.Run();

  min_intrinsic_size = algo.MinContentSize();
  max_intrinsic_size = algo.MaxContentSize();

  size_t number_of_tracks = algo.Tracks(direction).size();
  LayoutUnit total_gutters_size =
      GuttersSize(grid, direction, 0, number_of_tracks, WTF::nullopt);
  min_intrinsic_size += total_gutters_size;
  max_intrinsic_size += total_gutters_size;

#if DCHECK_IS_ON()
  DCHECK(algo.TracksAreWiderThanMinTrackBreadth());
#endif
}

LayoutUnit LayoutGrid::ComputeIntrinsicLogicalContentHeightUsing(
    const Length& logical_height_length,
    LayoutUnit intrinsic_content_height,
    LayoutUnit border_and_padding) const {
  if (logical_height_length.IsMinContent())
    return min_content_height_;

  if (logical_height_length.IsMaxContent())
    return max_content_height_;

  if (logical_height_length.IsFitContent()) {
    if (min_content_height_ == -1 || max_content_height_ == -1)
      return LayoutUnit(-1);
    LayoutUnit fill_available_extent =
        ContainingBlock()->AvailableLogicalHeight(kExcludeMarginBorderPadding);
    return std::min<LayoutUnit>(
        max_content_height_,
        std::max(min_content_height_, fill_available_extent));
  }

  if (logical_height_length.IsFillAvailable())
    return ContainingBlock()->AvailableLogicalHeight(
               kExcludeMarginBorderPadding) -
           border_and_padding;
  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::OverrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == kForColumns
             ? child.OverrideContainingBlockContentLogicalWidth()
             : child.OverrideContainingBlockContentLogicalHeight();
}

bool LayoutGrid::IsOrthogonalChild(const LayoutBox& child) const {
  return child.IsHorizontalWritingMode() != IsHorizontalWritingMode();
}

// Unfortunately there are still many layout methods that return -1 for
// non-resolvable sizes. We prefer to represent them with WTF::nullopt.
static Optional<LayoutUnit> ConvertLayoutUnitToOptional(LayoutUnit size) {
  if (size == -1)
    return WTF::nullopt;
  return size;
}

size_t LayoutGrid::ComputeAutoRepeatTracksCount(
    GridTrackSizingDirection direction,
    Optional<LayoutUnit> available_size) const {
  DCHECK(!available_size || available_size.value() != -1);
  bool is_row_axis = direction == kForColumns;
  const auto& auto_repeat_tracks = is_row_axis
                                       ? StyleRef().GridAutoRepeatColumns()
                                       : StyleRef().GridAutoRepeatRows();
  size_t auto_repeat_track_list_length = auto_repeat_tracks.size();

  if (!auto_repeat_track_list_length)
    return 0;

  if (!is_row_axis) {
    if (!available_size) {
      const Length& max_length = StyleRef().LogicalMaxHeight();
      if (!max_length.IsMaxSizeNone()) {
        available_size = ConvertLayoutUnitToOptional(
            ConstrainContentBoxLogicalHeightByMinMax(
                AvailableLogicalHeightUsing(max_length,
                                            kExcludeMarginBorderPadding),
                LayoutUnit(-1)));
      }
    }
  }

  bool needs_to_fulfill_minimum_size = false;
  if (!available_size) {
    const Length& min_size = is_row_axis ? StyleRef().LogicalMinWidth()
                                         : StyleRef().LogicalMinHeight();
    if (!min_size.IsSpecified())
      return auto_repeat_track_list_length;

    LayoutUnit containing_block_available_size =
        is_row_axis ? ContainingBlockLogicalWidthForContent()
                    : ContainingBlockLogicalHeightForContent(
                          kExcludeMarginBorderPadding);
    available_size = ValueForLength(min_size, containing_block_available_size);
    needs_to_fulfill_minimum_size = true;
  }

  LayoutUnit auto_repeat_tracks_size;
  for (auto auto_track_size : auto_repeat_tracks) {
    DCHECK(auto_track_size.MinTrackBreadth().IsLength());
    DCHECK(!auto_track_size.MinTrackBreadth().IsFlex());
    bool has_definite_max_track_sizing_function =
        auto_track_size.MaxTrackBreadth().IsLength() &&
        !auto_track_size.MaxTrackBreadth().IsContentSized();
    auto track_length = has_definite_max_track_sizing_function
                            ? auto_track_size.MaxTrackBreadth().length()
                            : auto_track_size.MinTrackBreadth().length();
    auto_repeat_tracks_size +=
        ValueForLength(track_length, available_size.value());
  }
  // For the purpose of finding the number of auto-repeated tracks, the UA must
  // floor the track size to a UA-specified value to avoid division by zero. It
  // is suggested that this floor be 1px.
  auto_repeat_tracks_size =
      std::max<LayoutUnit>(LayoutUnit(1), auto_repeat_tracks_size);

  // There will be always at least 1 auto-repeat track, so take it already into
  // account when computing the total track size.
  LayoutUnit tracks_size = auto_repeat_tracks_size;
  const Vector<GridTrackSize>& track_sizes =
      is_row_axis ? StyleRef().GridTemplateColumns()
                  : StyleRef().GridTemplateRows();

  for (const auto& track : track_sizes) {
    bool has_definite_max_track_breadth =
        track.MaxTrackBreadth().IsLength() &&
        !track.MaxTrackBreadth().IsContentSized();
    DCHECK(has_definite_max_track_breadth ||
           (track.MinTrackBreadth().IsLength() &&
            !track.MinTrackBreadth().IsContentSized()));
    tracks_size += ValueForLength(has_definite_max_track_breadth
                                      ? track.MaxTrackBreadth().length()
                                      : track.MinTrackBreadth().length(),
                                  available_size.value());
  }

  // Add gutters as if there where only 1 auto repeat track. Gaps between auto
  // repeat tracks will be added later when computing the repetitions.
  LayoutUnit gap_size = GridGap(direction, available_size);
  tracks_size += gap_size * track_sizes.size();

  LayoutUnit free_space = available_size.value() - tracks_size;
  if (free_space <= 0)
    return auto_repeat_track_list_length;

  size_t repetitions =
      1 + (free_space / (auto_repeat_tracks_size + gap_size)).ToInt();

  // Provided the grid container does not have a definite size or max-size in
  // the relevant axis, if the min size is definite then the number of
  // repetitions is the largest possible positive integer that fulfills that
  // minimum requirement.
  if (needs_to_fulfill_minimum_size)
    ++repetitions;

  return repetitions * auto_repeat_track_list_length;
}

std::unique_ptr<OrderedTrackIndexSet>
LayoutGrid::ComputeEmptyTracksForAutoRepeat(
    Grid& grid,
    GridTrackSizingDirection direction) const {
  bool is_row_axis = direction == kForColumns;
  if ((is_row_axis && StyleRef().GridAutoRepeatColumnsType() != kAutoFit) ||
      (!is_row_axis && StyleRef().GridAutoRepeatRowsType() != kAutoFit))
    return nullptr;

  std::unique_ptr<OrderedTrackIndexSet> empty_track_indexes;
  size_t insertion_point =
      is_row_axis ? StyleRef().GridAutoRepeatColumnsInsertionPoint()
                  : StyleRef().GridAutoRepeatRowsInsertionPoint();
  size_t first_auto_repeat_track =
      insertion_point + std::abs(grid.SmallestTrackStart(direction));
  size_t last_auto_repeat_track =
      first_auto_repeat_track + grid.AutoRepeatTracks(direction);

  if (!grid.HasGridItems()) {
    empty_track_indexes = WTF::WrapUnique(new OrderedTrackIndexSet);
    for (size_t track_index = first_auto_repeat_track;
         track_index < last_auto_repeat_track; ++track_index)
      empty_track_indexes->insert(track_index);
  } else {
    for (size_t track_index = first_auto_repeat_track;
         track_index < last_auto_repeat_track; ++track_index) {
      GridIterator iterator(grid, direction, track_index);
      if (!iterator.NextGridItem()) {
        if (!empty_track_indexes)
          empty_track_indexes = WTF::WrapUnique(new OrderedTrackIndexSet);
        empty_track_indexes->insert(track_index);
      }
    }
  }
  return empty_track_indexes;
}

size_t LayoutGrid::ClampAutoRepeatTracks(GridTrackSizingDirection direction,
                                         size_t auto_repeat_tracks) const {
  if (!auto_repeat_tracks)
    return 0;

  size_t insertion_point =
      direction == kForColumns
          ? StyleRef().GridAutoRepeatColumnsInsertionPoint()
          : StyleRef().GridAutoRepeatRowsInsertionPoint();

  if (insertion_point == 0)
    return std::min<size_t>(auto_repeat_tracks, kGridMaxTracks);

  if (insertion_point >= kGridMaxTracks)
    return 0;

  return std::min(auto_repeat_tracks,
                  static_cast<size_t>(kGridMaxTracks) - insertion_point);
}

// TODO(svillar): we shouldn't have to pass the available logical width as
// argument. The problem is that availableLogicalWidth() does always return a
// value even if we cannot resolve it like when computing the intrinsic size
// (preferred widths). That's why we pass the responsibility to the caller who
// does know whether the available logical width is indefinite or not.
void LayoutGrid::PlaceItemsOnGrid(
    Grid& grid,
    Optional<LayoutUnit> available_logical_width) const {
  size_t auto_repeat_rows = ComputeAutoRepeatTracksCount(
      kForRows, ConvertLayoutUnitToOptional(
                    AvailableLogicalHeightForPercentageComputation()));
  size_t auto_repeat_columns =
      ComputeAutoRepeatTracksCount(kForColumns, available_logical_width);

  auto_repeat_rows = ClampAutoRepeatTracks(kForRows, auto_repeat_rows);
  auto_repeat_columns = ClampAutoRepeatTracks(kForColumns, auto_repeat_columns);

  if (auto_repeat_rows != grid.AutoRepeatTracks(kForRows) ||
      auto_repeat_columns != grid.AutoRepeatTracks(kForColumns)) {
    grid.SetNeedsItemsPlacement(true);
    grid.SetAutoRepeatTracks(auto_repeat_rows, auto_repeat_columns);
  }

  if (!grid.NeedsItemsPlacement())
    return;

  DCHECK(!grid.HasGridItems());
  PopulateExplicitGridAndOrderIterator(grid);

  Vector<LayoutBox*> auto_major_axis_auto_grid_items;
  Vector<LayoutBox*> specified_major_axis_auto_grid_items;
#if DCHECK_IS_ON()
  DCHECK(!grid.HasAnyGridItemPaintOrder());
#endif
  DCHECK(!grid.HasAnyOrthogonalGridItem());
  bool has_any_orthogonal_grid_item = false;
  size_t child_index = 0;
  for (LayoutBox* child = grid.GetOrderIterator().First(); child;
       child = grid.GetOrderIterator().Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    has_any_orthogonal_grid_item =
        has_any_orthogonal_grid_item || IsOrthogonalChild(*child);
    grid.SetGridItemPaintOrder(*child, child_index++);

    GridArea area = grid.GridItemArea(*child);
    if (!area.rows.IsIndefinite())
      area.rows.Translate(abs(grid.SmallestTrackStart(kForRows)));
    if (!area.columns.IsIndefinite())
      area.columns.Translate(abs(grid.SmallestTrackStart(kForColumns)));

    if (area.rows.IsIndefinite() || area.columns.IsIndefinite()) {
      grid.SetGridItemArea(*child, area);
      GridSpan major_axis_positions =
          (AutoPlacementMajorAxisDirection() == kForColumns) ? area.columns
                                                             : area.rows;
      if (major_axis_positions.IsIndefinite())
        auto_major_axis_auto_grid_items.push_back(child);
      else
        specified_major_axis_auto_grid_items.push_back(child);
      continue;
    }
    grid.insert(*child, area);
  }
  grid.SetHasAnyOrthogonalGridItem(has_any_orthogonal_grid_item);

#if DCHECK_IS_ON()
  if (grid.HasGridItems()) {
    DCHECK_GE(grid.NumTracks(kForRows),
              GridPositionsResolver::ExplicitGridRowCount(
                  *Style(), grid.AutoRepeatTracks(kForRows)));
    DCHECK_GE(grid.NumTracks(kForColumns),
              GridPositionsResolver::ExplicitGridColumnCount(
                  *Style(), grid.AutoRepeatTracks(kForColumns)));
  }
#endif

  PlaceSpecifiedMajorAxisItemsOnGrid(grid,
                                     specified_major_axis_auto_grid_items);
  PlaceAutoMajorAxisItemsOnGrid(grid, auto_major_axis_auto_grid_items);

  // Compute collapsable tracks for auto-fit.
  grid.SetAutoRepeatEmptyColumns(
      ComputeEmptyTracksForAutoRepeat(grid, kForColumns));
  grid.SetAutoRepeatEmptyRows(ComputeEmptyTracksForAutoRepeat(grid, kForRows));

  grid.SetNeedsItemsPlacement(false);

#if DCHECK_IS_ON()
  for (LayoutBox* child = grid.GetOrderIterator().First(); child;
       child = grid.GetOrderIterator().Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    GridArea area = grid.GridItemArea(*child);
    DCHECK(area.rows.IsTranslatedDefinite());
    DCHECK(area.columns.IsTranslatedDefinite());
  }
#endif
}

void LayoutGrid::PopulateExplicitGridAndOrderIterator(Grid& grid) const {
  OrderIteratorPopulator populator(grid.GetOrderIterator());
  int smallest_row_start = 0;
  int smallest_column_start = 0;

  size_t auto_repeat_rows = grid.AutoRepeatTracks(kForRows);
  size_t auto_repeat_columns = grid.AutoRepeatTracks(kForColumns);
  size_t maximum_row_index =
      GridPositionsResolver::ExplicitGridRowCount(*Style(), auto_repeat_rows);
  size_t maximum_column_index = GridPositionsResolver::ExplicitGridColumnCount(
      *Style(), auto_repeat_columns);

  for (LayoutBox* child = FirstInFlowChildBox(); child;
       child = child->NextInFlowSiblingBox()) {
    populator.CollectChild(child);

    // This function bypasses the cache (gridItemArea()) as it is used to
    // build it.
    GridSpan row_positions =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            *Style(), *child, kForRows, auto_repeat_rows);
    GridSpan column_positions =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            *Style(), *child, kForColumns, auto_repeat_columns);
    grid.SetGridItemArea(*child, GridArea(row_positions, column_positions));

    // |positions| is 0 if we need to run the auto-placement algorithm.
    if (!row_positions.IsIndefinite()) {
      smallest_row_start =
          std::min(smallest_row_start, row_positions.UntranslatedStartLine());
      maximum_row_index =
          std::max<int>(maximum_row_index, row_positions.UntranslatedEndLine());
    } else {
      // Grow the grid for items with a definite row span, getting the largest
      // such span.
      size_t span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
          *Style(), *child, kForRows);
      maximum_row_index = std::max(maximum_row_index, span_size);
    }

    if (!column_positions.IsIndefinite()) {
      smallest_column_start = std::min(
          smallest_column_start, column_positions.UntranslatedStartLine());
      maximum_column_index = std::max<int>(
          maximum_column_index, column_positions.UntranslatedEndLine());
    } else {
      // Grow the grid for items with a definite column span, getting the
      // largest such span.
      size_t span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
          *Style(), *child, kForColumns);
      maximum_column_index = std::max(maximum_column_index, span_size);
    }
  }

  grid.SetSmallestTracksStart(smallest_row_start, smallest_column_start);
  grid.EnsureGridSize(maximum_row_index + abs(smallest_row_start),
                      maximum_column_index + abs(smallest_column_start));
}

std::unique_ptr<GridArea>
LayoutGrid::CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
    const Grid& grid,
    const LayoutBox& grid_item,
    GridTrackSizingDirection specified_direction,
    const GridSpan& specified_positions) const {
  GridTrackSizingDirection cross_direction =
      specified_direction == kForColumns ? kForRows : kForColumns;
  const size_t end_of_cross_direction = grid.NumTracks(cross_direction);
  size_t cross_direction_span_size =
      GridPositionsResolver::SpanSizeForAutoPlacedItem(*Style(), grid_item,
                                                       cross_direction);
  GridSpan cross_direction_positions = GridSpan::TranslatedDefiniteGridSpan(
      end_of_cross_direction,
      end_of_cross_direction + cross_direction_span_size);
  return WTF::WrapUnique(new GridArea(
      specified_direction == kForColumns ? cross_direction_positions
                                         : specified_positions,
      specified_direction == kForColumns ? specified_positions
                                         : cross_direction_positions));
}

void LayoutGrid::PlaceSpecifiedMajorAxisItemsOnGrid(
    Grid& grid,
    const Vector<LayoutBox*>& auto_grid_items) const {
  bool is_for_columns = AutoPlacementMajorAxisDirection() == kForColumns;
  bool is_grid_auto_flow_dense = Style()->IsGridAutoFlowAlgorithmDense();

  // Mapping between the major axis tracks (rows or columns) and the last
  // auto-placed item's position inserted on that track. This is needed to
  // implement "sparse" packing for items locked to a given track.
  // See http://dev.w3.org/csswg/css-grid/#auto-placement-algo
  HashMap<unsigned, unsigned, DefaultHash<unsigned>::Hash,
          WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      minor_axis_cursors;

  for (const auto& auto_grid_item : auto_grid_items) {
    GridSpan major_axis_positions =
        grid.GridItemSpan(*auto_grid_item, AutoPlacementMajorAxisDirection());
    DCHECK(major_axis_positions.IsTranslatedDefinite());
    DCHECK(
        !grid.GridItemSpan(*auto_grid_item, AutoPlacementMinorAxisDirection())
             .IsTranslatedDefinite());
    size_t minor_axis_span_size =
        GridPositionsResolver::SpanSizeForAutoPlacedItem(
            *Style(), *auto_grid_item, AutoPlacementMinorAxisDirection());
    unsigned major_axis_initial_position = major_axis_positions.StartLine();

    GridIterator iterator(
        grid, AutoPlacementMajorAxisDirection(),
        major_axis_positions.StartLine(),
        is_grid_auto_flow_dense
            ? 0
            : minor_axis_cursors.at(major_axis_initial_position));
    std::unique_ptr<GridArea> empty_grid_area = iterator.NextEmptyGridArea(
        major_axis_positions.IntegerSpan(), minor_axis_span_size);
    if (!empty_grid_area) {
      empty_grid_area = CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, *auto_grid_item, AutoPlacementMajorAxisDirection(),
          major_axis_positions);
    }

    grid.insert(*auto_grid_item, *empty_grid_area);

    if (!is_grid_auto_flow_dense)
      minor_axis_cursors.Set(major_axis_initial_position,
                             is_for_columns
                                 ? empty_grid_area->rows.StartLine()
                                 : empty_grid_area->columns.StartLine());
  }
}

void LayoutGrid::PlaceAutoMajorAxisItemsOnGrid(
    Grid& grid,
    const Vector<LayoutBox*>& auto_grid_items) const {
  std::pair<size_t, size_t> auto_placement_cursor = std::make_pair(0, 0);
  bool is_grid_auto_flow_dense = Style()->IsGridAutoFlowAlgorithmDense();

  for (const auto& auto_grid_item : auto_grid_items) {
    PlaceAutoMajorAxisItemOnGrid(grid, *auto_grid_item, auto_placement_cursor);

    // If grid-auto-flow is dense, reset auto-placement cursor.
    if (is_grid_auto_flow_dense) {
      auto_placement_cursor.first = 0;
      auto_placement_cursor.second = 0;
    }
  }
}

void LayoutGrid::PlaceAutoMajorAxisItemOnGrid(
    Grid& grid,
    LayoutBox& grid_item,
    std::pair<size_t, size_t>& auto_placement_cursor) const {
  GridSpan minor_axis_positions =
      grid.GridItemSpan(grid_item, AutoPlacementMinorAxisDirection());
  DCHECK(!grid.GridItemSpan(grid_item, AutoPlacementMajorAxisDirection())
              .IsTranslatedDefinite());
  size_t major_axis_span_size =
      GridPositionsResolver::SpanSizeForAutoPlacedItem(
          *Style(), grid_item, AutoPlacementMajorAxisDirection());

  const size_t end_of_major_axis =
      grid.NumTracks(AutoPlacementMajorAxisDirection());
  size_t major_axis_auto_placement_cursor =
      AutoPlacementMajorAxisDirection() == kForColumns
          ? auto_placement_cursor.second
          : auto_placement_cursor.first;
  size_t minor_axis_auto_placement_cursor =
      AutoPlacementMajorAxisDirection() == kForColumns
          ? auto_placement_cursor.first
          : auto_placement_cursor.second;

  std::unique_ptr<GridArea> empty_grid_area;
  if (minor_axis_positions.IsTranslatedDefinite()) {
    // Move to the next track in major axis if initial position in minor axis is
    // before auto-placement cursor.
    if (minor_axis_positions.StartLine() < minor_axis_auto_placement_cursor)
      major_axis_auto_placement_cursor++;

    if (major_axis_auto_placement_cursor < end_of_major_axis) {
      GridIterator iterator(grid, AutoPlacementMinorAxisDirection(),
                            minor_axis_positions.StartLine(),
                            major_axis_auto_placement_cursor);
      empty_grid_area = iterator.NextEmptyGridArea(
          minor_axis_positions.IntegerSpan(), major_axis_span_size);
    }

    if (!empty_grid_area) {
      empty_grid_area = CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, grid_item, AutoPlacementMinorAxisDirection(),
          minor_axis_positions);
    }
  } else {
    size_t minor_axis_span_size =
        GridPositionsResolver::SpanSizeForAutoPlacedItem(
            *Style(), grid_item, AutoPlacementMinorAxisDirection());

    for (size_t major_axis_index = major_axis_auto_placement_cursor;
         major_axis_index < end_of_major_axis; ++major_axis_index) {
      GridIterator iterator(grid, AutoPlacementMajorAxisDirection(),
                            major_axis_index, minor_axis_auto_placement_cursor);
      empty_grid_area = iterator.NextEmptyGridArea(major_axis_span_size,
                                                   minor_axis_span_size);

      if (empty_grid_area) {
        // Check that it fits in the minor axis direction, as we shouldn't grow
        // in that direction here (it was already managed in
        // populateExplicitGridAndOrderIterator()).
        size_t minor_axis_final_position_index =
            AutoPlacementMinorAxisDirection() == kForColumns
                ? empty_grid_area->columns.EndLine()
                : empty_grid_area->rows.EndLine();
        const size_t end_of_minor_axis =
            grid.NumTracks(AutoPlacementMinorAxisDirection());
        if (minor_axis_final_position_index <= end_of_minor_axis)
          break;

        // Discard empty grid area as it does not fit in the minor axis
        // direction. We don't need to create a new empty grid area yet as we
        // might find a valid one in the next iteration.
        empty_grid_area = nullptr;
      }

      // As we're moving to the next track in the major axis we should reset the
      // auto-placement cursor in the minor axis.
      minor_axis_auto_placement_cursor = 0;
    }

    if (!empty_grid_area)
      empty_grid_area = CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, grid_item, AutoPlacementMinorAxisDirection(),
          GridSpan::TranslatedDefiniteGridSpan(0, minor_axis_span_size));
  }

  grid.insert(grid_item, *empty_grid_area);
  // Move auto-placement cursor to the new position.
  auto_placement_cursor.first = empty_grid_area->rows.StartLine();
  auto_placement_cursor.second = empty_grid_area->columns.StartLine();
}

GridTrackSizingDirection LayoutGrid::AutoPlacementMajorAxisDirection() const {
  return Style()->IsGridAutoFlowDirectionColumn() ? kForColumns : kForRows;
}

GridTrackSizingDirection LayoutGrid::AutoPlacementMinorAxisDirection() const {
  return Style()->IsGridAutoFlowDirectionColumn() ? kForRows : kForColumns;
}

void LayoutGrid::DirtyGrid() {
  if (grid_.NeedsItemsPlacement())
    return;

  grid_.SetNeedsItemsPlacement(true);
  grid_items_overflowing_grid_area_.resize(0);
}

Vector<LayoutUnit> LayoutGrid::TrackSizesForComputedStyle(
    GridTrackSizingDirection direction) const {
  bool is_row_axis = direction == kForColumns;
  auto& positions = is_row_axis ? column_positions_ : row_positions_;
  size_t num_positions = positions.size();
  LayoutUnit offset_between_tracks =
      is_row_axis ? offset_between_columns_ : offset_between_rows_;

  Vector<LayoutUnit> tracks;
  if (num_positions < 2)
    return tracks;

  DCHECK(!grid_.NeedsItemsPlacement());
  bool has_collapsed_tracks = grid_.HasAutoRepeatEmptyTracks(direction);
  LayoutUnit gap = !has_collapsed_tracks ? GridGap(direction) : LayoutUnit();
  tracks.ReserveCapacity(num_positions - 1);
  for (size_t i = 0; i < num_positions - 2; ++i)
    tracks.push_back(positions[i + 1] - positions[i] - offset_between_tracks -
                     gap);
  tracks.push_back(positions[num_positions - 1] - positions[num_positions - 2]);

  if (!has_collapsed_tracks)
    return tracks;

  size_t remaining_empty_tracks =
      grid_.AutoRepeatEmptyTracks(direction)->size();
  size_t last_line = tracks.size();
  gap = GridGap(direction);
  for (size_t i = 1; i < last_line; ++i) {
    if (grid_.IsEmptyAutoRepeatTrack(direction, i - 1)) {
      --remaining_empty_tracks;
    } else {
      // Remove the gap between consecutive non empty tracks. Remove it also
      // just once for an arbitrary number of empty tracks between two non empty
      // ones.
      bool all_remaining_tracks_are_empty =
          remaining_empty_tracks == (last_line - i);
      if (!all_remaining_tracks_are_empty ||
          !grid_.IsEmptyAutoRepeatTrack(direction, i))
        tracks[i - 1] -= gap;
    }
  }

  return tracks;
}

const StyleContentAlignmentData& LayoutGrid::ContentAlignmentNormalBehavior() {
  static const StyleContentAlignmentData kNormalBehavior = {
      kContentPositionNormal, kContentDistributionStretch};
  return kNormalBehavior;
}

void LayoutGrid::ApplyStretchAlignmentToTracksIfNeeded(
    GridTrackSizingDirection direction) {
  Optional<LayoutUnit> free_space =
      track_sizing_algorithm_.FreeSpace(direction);
  if (!free_space || free_space.value() <= 0 ||
      (direction == kForColumns &&
       StyleRef().ResolvedJustifyContentDistribution(
           ContentAlignmentNormalBehavior()) != kContentDistributionStretch) ||
      (direction == kForRows &&
       StyleRef().ResolvedAlignContentDistribution(
           ContentAlignmentNormalBehavior()) != kContentDistributionStretch))
    return;

  // Spec defines auto-sized tracks as the ones with an 'auto' max-sizing
  // function.
  Vector<GridTrack>& all_tracks = track_sizing_algorithm_.Tracks(direction);
  Vector<unsigned> auto_sized_tracks_index;
  for (unsigned i = 0; i < all_tracks.size(); ++i) {
    const GridTrackSize& track_size =
        track_sizing_algorithm_.GetGridTrackSize(direction, i);
    if (track_size.HasAutoMaxTrackBreadth())
      auto_sized_tracks_index.push_back(i);
  }

  unsigned number_of_auto_sized_tracks = auto_sized_tracks_index.size();
  if (number_of_auto_sized_tracks < 1)
    return;

  LayoutUnit size_to_increase =
      free_space.value() / number_of_auto_sized_tracks;
  for (const auto& track_index : auto_sized_tracks_index) {
    GridTrack* track = all_tracks.data() + track_index;
    LayoutUnit base_size = track->BaseSize() + size_to_increase;
    track->SetBaseSize(base_size);
  }
  track_sizing_algorithm_.SetFreeSpace(direction, LayoutUnit());
}

void LayoutGrid::LayoutGridItems() {
  PopulateGridPositionsForDirection(kForColumns);
  PopulateGridPositionsForDirection(kForRows);
  grid_items_overflowing_grid_area_.resize(0);

  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned()) {
      PrepareChildForPositionedLayout(*child);
      continue;
    }

    // Because the grid area cannot be styled, we don't need to adjust
    // the grid breadth to account for 'box-sizing'.
    LayoutUnit old_override_containing_block_content_logical_width =
        child->HasOverrideContainingBlockLogicalWidth()
            ? child->OverrideContainingBlockContentLogicalWidth()
            : LayoutUnit();
    LayoutUnit old_override_containing_block_content_logical_height =
        child->HasOverrideContainingBlockLogicalHeight()
            ? child->OverrideContainingBlockContentLogicalHeight()
            : LayoutUnit();

    LayoutUnit override_containing_block_content_logical_width =
        GridAreaBreadthForChildIncludingAlignmentOffsets(*child, kForColumns);
    LayoutUnit override_containing_block_content_logical_height =
        GridAreaBreadthForChildIncludingAlignmentOffsets(*child, kForRows);

    if (old_override_containing_block_content_logical_width !=
            override_containing_block_content_logical_width ||
        (old_override_containing_block_content_logical_height !=
             override_containing_block_content_logical_height &&
         child->HasRelativeLogicalHeight()))
      child->SetNeedsLayout(LayoutInvalidationReason::kGridChanged);

    child->SetOverrideContainingBlockContentLogicalWidth(
        override_containing_block_content_logical_width);
    child->SetOverrideContainingBlockContentLogicalHeight(
        override_containing_block_content_logical_height);

    // Stretching logic might force a child layout, so we need to run it before
    // the layoutIfNeeded call to avoid unnecessary relayouts. This might imply
    // that child margins, needed to correctly determine the available space
    // before stretching, are not set yet.
    ApplyStretchAlignmentToChildIfNeeded(*child);

    child->LayoutIfNeeded();

    // We need pending layouts to be done in order to compute auto-margins
    // properly.
    UpdateAutoMarginsInColumnAxisIfNeeded(*child);
    UpdateAutoMarginsInRowAxisIfNeeded(*child);

    const GridArea& area = grid_.GridItemArea(*child);
#if DCHECK_IS_ON()
    DCHECK_LT(area.columns.StartLine(),
              track_sizing_algorithm_.Tracks(kForColumns).size());
    DCHECK_LT(area.rows.StartLine(),
              track_sizing_algorithm_.Tracks(kForRows).size());
#endif
    child->SetLogicalLocation(FindChildLogicalPosition(*child));

    // Keep track of children overflowing their grid area as we might need to
    // paint them even if the grid-area is not visible. Using physical
    // dimensions for simplicity, so we can forget about orthogonalty.
    LayoutUnit child_grid_area_height =
        IsHorizontalWritingMode()
            ? override_containing_block_content_logical_height
            : override_containing_block_content_logical_width;
    LayoutUnit child_grid_area_width =
        IsHorizontalWritingMode()
            ? override_containing_block_content_logical_width
            : override_containing_block_content_logical_height;
    LayoutRect grid_area_rect(
        GridAreaLogicalPosition(area),
        LayoutSize(child_grid_area_width, child_grid_area_height));
    if (!grid_area_rect.Contains(child->FrameRect()))
      grid_items_overflowing_grid_area_.push_back(child);
  }
}

void LayoutGrid::PrepareChildForPositionedLayout(LayoutBox& child) {
  DCHECK(child.IsOutOfFlowPositioned());
  child.ContainingBlock()->InsertPositionedObject(&child);

  PaintLayer* child_layer = child.Layer();
  child_layer->SetStaticInlinePosition(LayoutUnit(BorderStart()));
  child_layer->SetStaticBlockPosition(LayoutUnit(BorderBefore()));
}

void LayoutGrid::LayoutPositionedObjects(bool relayout_children,
                                         PositionedLayoutBehavior info) {
  TrackedLayoutBoxListHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  for (auto* child : *positioned_descendants) {
    LayoutUnit column_offset = LayoutUnit();
    LayoutUnit column_breadth = LayoutUnit();
    OffsetAndBreadthForPositionedChild(*child, kForColumns, column_offset,
                                       column_breadth);
    LayoutUnit row_offset = LayoutUnit();
    LayoutUnit row_breadth = LayoutUnit();
    OffsetAndBreadthForPositionedChild(*child, kForRows, row_offset,
                                       row_breadth);

    child->SetOverrideContainingBlockContentLogicalWidth(column_breadth);
    child->SetOverrideContainingBlockContentLogicalHeight(row_breadth);

    // Mark for layout as we're resetting the position before and we relay in
    // generic layout logic for positioned items in order to get the offsets
    // properly resolved.
    child->SetNeedsLayout(LayoutInvalidationReason::kGridChanged,
                          kMarkOnlyThis);

    LayoutPositionedObject(child, relayout_children, info);

    bool is_orthogonal_child = IsOrthogonalChild(*child);
    LayoutUnit logical_left =
        child->LogicalLeft() +
        (is_orthogonal_child ? row_offset : column_offset);
    LayoutUnit logical_top = child->LogicalTop() +
                             (is_orthogonal_child ? column_offset : row_offset);
    child->SetLogicalLocation(LayoutPoint(logical_left, logical_top));
  }
}

void LayoutGrid::OffsetAndBreadthForPositionedChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction,
    LayoutUnit& offset,
    LayoutUnit& breadth) {
  bool is_for_columns = direction == kForColumns;

  GridSpan positions = GridPositionsResolver::ResolveGridPositionsFromStyle(
      *Style(), child, direction, AutoRepeatCountForDirection(direction));
  if (positions.IsIndefinite()) {
    offset = LayoutUnit();
    breadth = is_for_columns ? ClientLogicalWidth() : ClientLogicalHeight();
    return;
  }

  // For positioned items we cannot use GridSpan::translate(). Because we could
  // end up with negative values, as the positioned items do not create implicit
  // tracks per spec.
  int smallest_start = abs(grid_.SmallestTrackStart(direction));
  int start_line = positions.UntranslatedStartLine() + smallest_start;
  int end_line = positions.UntranslatedEndLine() + smallest_start;

  GridPosition start_position = is_for_columns
                                    ? child.Style()->GridColumnStart()
                                    : child.Style()->GridRowStart();
  GridPosition end_position = is_for_columns ? child.Style()->GridColumnEnd()
                                             : child.Style()->GridRowEnd();
  int last_line = NumTracks(direction, grid_);

  bool start_is_auto =
      start_position.IsAuto() ||
      (start_position.IsNamedGridArea() &&
       !NamedLineCollection::IsValidNamedLineOrArea(
           start_position.NamedGridLine(), StyleRef(),
           GridPositionsResolver::InitialPositionSide(direction))) ||
      (start_line < 0) || (start_line > last_line);
  bool end_is_auto =
      end_position.IsAuto() ||
      (end_position.IsNamedGridArea() &&
       !NamedLineCollection::IsValidNamedLineOrArea(
           end_position.NamedGridLine(), StyleRef(),
           GridPositionsResolver::FinalPositionSide(direction))) ||
      (end_line < 0) || (end_line > last_line);

  Optional<LayoutUnit> available_size_for_gutters =
      AvailableSpaceForGutters(direction);
  LayoutUnit start;
  if (!start_is_auto) {
    if (is_for_columns) {
      if (StyleRef().IsLeftToRightDirection())
        start = column_positions_[start_line] - BorderLogicalLeft();
      else
        start = LogicalWidth() -
                TranslateRTLCoordinate(column_positions_[start_line]) -
                BorderLogicalRight();
    } else {
      start = row_positions_[start_line] - BorderBefore();
    }
  }

  LayoutUnit end =
      is_for_columns ? ClientLogicalWidth() : ClientLogicalHeight();
  if (!end_is_auto) {
    if (is_for_columns) {
      if (StyleRef().IsLeftToRightDirection())
        end = column_positions_[end_line] - BorderLogicalLeft();
      else
        end = LogicalWidth() -
              TranslateRTLCoordinate(column_positions_[end_line]) -
              BorderLogicalRight();
    } else {
      end = row_positions_[end_line] - BorderBefore();
    }

    // These vectors store line positions including gaps, but we shouldn't
    // consider them for the edges of the grid.
    if (end_line > 0 && end_line < last_line) {
      DCHECK(!grid_.NeedsItemsPlacement());
      end -= GuttersSize(grid_, direction, end_line - 1, 2,
                         available_size_for_gutters);
      end -= is_for_columns ? offset_between_columns_ : offset_between_rows_;
    }
  }

  breadth = std::max(end - start, LayoutUnit());
  offset = start;

  if (is_for_columns && !StyleRef().IsLeftToRightDirection()) {
    // We always want to calculate the static position from the left
    // (even if we're in RTL).
    if (end_is_auto) {
      offset = LayoutUnit();
    } else {
      offset = TranslateRTLCoordinate(column_positions_[end_line]) -
               BorderLogicalLeft();

      if (end_line > 0 && end_line < last_line) {
        DCHECK(!grid_.NeedsItemsPlacement());
        offset += GuttersSize(grid_, direction, end_line - 1, 2,
                              available_size_for_gutters);
        offset +=
            is_for_columns ? offset_between_columns_ : offset_between_rows_;
      }
    }
  }
}

LayoutUnit LayoutGrid::GridAreaBreadthForChildIncludingAlignmentOffsets(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  // We need the cached value when available because Content Distribution
  // alignment properties may have some influence in the final grid area
  // breadth.
  const Vector<GridTrack>& tracks = track_sizing_algorithm_.Tracks(direction);
  const GridSpan& span =
      track_sizing_algorithm_.GetGrid().GridItemSpan(child, direction);
  const Vector<LayoutUnit>& line_positions =
      (direction == kForColumns) ? column_positions_ : row_positions_;
  LayoutUnit initial_track_position = line_positions[span.StartLine()];
  LayoutUnit final_track_position = line_positions[span.EndLine() - 1];
  // Track Positions vector stores the 'start' grid line of each track, so we
  // have to add last track's baseSize.
  return final_track_position - initial_track_position +
         tracks[span.EndLine() - 1].BaseSize();
}

void LayoutGrid::PopulateGridPositionsForDirection(
    GridTrackSizingDirection direction) {
  // Since we add alignment offsets and track gutters, grid lines are not always
  // adjacent. Hence we will have to assume from now on that we just store
  // positions of the initial grid lines of each track, except the last one,
  // which is the only one considered as a final grid line of a track.

  // The grid container's frame elements (border, padding and <content-position>
  // offset) are sensible to the inline-axis flow direction. However, column
  // lines positions are 'direction' unaware. This simplification allows us to
  // use the same indexes to identify the columns independently on the
  // inline-axis direction.
  bool is_row_axis = direction == kForColumns;
  auto& tracks = track_sizing_algorithm_.Tracks(direction);
  size_t number_of_tracks = tracks.size();
  size_t number_of_lines = number_of_tracks + 1;
  size_t last_line = number_of_lines - 1;
  bool has_collapsed_tracks = grid_.HasAutoRepeatEmptyTracks(direction);
  size_t number_of_collapsed_tracks =
      has_collapsed_tracks ? grid_.AutoRepeatEmptyTracks(direction)->size() : 0;
  ContentAlignmentData offset = ComputeContentPositionAndDistributionOffset(
      direction, track_sizing_algorithm_.FreeSpace(direction).value(),
      number_of_tracks - number_of_collapsed_tracks);
  auto& positions = is_row_axis ? column_positions_ : row_positions_;
  positions.resize(number_of_lines);
  auto border_and_padding =
      is_row_axis ? BorderAndPaddingLogicalLeft() : BorderAndPaddingBefore();
  positions[0] = border_and_padding + offset.position_offset;
  if (number_of_lines > 1) {
    // If we have collapsed tracks we just ignore gaps here and add them later
    // as we might not compute the gap between two consecutive tracks without
    // examining the surrounding ones.
    LayoutUnit gap = !has_collapsed_tracks ? GridGap(direction) : LayoutUnit();
    size_t next_to_last_line = number_of_lines - 2;
    for (size_t i = 0; i < next_to_last_line; ++i)
      positions[i + 1] = positions[i] + offset.distribution_offset +
                         tracks[i].BaseSize() + gap;
    positions[last_line] =
        positions[next_to_last_line] + tracks[next_to_last_line].BaseSize();

    // Adjust collapsed gaps. Collapsed tracks cause the surrounding gutters to
    // collapse (they coincide exactly) except on the edges of the grid where
    // they become 0.
    if (has_collapsed_tracks) {
      gap = GridGap(direction);
      size_t remaining_empty_tracks = number_of_collapsed_tracks;
      LayoutUnit offset_accumulator;
      LayoutUnit gap_accumulator;
      for (size_t i = 1; i < last_line; ++i) {
        if (grid_.IsEmptyAutoRepeatTrack(direction, i - 1)) {
          --remaining_empty_tracks;
          offset_accumulator += offset.distribution_offset;
        } else {
          // Add gap between consecutive non empty tracks. Add it also just once
          // for an arbitrary number of empty tracks between two non empty ones.
          bool all_remaining_tracks_are_empty =
              remaining_empty_tracks == (last_line - i);
          if (!all_remaining_tracks_are_empty ||
              !grid_.IsEmptyAutoRepeatTrack(direction, i))
            gap_accumulator += gap;
        }
        positions[i] += gap_accumulator - offset_accumulator;
      }
      positions[last_line] += gap_accumulator - offset_accumulator;
    }
  }
  auto& offset_between_tracks =
      is_row_axis ? offset_between_columns_ : offset_between_rows_;
  offset_between_tracks = offset.distribution_offset;
}

static LayoutUnit ComputeOverflowAlignmentOffset(OverflowAlignment overflow,
                                                 LayoutUnit track_size,
                                                 LayoutUnit child_size) {
  LayoutUnit offset = track_size - child_size;
  switch (overflow) {
    case kOverflowAlignmentSafe:
      // If overflow is 'safe', we have to make sure we don't overflow the
      // 'start' edge (potentially cause some data loss as the overflow is
      // unreachable).
      return offset.ClampNegativeToZero();
    case kOverflowAlignmentUnsafe:
    case kOverflowAlignmentDefault:
      // If we overflow our alignment container and overflow is 'true'
      // (default), we ignore the overflow and just return the value regardless
      // (which may cause data loss as we overflow the 'start' edge).
      return offset;
  }

  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::MarginLogicalSizeForChild(
    GridTrackSizingDirection direction,
    const LayoutBox& child) const {
  return FlowAwareDirectionForChild(child, direction) == kForColumns
             ? child.MarginLogicalWidth()
             : child.MarginLogicalHeight();
}

LayoutUnit LayoutGrid::ComputeMarginLogicalSizeForChild(
    MarginDirection for_direction,
    const LayoutBox& child) const {
  if (!child.StyleRef().HasMargin())
    return LayoutUnit();

  bool is_row_axis = for_direction == kInlineDirection;
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  LayoutUnit logical_size =
      is_row_axis ? child.LogicalWidth() : child.LogicalHeight();
  Length margin_start_length = is_row_axis ? child.StyleRef().MarginStart()
                                           : child.StyleRef().MarginBefore();
  Length margin_end_length = is_row_axis ? child.StyleRef().MarginEnd()
                                         : child.StyleRef().MarginAfter();
  child.ComputeMarginsForDirection(
      for_direction, this, child.ContainingBlockLogicalWidthForContent(),
      logical_size, margin_start, margin_end, margin_start_length,
      margin_end_length);

  return margin_start + margin_end;
}

LayoutUnit LayoutGrid::AvailableAlignmentSpaceForChildBeforeStretching(
    LayoutUnit grid_area_breadth_for_child,
    const LayoutBox& child) const {
  // Because we want to avoid multiple layouts, stretching logic might be
  // performed before children are laid out, so we can't use the child cached
  // values. Hence, we need to compute margins in order to determine the
  // available height before stretching.
  GridTrackSizingDirection child_block_flow_direction =
      FlowAwareDirectionForChild(child, kForRows);
  return grid_area_breadth_for_child -
         (child.NeedsLayout()
              ? ComputeMarginLogicalSizeForChild(kBlockDirection, child)
              : MarginLogicalSizeForChild(child_block_flow_direction, child));
}

StyleSelfAlignmentData LayoutGrid::AlignSelfForChild(
    const LayoutBox& child,
    const ComputedStyle* style) const {
  if (!style)
    style = Style();
  return child.StyleRef().ResolvedAlignSelf(SelfAlignmentNormalBehavior(&child),
                                            style);
}

StyleSelfAlignmentData LayoutGrid::JustifySelfForChild(
    const LayoutBox& child,
    const ComputedStyle* style) const {
  if (!style)
    style = Style();
  return child.StyleRef().ResolvedJustifySelf(
      SelfAlignmentNormalBehavior(&child), style);
}

GridTrackSizingDirection LayoutGrid::FlowAwareDirectionForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  return !IsOrthogonalChild(child)
             ? direction
             : (direction == kForColumns ? kForRows : kForColumns);
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to
// LayoutBox.
void LayoutGrid::ApplyStretchAlignmentToChildIfNeeded(LayoutBox& child) {
  GridTrackSizingDirection child_block_direction =
      FlowAwareDirectionForChild(child, kForRows);
  bool block_flow_is_column_axis = child_block_direction == kForRows;
  bool allowed_to_stretch_child_block_size =
      block_flow_is_column_axis ? AllowedToStretchChildAlongColumnAxis(child)
                                : AllowedToStretchChildAlongRowAxis(child);
  if (allowed_to_stretch_child_block_size) {
    LayoutUnit stretched_logical_height =
        AvailableAlignmentSpaceForChildBeforeStretching(
            OverrideContainingBlockContentSizeForChild(child,
                                                       child_block_direction),
            child);
    LayoutUnit desired_logical_height = child.ConstrainLogicalHeightByMinMax(
        stretched_logical_height, LayoutUnit(-1));
    child.SetOverrideLogicalContentHeight(
        (desired_logical_height - child.BorderAndPaddingLogicalHeight())
            .ClampNegativeToZero());
    if (desired_logical_height != child.LogicalHeight()) {
      // TODO (lajava): Can avoid laying out here in some cases. See
      // https://webkit.org/b/87905.
      child.SetLogicalHeight(LayoutUnit());
      child.SetNeedsLayout(LayoutInvalidationReason::kGridChanged);
    }
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
bool LayoutGrid::HasAutoMarginsInColumnAxis(const LayoutBox& child) const {
  if (IsHorizontalWritingMode())
    return child.StyleRef().MarginTop().IsAuto() ||
           child.StyleRef().MarginBottom().IsAuto();
  return child.StyleRef().MarginLeft().IsAuto() ||
         child.StyleRef().MarginRight().IsAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
bool LayoutGrid::HasAutoMarginsInRowAxis(const LayoutBox& child) const {
  if (IsHorizontalWritingMode())
    return child.StyleRef().MarginLeft().IsAuto() ||
           child.StyleRef().MarginRight().IsAuto();
  return child.StyleRef().MarginTop().IsAuto() ||
         child.StyleRef().MarginBottom().IsAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
DISABLE_CFI_PERF
void LayoutGrid::UpdateAutoMarginsInRowAxisIfNeeded(LayoutBox& child) {
  DCHECK(!child.IsOutOfFlowPositioned());

  LayoutUnit available_alignment_space =
      child.OverrideContainingBlockContentLogicalWidth() -
      child.LogicalWidth() - child.MarginLogicalWidth();
  if (available_alignment_space <= 0)
    return;

  Length margin_start = child.Style()->MarginStartUsing(Style());
  Length margin_end = child.Style()->MarginEndUsing(Style());
  if (margin_start.IsAuto() && margin_end.IsAuto()) {
    child.SetMarginStart(available_alignment_space / 2, Style());
    child.SetMarginEnd(available_alignment_space / 2, Style());
  } else if (margin_start.IsAuto()) {
    child.SetMarginStart(available_alignment_space, Style());
  } else if (margin_end.IsAuto()) {
    child.SetMarginEnd(available_alignment_space, Style());
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
DISABLE_CFI_PERF
void LayoutGrid::UpdateAutoMarginsInColumnAxisIfNeeded(LayoutBox& child) {
  DCHECK(!child.IsOutOfFlowPositioned());

  LayoutUnit available_alignment_space =
      child.OverrideContainingBlockContentLogicalHeight() -
      child.LogicalHeight() - child.MarginLogicalHeight();
  if (available_alignment_space <= 0)
    return;

  Length margin_before = child.Style()->MarginBeforeUsing(Style());
  Length margin_after = child.Style()->MarginAfterUsing(Style());
  if (margin_before.IsAuto() && margin_after.IsAuto()) {
    child.SetMarginBefore(available_alignment_space / 2, Style());
    child.SetMarginAfter(available_alignment_space / 2, Style());
  } else if (margin_before.IsAuto()) {
    child.SetMarginBefore(available_alignment_space, Style());
  } else if (margin_after.IsAuto()) {
    child.SetMarginAfter(available_alignment_space, Style());
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it might be
// refactored somehow.
int LayoutGrid::SynthesizedBaselineFromContentBox(const LayoutBox& box,
                                                  LineDirectionMode direction) {
  if (direction == kHorizontalLine) {
    return (box.Size().Height() - box.BorderBottom() - box.PaddingBottom() -
            box.HorizontalScrollbarHeight())
        .ToInt();
  }
  return (box.Size().Width() - box.BorderLeft() - box.PaddingLeft() -
          box.VerticalScrollbarWidth())
      .ToInt();
}

int LayoutGrid::SynthesizedBaselineFromBorderBox(const LayoutBox& box,
                                                 LineDirectionMode direction) {
  return (direction == kHorizontalLine ? box.Size().Height()
                                       : box.Size().Width())
      .ToInt();
}

int LayoutGrid::BaselinePosition(FontBaseline,
                                 bool,
                                 LineDirectionMode direction,
                                 LinePositionMode mode) const {
  DCHECK_EQ(mode, kPositionOnContainingLine);
  int baseline = FirstLineBoxBaseline();
  // We take content-box's bottom if no valid baseline.
  if (baseline == -1)
    baseline = SynthesizedBaselineFromContentBox(*this, direction);

  return baseline + BeforeMarginInLineDirection(direction);
}

int LayoutGrid::FirstLineBoxBaseline() const {
  if (IsWritingModeRoot() || !grid_.HasGridItems())
    return -1;
  const LayoutBox* baseline_child = nullptr;
  const LayoutBox* first_child = nullptr;
  bool is_baseline_aligned = false;
  // Finding the first grid item in grid order.
  for (size_t column = 0;
       !is_baseline_aligned && column < grid_.NumTracks(kForColumns);
       column++) {
    for (size_t index = 0; index < grid_.Cell(0, column).size(); index++) {
      const LayoutBox* child = grid_.Cell(0, column)[index];
      DCHECK(!child->IsOutOfFlowPositioned());
      // If an item participates in baseline alignment, we select such item.
      if (IsBaselineAlignmentForChild(*child)) {
        // TODO (lajava): self-baseline and content-baseline alignment
        // still not implemented.
        baseline_child = child;
        is_baseline_aligned = true;
        break;
      }
      if (!baseline_child) {
        // Use dom order for items in the same cell.
        if (!first_child || (grid_.GridItemPaintOrder(*child) <
                             grid_.GridItemPaintOrder(*first_child)))
          first_child = child;
      }
    }
    if (!baseline_child && first_child)
      baseline_child = first_child;
  }

  if (!baseline_child)
    return -1;

  int baseline = IsOrthogonalChild(*baseline_child)
                     ? -1
                     : baseline_child->FirstLineBoxBaseline();
  // We take border-box's bottom if no valid baseline.
  if (baseline == -1) {
    // TODO (lajava): We should pass |direction| into
    // firstLineBoxBaseline and stop bailing out if we're a writing
    // mode root.  This would also fix some cases where the grid is
    // orthogonal to its container.
    LineDirectionMode direction =
        IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine;
    return (SynthesizedBaselineFromBorderBox(*baseline_child, direction) +
            baseline_child->LogicalTop())
        .ToInt();
  }

  return (baseline + baseline_child->LogicalTop()).ToInt();
}

int LayoutGrid::InlineBlockBaseline(LineDirectionMode direction) const {
  int baseline = FirstLineBoxBaseline();
  if (baseline != -1)
    return baseline;

  int margin_height =
      (direction == kHorizontalLine ? MarginTop() : MarginRight()).ToInt();
  return SynthesizedBaselineFromContentBox(*this, direction) + margin_height;
}

bool LayoutGrid::IsHorizontalGridAxis(GridAxis axis) const {
  return axis == kGridRowAxis ? IsHorizontalWritingMode()
                              : !IsHorizontalWritingMode();
}

bool LayoutGrid::IsParallelToBlockAxisForChild(const LayoutBox& child,
                                               GridAxis axis) const {
  return axis == kGridColumnAxis ? !IsOrthogonalChild(child)
                                 : IsOrthogonalChild(child);
}

bool LayoutGrid::IsDescentBaselineForChild(const LayoutBox& child,
                                           GridAxis baseline_axis) const {
  return IsHorizontalGridAxis(baseline_axis) &&
         ((child.StyleRef().IsFlippedBlocksWritingMode() &&
           !StyleRef().IsFlippedBlocksWritingMode()) ||
          (child.StyleRef().IsFlippedLinesWritingMode() &&
           StyleRef().IsFlippedBlocksWritingMode()));
}

bool LayoutGrid::IsBaselineAlignmentForChild(const LayoutBox& child,
                                             GridAxis baseline_axis) const {
  ItemPosition align =
      SelfAlignmentForChild(baseline_axis, child).GetPosition();
  bool has_auto_margins = baseline_axis == kGridColumnAxis
                              ? HasAutoMarginsInColumnAxis(child)
                              : HasAutoMarginsInRowAxis(child);
  return IsBaselinePosition(align) && !has_auto_margins;
}

const BaselineGroup& LayoutGrid::GetBaselineGroupForChild(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  DCHECK(IsBaselineAlignmentForChild(child, baseline_axis));
  auto& grid = track_sizing_algorithm_.GetGrid();
  bool is_column_axis_baseline = baseline_axis == kGridColumnAxis;
  bool is_row_axis_context = is_column_axis_baseline;
  const auto& span = is_row_axis_context
                         ? grid.GridItemSpan(child, kForRows)
                         : grid.GridItemSpan(child, kForColumns);
  auto& contexts_map = is_row_axis_context ? row_axis_alignment_context_
                                           : col_axis_alignment_context_;
  auto* context = contexts_map.at(span.StartLine());
  DCHECK(context);
  ItemPosition align =
      SelfAlignmentForChild(baseline_axis, child).GetPosition();
  return context->GetSharedGroup(child, align);
}

LayoutUnit LayoutGrid::MarginOverForChild(const LayoutBox& child,
                                          GridAxis axis) const {
  return IsHorizontalGridAxis(axis) ? child.MarginRight() : child.MarginTop();
}

LayoutUnit LayoutGrid::MarginUnderForChild(const LayoutBox& child,
                                           GridAxis axis) const {
  return IsHorizontalGridAxis(axis) ? child.MarginLeft() : child.MarginBottom();
}

LayoutUnit LayoutGrid::LogicalAscentForChild(const LayoutBox& child,
                                             GridAxis baseline_axis) const {
  LayoutUnit ascent = AscentForChild(child, baseline_axis);
  return IsDescentBaselineForChild(child, baseline_axis)
             ? DescentForChild(child, ascent, baseline_axis)
             : ascent;
}

LayoutUnit LayoutGrid::AscentForChild(const LayoutBox& child,
                                      GridAxis baseline_axis) const {
  LayoutUnit margin = IsDescentBaselineForChild(child, baseline_axis)
                          ? MarginUnderForChild(child, baseline_axis)
                          : MarginOverForChild(child, baseline_axis);
  int baseline = IsParallelToBlockAxisForChild(child, baseline_axis)
                     ? child.FirstLineBoxBaseline()
                     : -1;
  // We take border-box's under edge if no valid baseline.
  if (baseline == -1) {
    if (IsHorizontalGridAxis(baseline_axis)) {
      return StyleRef().IsFlippedBlocksWritingMode()
                 ? child.Size().Width().ToInt() + margin
                 : margin;
    }
    return child.Size().Height() + margin;
  }
  return LayoutUnit(baseline) + margin;
}

LayoutUnit LayoutGrid::DescentForChild(const LayoutBox& child,
                                       LayoutUnit ascent,
                                       GridAxis baseline_axis) const {
  if (IsParallelToBlockAxisForChild(child, baseline_axis))
    return child.MarginLogicalHeight() + child.LogicalHeight() - ascent;
  return child.MarginLogicalWidth() + child.LogicalWidth() - ascent;
}

bool LayoutGrid::IsBaselineContextComputed(GridAxis baseline_axis) const {
  return baseline_axis == kGridColumnAxis
             ? !row_axis_alignment_context_.IsEmpty()
             : !col_axis_alignment_context_.IsEmpty();
}

bool LayoutGrid::BaselineMayAffectIntrinsicWidth() const {
  if (!StyleRef().LogicalWidth().IsIntrinsicOrAuto())
    return false;
  for (const auto& context : col_axis_alignment_context_) {
    for (const auto& group : context.value->SharedGroups()) {
      if (group.size() > 1)
        return true;
    }
  }
  return false;
}

bool LayoutGrid::BaselineMayAffectIntrinsicHeight() const {
  if (!StyleRef().LogicalHeight().IsIntrinsicOrAuto())
    return false;
  for (const auto& context : row_axis_alignment_context_) {
    for (const auto& group : context.value->SharedGroups()) {
      if (group.size() > 1)
        return true;
    }
  }
  return false;
}

void LayoutGrid::ComputeBaselineAlignmentContext() {
  for (auto* child = FirstInFlowChildBox(); child;
       child = child->NextInFlowSiblingBox()) {
    UpdateBaselineAlignmentContextIfNeeded(*child, kGridRowAxis);
    UpdateBaselineAlignmentContextIfNeeded(*child, kGridColumnAxis);
  }
}

void LayoutGrid::UpdateBaselineAlignmentContextIfNeeded(
    LayoutBox& child,
    GridAxis baseline_axis) {
  // TODO (lajava): We must ensure this method is not called as part of an
  // intrinsic size computation.
  if (!IsBaselineAlignmentForChild(child, baseline_axis))
    return;

  child.LayoutIfNeeded();

  // Determine Ascent and Descent values of this child with respect to
  // its grid container.
  LayoutUnit ascent = AscentForChild(child, baseline_axis);
  LayoutUnit descent = DescentForChild(child, ascent, baseline_axis);
  if (IsDescentBaselineForChild(child, baseline_axis))
    std::swap(ascent, descent);

  // Looking up for a shared alignment context perpendicular to the
  // baseline axis.
  auto& grid = track_sizing_algorithm_.GetGrid();
  bool is_column_axis_baseline = baseline_axis == kGridColumnAxis;
  bool is_row_axis_context = is_column_axis_baseline;
  const auto& span = is_row_axis_context
                         ? grid.GridItemSpan(child, kForRows)
                         : grid.GridItemSpan(child, kForColumns);
  auto& contexts_map = is_row_axis_context ? row_axis_alignment_context_
                                           : col_axis_alignment_context_;
  auto add_result = contexts_map.insert(span.StartLine(), nullptr);

  // Looking for a compatible baseline-sharing group.
  ItemPosition align =
      SelfAlignmentForChild(baseline_axis, child).GetPosition();
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        WTF::MakeUnique<BaselineContext>(child, align, ascent, descent);
  } else {
    auto* context = add_result.stored_value->value.get();
    context->UpdateSharedGroup(child, align, ascent, descent);
  }
}

LayoutUnit LayoutGrid::ColumnAxisBaselineOffsetForChild(
    const LayoutBox& child) const {
  if (!IsBaselineAlignmentForChild(child, kGridColumnAxis))
    return LayoutUnit();
  auto& group = GetBaselineGroupForChild(child, kGridColumnAxis);
  if (group.size() > 1)
    return group.MaxAscent() - LogicalAscentForChild(child, kGridColumnAxis);
  return LayoutUnit();
}

LayoutUnit LayoutGrid::RowAxisBaselineOffsetForChild(
    const LayoutBox& child) const {
  if (!IsBaselineAlignmentForChild(child, kGridRowAxis))
    return LayoutUnit();
  auto& group = GetBaselineGroupForChild(child, kGridRowAxis);
  if (group.size() > 1)
    return group.MaxAscent() - LogicalAscentForChild(child, kGridRowAxis);
  return LayoutUnit();
}

GridAxisPosition LayoutGrid::ColumnAxisPositionForChild(
    const LayoutBox& child) const {
  bool has_same_writing_mode =
      child.StyleRef().GetWritingMode() == StyleRef().GetWritingMode();
  bool child_is_ltr = child.StyleRef().IsLeftToRightDirection();

  switch (AlignSelfForChild(child).GetPosition()) {
    case kItemPositionSelfStart:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'start' side in the
      // column axis.
      if (IsOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-start will be based on the child's
        // inline-axis direction (inline-start), because it's the one parallel
        // to the column axis.
        if (StyleRef().IsFlippedBlocksWritingMode())
          return child_is_ltr ? kGridAxisEnd : kGridAxisStart;
        return child_is_ltr ? kGridAxisStart : kGridAxisEnd;
      }
      // self-start is based on the child's block-flow direction. That's why we
      // need to check against the grid container's block-flow direction.
      return has_same_writing_mode ? kGridAxisStart : kGridAxisEnd;
    case kItemPositionSelfEnd:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'end' side in the
      // column axis.
      if (IsOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-end will be based on the child's
        // inline-axis direction, (inline-end) because it's the one parallel to
        // the column axis.
        if (StyleRef().IsFlippedBlocksWritingMode())
          return child_is_ltr ? kGridAxisStart : kGridAxisEnd;
        return child_is_ltr ? kGridAxisEnd : kGridAxisStart;
      }
      // self-end is based on the child's block-flow direction. That's why we
      // need to check against the grid container's block-flow direction.
      return has_same_writing_mode ? kGridAxisEnd : kGridAxisStart;
    case kItemPositionLeft:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-left' edge. The alignment axis (column axis) is always orthogonal
      // to the inline axis, hence this value behaves as 'start'.
      return kGridAxisStart;
    case kItemPositionRight:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-right' edge. The alignment axis (column axis) is always
      // orthogonal to the inline axis, hence this value behaves as 'start'.
      return kGridAxisStart;
    case kItemPositionCenter:
      return kGridAxisCenter;
    // Only used in flex layout, otherwise equivalent to 'start'.
    case kItemPositionFlexStart:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'start' edge (block-start) in the column axis.
    case kItemPositionStart:
      return kGridAxisStart;
    // Only used in flex layout, otherwise equivalent to 'end'.
    case kItemPositionFlexEnd:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'end' edge (block-end) in the column axis.
    case kItemPositionEnd:
      return kGridAxisEnd;
    case kItemPositionStretch:
      return kGridAxisStart;
    case kItemPositionBaseline:
    case kItemPositionLastBaseline:
      return kGridAxisStart;
    case kItemPositionAuto:
    case kItemPositionNormal:
      break;
  }

  NOTREACHED();
  return kGridAxisStart;
}

GridAxisPosition LayoutGrid::RowAxisPositionForChild(
    const LayoutBox& child) const {
  bool has_same_direction =
      child.StyleRef().Direction() == StyleRef().Direction();
  bool grid_is_ltr = StyleRef().IsLeftToRightDirection();

  switch (JustifySelfForChild(child).GetPosition()) {
    case kItemPositionSelfStart:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'start' side in the
      // row axis.
      if (IsOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-start will be based on the child's
        // block-axis direction, because it's the one parallel to the row axis.
        if (child.StyleRef().IsFlippedBlocksWritingMode())
          return grid_is_ltr ? kGridAxisEnd : kGridAxisStart;
        return grid_is_ltr ? kGridAxisStart : kGridAxisEnd;
      }
      // self-start is based on the child's inline-flow direction. That's why we
      // need to check against the grid container's direction.
      return has_same_direction ? kGridAxisStart : kGridAxisEnd;
    case kItemPositionSelfEnd:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'end' side in the
      // row axis.
      if (IsOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-end will be based on the child's
        // block-axis direction, because it's the one parallel to the row axis.
        if (child.StyleRef().IsFlippedBlocksWritingMode())
          return grid_is_ltr ? kGridAxisStart : kGridAxisEnd;
        return grid_is_ltr ? kGridAxisEnd : kGridAxisStart;
      }
      // self-end is based on the child's inline-flow direction. That's why we
      // need to check against the grid container's direction.
      return has_same_direction ? kGridAxisEnd : kGridAxisStart;
    case kItemPositionLeft:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-left' edge. We want the physical 'left' side, so we have to take
      // account, container's inline-flow direction.
      return grid_is_ltr ? kGridAxisStart : kGridAxisEnd;
    case kItemPositionRight:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-right' edge. We want the physical 'right' side, so we have to
      // take account, container's inline-flow direction.
      return grid_is_ltr ? kGridAxisEnd : kGridAxisStart;
    case kItemPositionCenter:
      return kGridAxisCenter;
    // Only used in flex layout, otherwise equivalent to 'start'.
    case kItemPositionFlexStart:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'start' edge (inline-start) in the row axis.
    case kItemPositionStart:
      return kGridAxisStart;
    // Only used in flex layout, otherwise equivalent to 'end'.
    case kItemPositionFlexEnd:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'end' edge (inline-end) in the row axis.
    case kItemPositionEnd:
      return kGridAxisEnd;
    case kItemPositionStretch:
      return kGridAxisStart;
    case kItemPositionBaseline:
    case kItemPositionLastBaseline:
      return kGridAxisStart;
    case kItemPositionAuto:
    case kItemPositionNormal:
      break;
  }

  NOTREACHED();
  return kGridAxisStart;
}

LayoutUnit LayoutGrid::ColumnAxisOffsetForChild(const LayoutBox& child) const {
  const GridSpan& rows_span =
      track_sizing_algorithm_.GetGrid().GridItemSpan(child, kForRows);
  size_t child_start_line = rows_span.StartLine();
  LayoutUnit start_of_row = row_positions_[child_start_line];
  LayoutUnit start_position = start_of_row + MarginBeforeForChild(child);
  if (HasAutoMarginsInColumnAxis(child))
    return start_position;
  GridAxisPosition axis_position = ColumnAxisPositionForChild(child);
  switch (axis_position) {
    case kGridAxisStart:
      return start_position + ColumnAxisBaselineOffsetForChild(child);
    case kGridAxisEnd:
    case kGridAxisCenter: {
      size_t child_end_line = rows_span.EndLine();
      LayoutUnit end_of_row = row_positions_[child_end_line];
      // m_rowPositions include distribution offset (because of content
      // alignment) and gutters so we need to subtract them to get the actual
      // end position for a given row (this does not have to be done for the
      // last track as there are no more m_columnPositions after it).
      LayoutUnit track_gap = GridGap(kForRows);
      if (child_end_line < row_positions_.size() - 1) {
        end_of_row -= track_gap;
        end_of_row -= offset_between_rows_;
      }
      LayoutUnit column_axis_child_size =
          IsOrthogonalChild(child)
              ? child.LogicalWidth() + child.MarginLogicalWidth()
              : child.LogicalHeight() + child.MarginLogicalHeight();
      OverflowAlignment overflow = AlignSelfForChild(child).Overflow();
      LayoutUnit offset_from_start_position = ComputeOverflowAlignmentOffset(
          overflow, end_of_row - start_of_row, column_axis_child_size);
      return start_position + (axis_position == kGridAxisEnd
                                   ? offset_from_start_position
                                   : offset_from_start_position / 2);
    }
  }

  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::RowAxisOffsetForChild(const LayoutBox& child) const {
  const GridSpan& columns_span =
      track_sizing_algorithm_.GetGrid().GridItemSpan(child, kForColumns);
  size_t child_start_line = columns_span.StartLine();
  LayoutUnit start_of_column = column_positions_[child_start_line];
  LayoutUnit start_position = start_of_column + MarginStartForChild(child);
  if (HasAutoMarginsInRowAxis(child))
    return start_position;
  GridAxisPosition axis_position = RowAxisPositionForChild(child);
  switch (axis_position) {
    case kGridAxisStart:
      return start_position + RowAxisBaselineOffsetForChild(child);
    case kGridAxisEnd:
    case kGridAxisCenter: {
      size_t child_end_line = columns_span.EndLine();
      LayoutUnit end_of_column = column_positions_[child_end_line];
      // m_columnPositions include distribution offset (because of content
      // alignment) and gutters so we need to subtract them to get the actual
      // end position for a given column (this does not have to be done for the
      // last track as there are no more m_columnPositions after it).
      LayoutUnit track_gap = GridGap(kForColumns);
      if (child_end_line < column_positions_.size() - 1) {
        end_of_column -= track_gap;
        end_of_column -= offset_between_columns_;
      }
      LayoutUnit row_axis_child_size =
          IsOrthogonalChild(child)
              ? child.LogicalHeight() + child.MarginLogicalHeight()
              : child.LogicalWidth() + child.MarginLogicalWidth();
      OverflowAlignment overflow = JustifySelfForChild(child).Overflow();
      LayoutUnit offset_from_start_position = ComputeOverflowAlignmentOffset(
          overflow, end_of_column - start_of_column, row_axis_child_size);
      return start_position + (axis_position == kGridAxisEnd
                                   ? offset_from_start_position
                                   : offset_from_start_position / 2);
    }
  }

  NOTREACHED();
  return LayoutUnit();
}

ContentPosition static ResolveContentDistributionFallback(
    ContentDistributionType distribution) {
  switch (distribution) {
    case kContentDistributionSpaceBetween:
      return kContentPositionStart;
    case kContentDistributionSpaceAround:
      return kContentPositionCenter;
    case kContentDistributionSpaceEvenly:
      return kContentPositionCenter;
    case kContentDistributionStretch:
      return kContentPositionStart;
    case kContentDistributionDefault:
      return kContentPositionNormal;
  }

  NOTREACHED();
  return kContentPositionNormal;
}

static ContentAlignmentData ContentDistributionOffset(
    const LayoutUnit& available_free_space,
    ContentPosition& fallback_position,
    ContentDistributionType distribution,
    unsigned number_of_grid_tracks) {
  if (distribution != kContentDistributionDefault &&
      fallback_position == kContentPositionNormal)
    fallback_position = ResolveContentDistributionFallback(distribution);

  if (available_free_space <= 0)
    return {};

  LayoutUnit distribution_offset;
  switch (distribution) {
    case kContentDistributionSpaceBetween:
      if (number_of_grid_tracks < 2)
        return {};
      return {LayoutUnit(), available_free_space / (number_of_grid_tracks - 1)};
    case kContentDistributionSpaceAround:
      if (number_of_grid_tracks < 1)
        return {};
      distribution_offset = available_free_space / number_of_grid_tracks;
      return {distribution_offset / 2, distribution_offset};
    case kContentDistributionSpaceEvenly:
      distribution_offset = available_free_space / (number_of_grid_tracks + 1);
      return {distribution_offset, distribution_offset};
    case kContentDistributionStretch:
    case kContentDistributionDefault:
      return {};
  }

  NOTREACHED();
  return {};
}

ContentAlignmentData LayoutGrid::ComputeContentPositionAndDistributionOffset(
    GridTrackSizingDirection direction,
    const LayoutUnit& available_free_space,
    unsigned number_of_grid_tracks) const {
  bool is_row_axis = direction == kForColumns;
  ContentPosition position = is_row_axis
                                 ? StyleRef().ResolvedJustifyContentPosition(
                                       ContentAlignmentNormalBehavior())
                                 : StyleRef().ResolvedAlignContentPosition(
                                       ContentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      is_row_axis ? StyleRef().ResolvedJustifyContentDistribution(
                        ContentAlignmentNormalBehavior())
                  : StyleRef().ResolvedAlignContentDistribution(
                        ContentAlignmentNormalBehavior());
  // If <content-distribution> value can't be applied, 'position' will become
  // the associated <content-position> fallback value.
  ContentAlignmentData content_alignment = ContentDistributionOffset(
      available_free_space, position, distribution, number_of_grid_tracks);
  if (content_alignment.IsValid())
    return content_alignment;

  OverflowAlignment overflow =
      is_row_axis ? StyleRef().JustifyContentOverflowAlignment()
                  : StyleRef().AlignContentOverflowAlignment();
  // TODO (lajava): Default value for overflow isn't exaclty as 'unsafe'.
  // https://drafts.csswg.org/css-align/#overflow-values
  if (available_free_space == 0 ||
      (available_free_space < 0 && overflow == kOverflowAlignmentSafe))
    return {LayoutUnit(), LayoutUnit()};

  switch (position) {
    case kContentPositionLeft:
      // The align-content's axis is always orthogonal to the inline-axis.
      return {LayoutUnit(), LayoutUnit()};
    case kContentPositionRight:
      if (is_row_axis)
        return {available_free_space, LayoutUnit()};
      // The align-content's axis is always orthogonal to the inline-axis.
      return {LayoutUnit(), LayoutUnit()};
    case kContentPositionCenter:
      return {available_free_space / 2, LayoutUnit()};
    // Only used in flex layout, for other layout, it's equivalent to 'End'.
    case kContentPositionFlexEnd:
    case kContentPositionEnd:
      if (is_row_axis)
        return {StyleRef().IsLeftToRightDirection() ? available_free_space
                                                    : LayoutUnit(),
                LayoutUnit()};
      return {available_free_space, LayoutUnit()};
    // Only used in flex layout, for other layout, it's equivalent to 'Start'.
    case kContentPositionFlexStart:
    case kContentPositionStart:
      if (is_row_axis)
        return {StyleRef().IsLeftToRightDirection() ? LayoutUnit()
                                                    : available_free_space,
                LayoutUnit()};
      return {LayoutUnit(), LayoutUnit()};
    case kContentPositionBaseline:
    case kContentPositionLastBaseline:
      // FIXME: These two require implementing Baseline Alignment. For now, we
      // always 'start' align the child. crbug.com/234191
      if (is_row_axis)
        return {StyleRef().IsLeftToRightDirection() ? LayoutUnit()
                                                    : available_free_space,
                LayoutUnit()};
      return {LayoutUnit(), LayoutUnit()};
    case kContentPositionNormal:
      break;
  }

  NOTREACHED();
  return {LayoutUnit(), LayoutUnit()};
}

LayoutUnit LayoutGrid::TranslateRTLCoordinate(LayoutUnit coordinate) const {
  DCHECK(!StyleRef().IsLeftToRightDirection());

  LayoutUnit alignment_offset = column_positions_[0];
  LayoutUnit right_grid_edge_position =
      column_positions_[column_positions_.size() - 1];
  return right_grid_edge_position + alignment_offset - coordinate;
}

LayoutPoint LayoutGrid::FindChildLogicalPosition(const LayoutBox& child) const {
  LayoutUnit column_axis_offset = ColumnAxisOffsetForChild(child);
  LayoutUnit row_axis_offset = RowAxisOffsetForChild(child);
  // We stored m_columnPosition's data ignoring the direction, hence we might
  // need now to translate positions from RTL to LTR, as it's more convenient
  // for painting.
  if (!Style()->IsLeftToRightDirection())
    row_axis_offset = TranslateRTLCoordinate(row_axis_offset) -
                      (IsOrthogonalChild(child) ? child.LogicalHeight()
                                                : child.LogicalWidth());

  // "In the positioning phase [...] calculations are performed according to the
  // writing mode of the containing block of the box establishing the orthogonal
  // flow." However, the resulting LayoutPoint will be used in
  // 'setLogicalPosition' in order to set the child's logical position, which
  // will only take into account the child's writing-mode.
  LayoutPoint child_location(row_axis_offset, column_axis_offset);
  return IsOrthogonalChild(child) ? child_location.TransposedPoint()
                                  : child_location;
}

LayoutPoint LayoutGrid::GridAreaLogicalPosition(const GridArea& area) const {
  LayoutUnit column_axis_offset = row_positions_[area.rows.StartLine()];
  LayoutUnit row_axis_offset = column_positions_[area.columns.StartLine()];

  // See comment in findChildLogicalPosition() about why we need sometimes to
  // translate from RTL to LTR the rowAxisOffset coordinate.
  return LayoutPoint(Style()->IsLeftToRightDirection()
                         ? row_axis_offset
                         : TranslateRTLCoordinate(row_axis_offset),
                     column_axis_offset);
}

void LayoutGrid::PaintChildren(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) const {
  DCHECK(!grid_.NeedsItemsPlacement());
  if (grid_.HasGridItems())
    GridPainter(*this).PaintChildren(paint_info, paint_offset);
}

bool LayoutGrid::CachedHasDefiniteLogicalHeight() const {
  SECURITY_DCHECK(has_definite_logical_height_);
  return has_definite_logical_height_.value();
}

size_t LayoutGrid::NumTracks(GridTrackSizingDirection direction,
                             const Grid& grid) const {
  // Due to limitations in our internal representation, we cannot know the
  // number of columns from m_grid *if* there is no row (because m_grid would be
  // empty). That's why in that case we need to get it from the style. Note that
  // we know for sure that there are't any implicit tracks, because not having
  // rows implies that there are no "normal" children (out-of-flow children are
  // not stored in m_grid).
  DCHECK(!grid.NeedsItemsPlacement());
  if (direction == kForRows)
    return grid.NumTracks(kForRows);

  return grid.NumTracks(kForRows)
             ? grid.NumTracks(kForColumns)
             : GridPositionsResolver::ExplicitGridColumnCount(
                   StyleRef(), grid.AutoRepeatTracks(kForColumns));
}

}  // namespace blink
