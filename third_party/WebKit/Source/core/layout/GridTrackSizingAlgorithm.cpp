// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_track_sizing_algorithm.h"

#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/layout/grid.h"
#include "third_party/blink/renderer/core/layout/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/platform/length_functions.h"

namespace blink {

class GridSizingData;

LayoutUnit GridTrack::BaseSize() const {
  DCHECK(IsGrowthLimitBiggerThanBaseSize());
  return base_size_;
}

LayoutUnit GridTrack::GrowthLimit() const {
  DCHECK(IsGrowthLimitBiggerThanBaseSize());
  DCHECK(!growth_limit_cap_ || growth_limit_cap_.value() >= growth_limit_ ||
         base_size_ >= growth_limit_cap_.value());
  return growth_limit_;
}

void GridTrack::SetBaseSize(LayoutUnit base_size) {
  base_size_ = base_size;
  EnsureGrowthLimitIsBiggerThanBaseSize();
}

void GridTrack::SetGrowthLimit(LayoutUnit growth_limit) {
  growth_limit_ =
      growth_limit == kInfinity
          ? growth_limit
          : std::min(growth_limit, growth_limit_cap_.value_or(growth_limit));
  EnsureGrowthLimitIsBiggerThanBaseSize();
}

bool GridTrack::InfiniteGrowthPotential() const {
  return GrowthLimitIsInfinite() || infinitely_growable_;
}

void GridTrack::SetPlannedSize(LayoutUnit planned_size) {
  DCHECK(planned_size >= 0 || planned_size == kInfinity);
  planned_size_ = planned_size;
}

void GridTrack::SetSizeDuringDistribution(LayoutUnit size_during_distribution) {
  DCHECK_GE(size_during_distribution, 0);
  DCHECK(GrowthLimitIsInfinite() || GrowthLimit() >= size_during_distribution);
  size_during_distribution_ = size_during_distribution;
}

void GridTrack::GrowSizeDuringDistribution(
    LayoutUnit size_during_distribution) {
  DCHECK_GE(size_during_distribution, 0);
  size_during_distribution_ += size_during_distribution;
}

void GridTrack::SetInfinitelyGrowable(bool infinitely_growable) {
  infinitely_growable_ = infinitely_growable;
}

void GridTrack::SetGrowthLimitCap(Optional<LayoutUnit> growth_limit_cap) {
  DCHECK(!growth_limit_cap || *growth_limit_cap >= 0);
  growth_limit_cap_ = growth_limit_cap;
}

bool GridTrack::IsGrowthLimitBiggerThanBaseSize() const {
  return GrowthLimitIsInfinite() || growth_limit_ >= base_size_;
}

void GridTrack::EnsureGrowthLimitIsBiggerThanBaseSize() {
  if (growth_limit_ != kInfinity && growth_limit_ < base_size_)
    growth_limit_ = base_size_;
}

class IndefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
 public:
  IndefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
      : GridTrackSizingAlgorithmStrategy(algorithm) {}

 private:
  void LayoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool override_size_has_changed) const override;
  void MaximizeTracks(Vector<GridTrack>&,
                      Optional<LayoutUnit>& free_space) override;
  double FindUsedFlexFraction(Vector<size_t>& flexible_sized_tracks_index,
                              GridTrackSizingDirection,
                              Optional<LayoutUnit> free_space) const override;
  bool RecomputeUsedFlexFractionIfNeeded(
      Vector<size_t>& flexible_sized_tracks_index,
      double& flex_fraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& total_growth) const override;
  LayoutUnit FreeSpaceForStretchAutoTracksStep() const override;
};

class DefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
 public:
  DefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
      : GridTrackSizingAlgorithmStrategy(algorithm) {}

 private:
  void LayoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool override_size_has_changed) const override;
  void MaximizeTracks(Vector<GridTrack>&,
                      Optional<LayoutUnit>& free_space) override;
  double FindUsedFlexFraction(Vector<size_t>& flexible_sized_tracks_index,
                              GridTrackSizingDirection,
                              Optional<LayoutUnit> free_space) const override;
  bool RecomputeUsedFlexFractionIfNeeded(
      Vector<size_t>& flexible_sized_tracks_index,
      double& flex_fraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& total_growth) const override {
    return false;
  }
  LayoutUnit FreeSpaceForStretchAutoTracksStep() const override;
};

void GridTrackSizingAlgorithmStrategy::SetNeedsLayoutForChild(
    LayoutBox& child) const {
  if (algorithm_.is_in_perform_layout_) {
    child.SetNeedsLayout(LayoutInvalidationReason::kGridChanged, kMarkOnlyThis);
  }
}

GridTrackSizingAlgorithmStrategy::~GridTrackSizingAlgorithmStrategy() = default;

bool GridTrackSizingAlgorithmStrategy::
    ShouldClearOverrideContainingBlockContentSizeForChild(
        const LayoutGrid& grid,
        const LayoutBox& child,
        GridTrackSizingDirection direction) {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(grid, child, kForColumns);
  if (direction == child_inline_direction) {
    return child.HasRelativeLogicalWidth() ||
           child.StyleRef().LogicalWidth().IsIntrinsicOrAuto();
  }
  return child.HasRelativeLogicalHeight() ||
         child.StyleRef().LogicalHeight().IsIntrinsicOrAuto();
}

void GridTrackSizingAlgorithmStrategy::
    SetOverrideContainingBlockContentSizeForChild(
        LayoutBox& child,
        GridTrackSizingDirection direction,
        LayoutUnit size) {
  if (direction == kForColumns)
    child.SetOverrideContainingBlockContentLogicalWidth(size);
  else
    child.SetOverrideContainingBlockContentLogicalHeight(size);
}

LayoutUnit GridTrackSizingAlgorithm::AssumedRowsSizeForOrthogonalChild(
    const LayoutBox& child) const {
  DCHECK(GridLayoutUtils::IsOrthogonalChild(*layout_grid_, child));
  const GridSpan& span = grid_.GridItemSpan(child, kForRows);
  LayoutUnit grid_area_size;
  bool grid_area_is_indefinite = false;
  LayoutUnit containing_block_available_size =
      layout_grid_->ContainingBlockLogicalHeightForContent(
          kExcludeMarginBorderPadding);
  for (auto track_position : span) {
    GridLength max_track_size =
        GetGridTrackSize(kForRows, track_position).MaxTrackBreadth();
    if (max_track_size.IsContentSized() || max_track_size.IsFlex()) {
      grid_area_is_indefinite = true;
    } else {
      grid_area_size += ValueForLength(max_track_size.length(),
                                       containing_block_available_size);
    }
  }

  grid_area_size +=
      layout_grid_->GuttersSize(grid_, kForRows, span.StartLine(),
                                span.IntegerSpan(), AvailableSpace(kForRows));

  return grid_area_is_indefinite
             ? std::max(child.MaxPreferredLogicalWidth(), grid_area_size)
             : grid_area_size;
}

LayoutUnit GridTrackSizingAlgorithm::GridAreaBreadthForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  if (direction == kForRows && sizing_state_ == kColumnSizingFirstIteration)
    return AssumedRowsSizeForOrthogonalChild(child);

  Vector<GridTrack>& all_tracks = Tracks(direction);
  const GridSpan& span = grid_.GridItemSpan(child, direction);
  LayoutUnit grid_area_breadth;
  for (const auto& track_position : span)
    grid_area_breadth += all_tracks[track_position].BaseSize();

  grid_area_breadth +=
      layout_grid_->GuttersSize(grid_, direction, span.StartLine(),
                                span.IntegerSpan(), AvailableSpace(direction));

  return grid_area_breadth;
}

bool GridTrackSizingAlgorithmStrategy::
    UpdateOverrideContainingBlockContentSizeForChild(
        LayoutBox& child,
        GridTrackSizingDirection direction,
        Optional<LayoutUnit> override_size) const {
  if (!override_size)
    override_size = algorithm_.GridAreaBreadthForChild(child, direction);
  if (GridLayoutUtils::HasOverrideContainingBlockContentSizeForChild(
          child, direction) &&
      GridLayoutUtils::OverrideContainingBlockContentSizeForChild(
          child, direction) == override_size.value())
    return false;

  SetOverrideContainingBlockContentSizeForChild(child, direction,
                                                override_size.value());
  return true;
}

Optional<LayoutUnit>
GridTrackSizingAlgorithmStrategy::ExtentForBaselineAlignment(
    const LayoutBox& child) const {
  const LayoutGrid& layout_grid = *GetLayoutGrid();
  GridAxis baseline_axis =
      GridLayoutUtils::IsOrthogonalChild(*GetLayoutGrid(), child)
          ? kGridRowAxis
          : kGridColumnAxis;
  if (!layout_grid.IsBaselineAlignmentForChild(child, baseline_axis))
    return WTF::nullopt;

  ItemPosition align =
      layout_grid.SelfAlignmentForChild(baseline_axis, child).GetPosition();
  const auto& span =
      baseline_axis == kGridColumnAxis
          ? algorithm_.GetGrid().GridItemSpan(child, kForRows)
          : algorithm_.GetGrid().GridItemSpan(child, kForColumns);

  return algorithm_.baseline_alignment_.ExtentForBaselineAlignment(
      align, span.StartLine(), child, baseline_axis);
};

LayoutUnit GridTrackSizingAlgorithmStrategy::LogicalHeightForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_block_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForRows);
  // If |child| has a relative block-axis size, we shouldn't let it override its
  // intrinsic size, which is what we are interested in here. Thus we
  // need to set the block-axis OverrideContainingBlock size to -1 (no possible
  // resolution).
  if (ShouldClearOverrideContainingBlockContentSizeForChild(
          *GetLayoutGrid(), child, child_block_direction)) {
    SetOverrideContainingBlockContentSizeForChild(child, child_block_direction,
                                                  LayoutUnit(-1));
    SetNeedsLayoutForChild(child);
  }

  child.LayoutIfNeeded();

  if (auto baseline_extent = ExtentForBaselineAlignment(child))
    return baseline_extent.value();

  return child.LogicalHeight() +
         GridLayoutUtils::MarginLogicalHeightForChild(*GetLayoutGrid(), child);
}

DISABLE_CFI_PERF
LayoutUnit GridTrackSizingAlgorithmStrategy::MinContentForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction) {
    // FIXME: It's unclear if we should return the intrinsic width or the
    // preferred width.
    // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
    return child.MinPreferredLogicalWidth() +
           GridLayoutUtils::MarginLogicalWidthForChild(*GetLayoutGrid(), child);
  }

  if (Direction() == kForColumns && !AvailableSpace()) {
    DCHECK(GridLayoutUtils::IsOrthogonalChild(*GetLayoutGrid(), child));
    if (auto baseline_extent = ExtentForBaselineAlignment(child))
      return baseline_extent.value();
  }

  if (UpdateOverrideContainingBlockContentSizeForChild(
          child, child_inline_direction)) {
    SetNeedsLayoutForChild(child);
  }
  return LogicalHeightForChild(child);
}

DISABLE_CFI_PERF
LayoutUnit GridTrackSizingAlgorithmStrategy::MaxContentForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction) {
    // FIXME: It's unclear if we should return the intrinsic width or the
    // preferred width.
    // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
    return child.MaxPreferredLogicalWidth() +
           GridLayoutUtils::MarginLogicalWidthForChild(*GetLayoutGrid(), child);
  }

  if (UpdateOverrideContainingBlockContentSizeForChild(
          child, child_inline_direction)) {
    SetNeedsLayoutForChild(child);
  }
  return LogicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::MinSizeForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  bool is_row_axis = Direction() == child_inline_direction;
  const Length& child_size = is_row_axis ? child.StyleRef().LogicalWidth()
                                         : child.StyleRef().LogicalHeight();
  const Length& child_min_size = is_row_axis
                                     ? child.StyleRef().LogicalMinWidth()
                                     : child.StyleRef().LogicalMinHeight();
  bool overflow_is_visible =
      is_row_axis
          ? child.StyleRef().OverflowInlineDirection() == EOverflow::kVisible
          : child.StyleRef().OverflowBlockDirection() == EOverflow::kVisible;
  if (child_size.IsAuto() && child_min_size.IsAuto() && overflow_is_visible) {
    LayoutUnit min_size = MinContentForChild(child);
    const GridSpan& span =
        algorithm_.GetGrid().GridItemSpan(child, Direction());
    LayoutUnit max_breadth;
    for (const auto& track_position : span) {
      GridTrackSize track_size =
          algorithm_.GetGridTrackSize(Direction(), track_position);
      if (!track_size.HasFixedMaxTrackBreadth())
        return min_size;
      max_breadth += ValueForLength(track_size.MaxTrackBreadth().length(),
                                    AvailableSpace().value_or(LayoutUnit()));
    }
    if (min_size > max_breadth) {
      LayoutUnit margin_and_border_and_padding =
          is_row_axis ? GridLayoutUtils::MarginLogicalWidthForChild(
                            *GetLayoutGrid(), child) +
                            child.BorderAndPaddingLogicalWidth()
                      : GridLayoutUtils::MarginLogicalHeightForChild(
                            *GetLayoutGrid(), child) +
                            child.BorderAndPaddingLogicalHeight();
      min_size = std::max(max_breadth, margin_and_border_and_padding);
    }
    return min_size;
  }

  if (!child_size.IsAuto())
    return MinContentForChild(child);

  LayoutUnit grid_area_size =
      algorithm_.GridAreaBreadthForChild(child, child_inline_direction);

  if (is_row_axis)
    return MinLogicalWidthForChild(child, child_min_size, grid_area_size);

  bool override_size_has_changed =
      UpdateOverrideContainingBlockContentSizeForChild(
          child, child_inline_direction, grid_area_size);
  LayoutGridItemForMinSizeComputation(child, override_size_has_changed);

  return child.ComputeLogicalHeightUsing(kMinSize, child_min_size,
                                         child.IntrinsicLogicalHeight()) +
         GridLayoutUtils::MarginLogicalHeightForChild(*GetLayoutGrid(), child) +
         child.ScrollbarLogicalHeight();
}

void GridTrackSizingAlgorithm::UpdateBaselineAlignmentContextIfNeeded(
    LayoutBox& child,
    GridAxis baseline_axis) {
  // TODO (lajava): We must ensure this method is not called as part of an
  // intrinsic size computation.
  if (!layout_grid_->IsBaselineAlignmentForChild(child, baseline_axis))
    return;

  child.LayoutIfNeeded();
  ItemPosition align =
      layout_grid_->SelfAlignmentForChild(baseline_axis, child).GetPosition();
  const auto& span = baseline_axis == kGridColumnAxis
                         ? grid_.GridItemSpan(child, kForRows)
                         : grid_.GridItemSpan(child, kForColumns);
  baseline_alignment_.UpdateBaselineAlignmentContextIfNeeded(
      align, span.StartLine(), child, baseline_axis);
}

LayoutUnit GridTrackSizingAlgorithm::BaselineOffsetForChild(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  if (!layout_grid_->IsBaselineAlignmentForChild(child, baseline_axis))
    return LayoutUnit();

  ItemPosition align =
      layout_grid_->SelfAlignmentForChild(baseline_axis, child).GetPosition();
  const auto& span = baseline_axis == kGridColumnAxis
                         ? grid_.GridItemSpan(child, kForRows)
                         : grid_.GridItemSpan(child, kForColumns);

  return baseline_alignment_.BaselineOffsetForChild(align, span.StartLine(),
                                                    child, baseline_axis);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::ComputeTrackBasedSize() const {
  return algorithm_.ComputeTrackBasedSize();
}

double GridTrackSizingAlgorithmStrategy::FindFrUnitSize(
    const GridSpan& tracks_span,
    LayoutUnit left_over_space) const {
  return algorithm_.FindFrUnitSize(tracks_span, left_over_space);
}

void GridTrackSizingAlgorithmStrategy::DistributeSpaceToTracks(
    Vector<GridTrack*>& tracks,
    LayoutUnit& available_logical_space) const {
  algorithm_.DistributeSpaceToTracks<kMaximizeTracks>(tracks, nullptr,
                                                      available_logical_space);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::MinLogicalWidthForChild(
    LayoutBox& child,
    Length child_min_size,
    LayoutUnit available_size) const {
  return child.ComputeLogicalWidthUsing(kMinSize, child_min_size,
                                        available_size, GetLayoutGrid()) +
         GridLayoutUtils::MarginLogicalWidthForChild(*GetLayoutGrid(), child);
}

void DefiniteSizeStrategy::LayoutGridItemForMinSizeComputation(
    LayoutBox& child,
    bool override_size_has_changed) const {
  if (override_size_has_changed)
    SetNeedsLayoutForChild(child);
  child.LayoutIfNeeded();
}

void DefiniteSizeStrategy::MaximizeTracks(Vector<GridTrack>& tracks,
                                          Optional<LayoutUnit>& free_space) {
  size_t tracks_size = tracks.size();
  Vector<GridTrack*> tracks_for_distribution(tracks_size);
  for (size_t i = 0; i < tracks_size; ++i) {
    tracks_for_distribution[i] = tracks.data() + i;
    tracks_for_distribution[i]->SetPlannedSize(
        tracks_for_distribution[i]->BaseSize());
  }

  DCHECK(free_space);
  DistributeSpaceToTracks(tracks_for_distribution, free_space.value());

  for (auto* track : tracks_for_distribution)
    track->SetBaseSize(track->PlannedSize());
}

double DefiniteSizeStrategy::FindUsedFlexFraction(
    Vector<size_t>& flexible_sized_tracks_index,
    GridTrackSizingDirection direction,
    Optional<LayoutUnit> free_space) const {
  GridSpan all_tracks_span = GridSpan::TranslatedDefiniteGridSpan(
      0, algorithm_.Tracks(direction).size());
  DCHECK(free_space);
  return FindFrUnitSize(all_tracks_span, free_space.value());
}

LayoutUnit DefiniteSizeStrategy::FreeSpaceForStretchAutoTracksStep() const {
  DCHECK(algorithm_.FreeSpace(Direction()));
  return algorithm_.FreeSpace(Direction()).value();
}

void IndefiniteSizeStrategy::LayoutGridItemForMinSizeComputation(
    LayoutBox& child,
    bool override_size_has_changed) const {
  if (override_size_has_changed && Direction() != kForColumns)
    SetNeedsLayoutForChild(child);
  child.LayoutIfNeeded();
}

void IndefiniteSizeStrategy::MaximizeTracks(Vector<GridTrack>& tracks,
                                            Optional<LayoutUnit>&) {
  for (auto& track : tracks)
    track.SetBaseSize(track.GrowthLimit());
}

static inline double NormalizedFlexFraction(const GridTrack& track,
                                            double flex_factor) {
  return track.BaseSize() / std::max<double>(1, flex_factor);
}

double IndefiniteSizeStrategy::FindUsedFlexFraction(
    Vector<size_t>& flexible_sized_tracks_index,
    GridTrackSizingDirection direction,
    Optional<LayoutUnit>) const {
  auto all_tracks = algorithm_.Tracks(direction);

  double flex_fraction = 0;
  for (const auto& track_index : flexible_sized_tracks_index) {
    // TODO(svillar): we pass TrackSizing to gridTrackSize() because it does not
    // really matter as we know the track is a flex sized track. It'd be nice
    // not to have to do that.
    flex_fraction = std::max(
        flex_fraction, NormalizedFlexFraction(
                           all_tracks[track_index],
                           algorithm_.GetGridTrackSize(direction, track_index)
                               .MaxTrackBreadth()
                               .Flex()));
  }

  const Grid& grid = algorithm_.GetGrid();
  if (!grid.HasGridItems())
    return flex_fraction;

  for (size_t i = 0; i < flexible_sized_tracks_index.size(); ++i) {
    GridIterator iterator(grid, direction, flexible_sized_tracks_index[i]);
    while (LayoutBox* grid_item = iterator.NextGridItem()) {
      const GridSpan& span = grid.GridItemSpan(*grid_item, direction);

      // Do not include already processed items.
      if (i > 0 && span.StartLine() <= flexible_sized_tracks_index[i - 1])
        continue;

      // Removing gutters from the max-content contribution of the item,
      // so they are not taken into account in FindFrUnitSize().
      LayoutUnit left_over_space =
          MaxContentForChild(*grid_item) -
          GetLayoutGrid()->GuttersSize(algorithm_.GetGrid(), direction,
                                       span.StartLine(), span.IntegerSpan(),
                                       AvailableSpace());
      flex_fraction =
          std::max(flex_fraction, FindFrUnitSize(span, left_over_space));
    }
  }

  return flex_fraction;
}

bool IndefiniteSizeStrategy::RecomputeUsedFlexFractionIfNeeded(
    Vector<size_t>& flexible_sized_tracks_index,
    double& flex_fraction,
    Vector<LayoutUnit>& increments,
    LayoutUnit& total_growth) const {
  if (Direction() == kForColumns)
    return false;

  const LayoutGrid* layout_grid = GetLayoutGrid();
  LayoutUnit min_size = layout_grid->ComputeContentLogicalHeight(
      kMinSize, layout_grid->StyleRef().LogicalMinHeight(), LayoutUnit(-1));
  LayoutUnit max_size = layout_grid->ComputeContentLogicalHeight(
      kMaxSize, layout_grid->StyleRef().LogicalMaxHeight(), LayoutUnit(-1));

  // Redo the flex fraction computation using min|max-height as definite
  // available space in case the total height is smaller than min-height or
  // larger than max-height.
  LayoutUnit rows_size = total_growth + ComputeTrackBasedSize();
  bool check_min_size = min_size && rows_size < min_size;
  bool check_max_size = max_size != -1 && rows_size > max_size;
  if (!check_min_size && !check_max_size)
    return false;

  LayoutUnit free_space = check_max_size ? max_size : LayoutUnit(-1);
  const Grid& grid = algorithm_.GetGrid();
  free_space =
      std::max(free_space, min_size) -
      layout_grid->GuttersSize(grid, kForRows, 0, grid.NumTracks(kForRows),
                               AvailableSpace());

  size_t number_of_tracks = algorithm_.Tracks(Direction()).size();
  flex_fraction = FindFrUnitSize(
      GridSpan::TranslatedDefiniteGridSpan(0, number_of_tracks), free_space);
  return true;
}

LayoutUnit IndefiniteSizeStrategy::FreeSpaceForStretchAutoTracksStep() const {
  DCHECK(!algorithm_.FreeSpace(Direction()));
  if (Direction() == kForColumns)
    return LayoutUnit();

  LayoutUnit min_size = GetLayoutGrid()->ComputeContentLogicalHeight(
      kMinSize, GetLayoutGrid()->StyleRef().LogicalMinHeight(), LayoutUnit(-1));
  return min_size - ComputeTrackBasedSize();
}

Optional<LayoutUnit> GridTrackSizingAlgorithm::FreeSpace(
    GridTrackSizingDirection direction) const {
  return direction == kForRows ? free_space_rows_ : free_space_columns_;
}

Optional<LayoutUnit> GridTrackSizingAlgorithm::AvailableSpace(
    GridTrackSizingDirection direction) const {
  return direction == kForRows ? available_space_rows_
                               : available_space_columns_;
}

Optional<LayoutUnit> GridTrackSizingAlgorithm::AvailableSpace() const {
  return AvailableSpace(direction_);
}

void GridTrackSizingAlgorithm::SetAvailableSpace(
    GridTrackSizingDirection direction,
    Optional<LayoutUnit> available_space) {
  if (direction == kForColumns)
    available_space_columns_ = available_space;
  else
    available_space_rows_ = available_space;
}

Vector<GridTrack>& GridTrackSizingAlgorithm::Tracks(
    GridTrackSizingDirection direction) {
  return direction == kForColumns ? columns_ : rows_;
}

const Vector<GridTrack>& GridTrackSizingAlgorithm::Tracks(
    GridTrackSizingDirection direction) const {
  return direction == kForColumns ? columns_ : rows_;
}

void GridTrackSizingAlgorithm::SetFreeSpace(GridTrackSizingDirection direction,
                                            Optional<LayoutUnit> free_space) {
  if (direction == kForColumns)
    free_space_columns_ = free_space;
  else
    free_space_rows_ = free_space;
}

GridTrackSize GridTrackSizingAlgorithm::RawGridTrackSize(
    GridTrackSizingDirection direction,
    size_t translated_index) const {
  bool is_row_axis = direction == kForColumns;
  const Vector<GridTrackSize>& track_styles =
      is_row_axis ? layout_grid_->StyleRef().GridTemplateColumns()
                  : layout_grid_->StyleRef().GridTemplateRows();
  const Vector<GridTrackSize>& auto_repeat_track_styles =
      is_row_axis ? layout_grid_->StyleRef().GridAutoRepeatColumns()
                  : layout_grid_->StyleRef().GridAutoRepeatRows();
  const Vector<GridTrackSize>& auto_track_styles =
      is_row_axis ? layout_grid_->StyleRef().GridAutoColumns()
                  : layout_grid_->StyleRef().GridAutoRows();
  size_t insertion_point =
      is_row_axis
          ? layout_grid_->StyleRef().GridAutoRepeatColumnsInsertionPoint()
          : layout_grid_->StyleRef().GridAutoRepeatRowsInsertionPoint();
  size_t auto_repeat_tracks_count = grid_.AutoRepeatTracks(direction);

  // We should not use GridPositionsResolver::explicitGridXXXCount() for this
  // because the explicit grid might be larger than the number of tracks in
  // grid-template-rows|columns (if grid-template-areas is specified for
  // example).
  size_t explicit_tracks_count = track_styles.size() + auto_repeat_tracks_count;

  int untranslated_index_as_int =
      translated_index + grid_.SmallestTrackStart(direction);
  size_t auto_track_styles_size = auto_track_styles.size();
  if (untranslated_index_as_int < 0) {
    int index =
        untranslated_index_as_int % static_cast<int>(auto_track_styles_size);
    // We need to traspose the index because the first negative implicit line
    // will get the last defined auto track and so on.
    index += index ? auto_track_styles_size : 0;
    return auto_track_styles[index];
  }

  size_t untranslated_index = static_cast<size_t>(untranslated_index_as_int);
  if (untranslated_index >= explicit_tracks_count) {
    return auto_track_styles[(untranslated_index - explicit_tracks_count) %
                             auto_track_styles_size];
  }

  if (LIKELY(!auto_repeat_tracks_count) || untranslated_index < insertion_point)
    return track_styles[untranslated_index];

  if (untranslated_index < (insertion_point + auto_repeat_tracks_count)) {
    size_t auto_repeat_local_index =
        untranslated_index_as_int - insertion_point;
    return auto_repeat_track_styles[auto_repeat_local_index %
                                    auto_repeat_track_styles.size()];
  }

  return track_styles[untranslated_index - auto_repeat_tracks_count];
}

GridTrackSize GridTrackSizingAlgorithm::GetGridTrackSize(
    GridTrackSizingDirection direction,
    size_t translated_index) const {
  // Collapse empty auto repeat tracks if auto-fit.
  if (grid_.HasAutoRepeatEmptyTracks(direction) &&
      grid_.IsEmptyAutoRepeatTrack(direction, translated_index))
    return {Length(kFixed), kLengthTrackSizing};

  const GridTrackSize& track_size =
      RawGridTrackSize(direction, translated_index);
  if (track_size.IsFitContent())
    return track_size;

  GridLength min_track_breadth = track_size.MinTrackBreadth();
  GridLength max_track_breadth = track_size.MaxTrackBreadth();
  // If the logical width/height of the grid container is indefinite, percentage
  // values are treated as <auto>.
  if (min_track_breadth.HasPercentage() || max_track_breadth.HasPercentage()) {
    // TODO(svillar): we should remove the second check later. We need it
    // because during the second iteration of the algorithm we set definite
    // sizes in the grid container so percents would not resolve properly (it
    // would think that the height is definite when it is not).
    if (!AvailableSpace(direction) ||
        (direction == kForRows &&
         !layout_grid_->CachedHasDefiniteLogicalHeight())) {
      UseCounter::Count(layout_grid_->GetDocument(),
                        WebFeature::kGridRowTrackPercentIndefiniteHeight);
      if (min_track_breadth.HasPercentage())
        min_track_breadth = Length(kAuto);
      if (max_track_breadth.HasPercentage())
        max_track_breadth = Length(kAuto);
    }
  }

  // Flex sizes are invalid as a min sizing function. However we still can have
  // a flexible |minTrackBreadth| if the track had a flex size directly (e.g.
  // "1fr"), the spec says that in this case it implies an automatic minimum.
  if (min_track_breadth.IsFlex())
    min_track_breadth = Length(kAuto);

  return GridTrackSize(min_track_breadth, max_track_breadth);
}

LayoutUnit GridTrackSizingAlgorithm::InitialBaseSize(
    const GridTrackSize& track_size) const {
  const GridLength& grid_length = track_size.MinTrackBreadth();
  if (grid_length.IsFlex())
    return LayoutUnit();

  const Length& track_length = grid_length.length();
  if (track_length.IsSpecified()) {
    return ValueForLength(track_length,
                          AvailableSpace().value_or(LayoutUnit()));
  }

  DCHECK(track_length.IsMinContent() || track_length.IsAuto() ||
         track_length.IsMaxContent());
  return LayoutUnit();
}

LayoutUnit GridTrackSizingAlgorithm::InitialGrowthLimit(
    const GridTrackSize& track_size,
    LayoutUnit base_size) const {
  const GridLength& grid_length = track_size.MaxTrackBreadth();
  if (grid_length.IsFlex())
    return base_size;

  const Length& track_length = grid_length.length();
  if (track_length.IsSpecified()) {
    return ValueForLength(track_length,
                          AvailableSpace().value_or(LayoutUnit()));
  }

  DCHECK(track_length.IsMinContent() || track_length.IsAuto() ||
         track_length.IsMaxContent());
  return LayoutUnit(kInfinity);
}

void GridTrackSizingAlgorithm::InitializeTrackSizes() {
  DCHECK(content_sized_tracks_index_.IsEmpty());
  DCHECK(flexible_sized_tracks_index_.IsEmpty());
  DCHECK(auto_sized_tracks_for_stretch_index_.IsEmpty());
  Vector<GridTrack>& track_list = Tracks(direction_);
  bool has_definite_free_space = !!AvailableSpace();
  size_t num_tracks = track_list.size();
  for (size_t i = 0; i < num_tracks; ++i) {
    GridTrackSize track_size = GetGridTrackSize(direction_, i);
    GridTrack& track = track_list[i];
    track.SetBaseSize(InitialBaseSize(track_size));
    track.SetGrowthLimit(InitialGrowthLimit(track_size, track.BaseSize()));
    track.SetInfinitelyGrowable(false);

    if (track_size.IsFitContent()) {
      GridLength grid_length = track_size.FitContentTrackBreadth();
      if (!grid_length.HasPercentage() || has_definite_free_space) {
        track.SetGrowthLimitCap(ValueForLength(
            grid_length.length(), AvailableSpace().value_or(LayoutUnit())));
      }
    }

    if (track_size.IsContentSized())
      content_sized_tracks_index_.push_back(i);
    if (track_size.MaxTrackBreadth().IsFlex())
      flexible_sized_tracks_index_.push_back(i);
    if (track_size.HasAutoMaxTrackBreadth() && !track_size.IsFitContent())
      auto_sized_tracks_for_stretch_index_.push_back(i);
  }
}

void GridTrackSizingAlgorithm::SizeTrackToFitNonSpanningItem(
    const GridSpan& span,
    LayoutBox& grid_item,
    GridTrack& track) {
  const size_t track_position = span.StartLine();
  GridTrackSize track_size = GetGridTrackSize(direction_, track_position);

  if (track_size.HasMinContentMinTrackBreadth()) {
    track.SetBaseSize(
        std::max(track.BaseSize(), strategy_->MinContentForChild(grid_item)));
  } else if (track_size.HasMaxContentMinTrackBreadth()) {
    track.SetBaseSize(
        std::max(track.BaseSize(), strategy_->MaxContentForChild(grid_item)));
  } else if (track_size.HasAutoMinTrackBreadth()) {
    track.SetBaseSize(
        std::max(track.BaseSize(), strategy_->MinSizeForChild(grid_item)));
  }

  if (track_size.HasMinContentMaxTrackBreadth()) {
    track.SetGrowthLimit(std::max(track.GrowthLimit(),
                                  strategy_->MinContentForChild(grid_item)));
  } else if (track_size.HasMaxContentOrAutoMaxTrackBreadth()) {
    LayoutUnit growth_limit = strategy_->MaxContentForChild(grid_item);
    if (track_size.IsFitContent()) {
      growth_limit =
          std::min(growth_limit,
                   ValueForLength(track_size.FitContentTrackBreadth().length(),
                                  AvailableSpace().value_or(LayoutUnit())));
    }
    track.SetGrowthLimit(std::max(track.GrowthLimit(), growth_limit));
  }
}

bool GridTrackSizingAlgorithm::SpanningItemCrossesFlexibleSizedTracks(
    const GridSpan& span) const {
  for (const auto& track_position : span) {
    const GridTrackSize& track_size =
        GetGridTrackSize(direction_, track_position);
    if (track_size.MinTrackBreadth().IsFlex() ||
        track_size.MaxTrackBreadth().IsFlex())
      return true;
  }

  return false;
}

// We're basically using a class instead of a std::pair because of accessing
// gridItem() or getGridSpan() is much more self-explanatory that using .first
// or .second members in the pair. Having a std::pair<LayoutBox*, size_t>
// does not work either because we still need the GridSpan so we'd have to add
// an extra hash lookup for each item.
class GridItemWithSpan {
 public:
  GridItemWithSpan(LayoutBox& grid_item, const GridSpan& grid_span)
      : grid_item_(&grid_item), grid_span_(grid_span) {}

  LayoutBox& GridItem() const { return *grid_item_; }
  GridSpan GetGridSpan() const { return grid_span_; }

  bool operator<(const GridItemWithSpan other) const {
    return grid_span_.IntegerSpan() < other.grid_span_.IntegerSpan();
  }

 private:
  LayoutBox* grid_item_;
  GridSpan grid_span_;
};

struct GridItemsSpanGroupRange {
  Vector<GridItemWithSpan>::iterator range_start;
  Vector<GridItemWithSpan>::iterator range_end;
};

enum TrackSizeRestriction {
  kAllowInfinity,
  kForbidInfinity,
};

static LayoutUnit TrackSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrack& track,
    TrackSizeRestriction restriction) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
    case kResolveMaxContentMinimums:
    case kMaximizeTracks:
      return track.BaseSize();
    case kResolveIntrinsicMaximums:
    case kResolveMaxContentMaximums:
      const LayoutUnit& growth_limit = track.GrowthLimit();
      if (restriction == kAllowInfinity)
        return growth_limit;
      return growth_limit == kInfinity ? track.BaseSize() : growth_limit;
  }

  NOTREACHED();
  return track.BaseSize();
}

static bool ShouldProcessTrackForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrackSize& track_size) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
      return track_size.HasIntrinsicMinTrackBreadth();
    case kResolveContentBasedMinimums:
      return track_size.HasMinOrMaxContentMinTrackBreadth();
    case kResolveMaxContentMinimums:
      return track_size.HasMaxContentMinTrackBreadth();
    case kResolveIntrinsicMaximums:
      return track_size.HasIntrinsicMaxTrackBreadth();
    case kResolveMaxContentMaximums:
      return track_size.HasMaxContentOrAutoMaxTrackBreadth();
    case kMaximizeTracks:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

static bool TrackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrackSize& track_size) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
      return track_size
          .HasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case kResolveMaxContentMinimums:
      return track_size
          .HasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case kResolveIntrinsicMaximums:
    case kResolveMaxContentMaximums:
      return true;
    case kMaximizeTracks:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

static void MarkAsInfinitelyGrowableForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    GridTrack& track) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
    case kResolveMaxContentMinimums:
      return;
    case kResolveIntrinsicMaximums:
      if (TrackSizeForTrackSizeComputationPhase(phase, track, kAllowInfinity) ==
              kInfinity &&
          track.PlannedSize() != kInfinity)
        track.SetInfinitelyGrowable(true);
      return;
    case kResolveMaxContentMaximums:
      if (track.InfinitelyGrowable())
        track.SetInfinitelyGrowable(false);
      return;
    case kMaximizeTracks:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

static void UpdateTrackSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    GridTrack& track) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
    case kResolveMaxContentMinimums:
      track.SetBaseSize(track.PlannedSize());
      return;
    case kResolveIntrinsicMaximums:
    case kResolveMaxContentMaximums:
      track.SetGrowthLimit(track.PlannedSize());
      return;
    case kMaximizeTracks:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

LayoutUnit GridTrackSizingAlgorithm::ItemSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    LayoutBox& grid_item) const {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveIntrinsicMaximums:
      return strategy_->MinSizeForChild(grid_item);
    case kResolveContentBasedMinimums:
      return strategy_->MinContentForChild(grid_item);
    case kResolveMaxContentMinimums:
    case kResolveMaxContentMaximums:
      return strategy_->MaxContentForChild(grid_item);
    case kMaximizeTracks:
      NOTREACHED();
      return LayoutUnit();
  }

  NOTREACHED();
  return LayoutUnit();
}

static bool SortByGridTrackGrowthPotential(const GridTrack* track1,
                                           const GridTrack* track2) {
  // This check ensures that we respect the irreflexivity property of the strict
  // weak ordering required by std::sort(forall x: NOT x < x).
  bool track1_has_infinite_growth_potential_without_cap =
      track1->InfiniteGrowthPotential() && !track1->GrowthLimitCap();
  bool track2_has_infinite_growth_potential_without_cap =
      track2->InfiniteGrowthPotential() && !track2->GrowthLimitCap();

  if (track1_has_infinite_growth_potential_without_cap &&
      track2_has_infinite_growth_potential_without_cap)
    return false;

  if (track1_has_infinite_growth_potential_without_cap ||
      track2_has_infinite_growth_potential_without_cap)
    return track2_has_infinite_growth_potential_without_cap;

  LayoutUnit track1_limit =
      track1->GrowthLimitCap().value_or(track1->GrowthLimit());
  LayoutUnit track2_limit =
      track2->GrowthLimitCap().value_or(track2->GrowthLimit());
  return (track1_limit - track1->BaseSize()) <
         (track2_limit - track2->BaseSize());
}

static void ClampGrowthShareIfNeeded(TrackSizeComputationPhase phase,
                                     const GridTrack& track,
                                     LayoutUnit& growth_share) {
  if (phase != kResolveMaxContentMaximums || !track.GrowthLimitCap())
    return;

  LayoutUnit distance_to_cap =
      track.GrowthLimitCap().value() - track.SizeDuringDistribution();
  if (distance_to_cap <= 0)
    return;

  growth_share = std::min(growth_share, distance_to_cap);
}

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::DistributeSpaceToTracks(
    Vector<GridTrack*>& tracks,
    Vector<GridTrack*>* grow_beyond_growth_limits_tracks,
    LayoutUnit& available_logical_space) const {
  DCHECK_GE(available_logical_space, 0);

  for (auto* track : tracks) {
    track->SetSizeDuringDistribution(
        TrackSizeForTrackSizeComputationPhase(phase, *track, kForbidInfinity));
  }

  if (available_logical_space > 0) {
    std::sort(tracks.begin(), tracks.end(), SortByGridTrackGrowthPotential);

    size_t tracks_size = tracks.size();
    for (size_t i = 0; i < tracks_size; ++i) {
      GridTrack& track = *tracks[i];
      LayoutUnit available_logical_space_share =
          available_logical_space / (tracks_size - i);
      const LayoutUnit& track_breadth =
          TrackSizeForTrackSizeComputationPhase(phase, track, kForbidInfinity);
      LayoutUnit growth_share =
          track.InfiniteGrowthPotential()
              ? available_logical_space_share
              : std::min(available_logical_space_share,
                         track.GrowthLimit() - track_breadth);
      ClampGrowthShareIfNeeded(phase, track, growth_share);
      DCHECK_GE(growth_share, 0) << "We must never shrink any grid track or "
                                    "else we can't guarantee we abide by our "
                                    "min-sizing function.";
      track.GrowSizeDuringDistribution(growth_share);
      available_logical_space -= growth_share;
    }
  }

  if (available_logical_space > 0 && grow_beyond_growth_limits_tracks) {
    // We need to sort them because there might be tracks with growth limit caps
    // (like the ones with fit-content()) which cannot indefinitely grow over
    // the limits.
    if (phase == kResolveMaxContentMaximums) {
      std::sort(grow_beyond_growth_limits_tracks->begin(),
                grow_beyond_growth_limits_tracks->end(),
                SortByGridTrackGrowthPotential);
    }

    size_t tracks_growing_above_max_breadth_size =
        grow_beyond_growth_limits_tracks->size();
    for (size_t i = 0; i < tracks_growing_above_max_breadth_size; ++i) {
      GridTrack* track = grow_beyond_growth_limits_tracks->at(i);
      LayoutUnit growth_share =
          available_logical_space / (tracks_growing_above_max_breadth_size - i);
      ClampGrowthShareIfNeeded(phase, *track, growth_share);
      DCHECK_GE(growth_share, 0) << "We must never shrink any grid track or "
                                    "else we can't guarantee we abide by our "
                                    "min-sizing function.";
      track->GrowSizeDuringDistribution(growth_share);
      available_logical_space -= growth_share;
    }
  }

  for (auto* track : tracks) {
    track->SetPlannedSize(
        track->PlannedSize() == kInfinity
            ? track->SizeDuringDistribution()
            : std::max(track->PlannedSize(), track->SizeDuringDistribution()));
  }
}

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::IncreaseSizesToAccommodateSpanningItems(
    const GridItemsSpanGroupRange& grid_items_with_span) {
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (const auto& track_index : content_sized_tracks_index_) {
    GridTrack& track = all_tracks[track_index];
    track.SetPlannedSize(
        TrackSizeForTrackSizeComputationPhase(phase, track, kAllowInfinity));
  }

  Vector<GridTrack*> grow_beyond_growth_limits_tracks;
  Vector<GridTrack*> filtered_tracks;
  for (auto* it = grid_items_with_span.range_start;
       it != grid_items_with_span.range_end; ++it) {
    GridItemWithSpan& grid_item_with_span = *it;
    DCHECK_GT(grid_item_with_span.GetGridSpan().IntegerSpan(), 1u);
    const GridSpan& item_span = grid_item_with_span.GetGridSpan();

    grow_beyond_growth_limits_tracks.Shrink(0);
    filtered_tracks.Shrink(0);
    LayoutUnit spanning_tracks_size;
    for (const auto& track_position : item_span) {
      GridTrackSize track_size = GetGridTrackSize(direction_, track_position);
      GridTrack& track = Tracks(direction_)[track_position];
      spanning_tracks_size +=
          TrackSizeForTrackSizeComputationPhase(phase, track, kForbidInfinity);
      if (!ShouldProcessTrackForTrackSizeComputationPhase(phase, track_size))
        continue;

      filtered_tracks.push_back(&track);

      if (TrackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(
              phase, track_size))
        grow_beyond_growth_limits_tracks.push_back(&track);
    }

    if (filtered_tracks.IsEmpty())
      continue;

    spanning_tracks_size +=
        layout_grid_->GuttersSize(grid_, direction_, item_span.StartLine(),
                                  item_span.IntegerSpan(), AvailableSpace());

    LayoutUnit extra_space = ItemSizeForTrackSizeComputationPhase(
                                 phase, grid_item_with_span.GridItem()) -
                             spanning_tracks_size;
    extra_space = extra_space.ClampNegativeToZero();
    auto& tracks_to_grow_beyond_growth_limits =
        grow_beyond_growth_limits_tracks.IsEmpty()
            ? filtered_tracks
            : grow_beyond_growth_limits_tracks;
    DistributeSpaceToTracks<phase>(
        filtered_tracks, &tracks_to_grow_beyond_growth_limits, extra_space);
  }

  for (const auto& track_index : content_sized_tracks_index_) {
    GridTrack& track = all_tracks[track_index];
    MarkAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
    UpdateTrackSizeForTrackSizeComputationPhase(phase, track);
  }
}

void GridTrackSizingAlgorithm::ResolveIntrinsicTrackSizes() {
  Vector<GridItemWithSpan> items_sorted_by_increasing_span;
  if (grid_.HasGridItems()) {
    HashSet<LayoutBox*> items_set;
    for (const auto& track_index : content_sized_tracks_index_) {
      GridIterator iterator(grid_, direction_, track_index);
      GridTrack& track = Tracks(direction_)[track_index];
      while (LayoutBox* grid_item = iterator.NextGridItem()) {
        if (items_set.insert(grid_item).is_new_entry) {
          const GridSpan& span = grid_.GridItemSpan(*grid_item, direction_);
          if (span.IntegerSpan() == 1) {
            SizeTrackToFitNonSpanningItem(span, *grid_item, track);
          } else if (!SpanningItemCrossesFlexibleSizedTracks(span)) {
            items_sorted_by_increasing_span.push_back(
                GridItemWithSpan(*grid_item, span));
          }
        }
      }
    }
    std::sort(items_sorted_by_increasing_span.begin(),
              items_sorted_by_increasing_span.end());
  }

  auto* it = items_sorted_by_increasing_span.begin();
  auto* end = items_sorted_by_increasing_span.end();
  while (it != end) {
    GridItemsSpanGroupRange span_group_range = {it,
                                                std::upper_bound(it, end, *it)};
    IncreaseSizesToAccommodateSpanningItems<kResolveIntrinsicMinimums>(
        span_group_range);
    IncreaseSizesToAccommodateSpanningItems<kResolveContentBasedMinimums>(
        span_group_range);
    IncreaseSizesToAccommodateSpanningItems<kResolveMaxContentMinimums>(
        span_group_range);
    IncreaseSizesToAccommodateSpanningItems<kResolveIntrinsicMaximums>(
        span_group_range);
    IncreaseSizesToAccommodateSpanningItems<kResolveMaxContentMaximums>(
        span_group_range);
    it = span_group_range.range_end;
  }

  for (const auto& track_index : content_sized_tracks_index_) {
    GridTrack& track = Tracks(direction_)[track_index];
    if (track.GrowthLimit() == kInfinity)
      track.SetGrowthLimit(track.BaseSize());
  }
}

void GridTrackSizingAlgorithm::ComputeGridContainerIntrinsicSizes() {
  min_content_size_ = max_content_size_ = LayoutUnit();

  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (auto& track : all_tracks) {
    DCHECK(!track.InfiniteGrowthPotential());
    min_content_size_ += track.BaseSize();
    max_content_size_ += track.GrowthLimit();
    // The growth limit caps must be cleared now in order to properly sort
    // tracks by growth potential on an eventual "Maximize Tracks".
    track.SetGrowthLimitCap(WTF::nullopt);
  }
}

LayoutUnit GridTrackSizingAlgorithm::ComputeTrackBasedSize() const {
  LayoutUnit size;

  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (auto& track : all_tracks)
    size += track.BaseSize();

  size += layout_grid_->GuttersSize(grid_, direction_, 0, all_tracks.size(),
                                    AvailableSpace());

  return size;
}

double GridTrackSizingAlgorithm::FindFrUnitSize(
    const GridSpan& tracks_span,
    LayoutUnit left_over_space) const {
  if (left_over_space <= 0)
    return 0;

  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  double flex_factor_sum = 0;
  Vector<size_t, 8> flexible_tracks_indexes;
  for (const auto& track_index : tracks_span) {
    GridTrackSize track_size = GetGridTrackSize(direction_, track_index);
    if (!track_size.MaxTrackBreadth().IsFlex()) {
      left_over_space -= all_tracks[track_index].BaseSize();
    } else {
      flexible_tracks_indexes.push_back(track_index);
      flex_factor_sum += track_size.MaxTrackBreadth().Flex();
    }
  }
  // We don't remove the gutters from left_over_space here, because that was
  // already done before.

  // The function is not called if we don't have <flex> grid tracks.
  DCHECK(!flexible_tracks_indexes.IsEmpty());

  return ComputeFlexFactorUnitSize(all_tracks, flex_factor_sum, left_over_space,
                                   flexible_tracks_indexes);
}

double GridTrackSizingAlgorithm::ComputeFlexFactorUnitSize(
    const Vector<GridTrack>& tracks,
    double flex_factor_sum,
    LayoutUnit& left_over_space,
    const Vector<size_t, 8>& flexible_tracks_indexes,
    std::unique_ptr<TrackIndexSet> tracks_to_treat_as_inflexible) const {
  // We want to avoid the effect of flex factors sum below 1 making the factor
  // unit size to grow exponentially.
  double hypothetical_factor_unit_size =
      left_over_space / std::max<double>(1, flex_factor_sum);

  // product of the hypothetical "flex factor unit" and any flexible track's
  // "flex factor" must be grater than such track's "base size".
  std::unique_ptr<TrackIndexSet> additional_tracks_to_treat_as_inflexible =
      std::move(tracks_to_treat_as_inflexible);
  bool valid_flex_factor_unit = true;
  for (auto index : flexible_tracks_indexes) {
    if (additional_tracks_to_treat_as_inflexible &&
        additional_tracks_to_treat_as_inflexible->Contains(index))
      continue;
    LayoutUnit base_size = tracks[index].BaseSize();
    double flex_factor =
        GetGridTrackSize(direction_, index).MaxTrackBreadth().Flex();
    // treating all such tracks as inflexible.
    if (base_size > hypothetical_factor_unit_size * flex_factor) {
      left_over_space -= base_size;
      flex_factor_sum -= flex_factor;
      if (!additional_tracks_to_treat_as_inflexible) {
        additional_tracks_to_treat_as_inflexible =
            std::make_unique<TrackIndexSet>();
      }
      additional_tracks_to_treat_as_inflexible->insert(index);
      valid_flex_factor_unit = false;
    }
  }
  if (!valid_flex_factor_unit) {
    return ComputeFlexFactorUnitSize(
        tracks, flex_factor_sum, left_over_space, flexible_tracks_indexes,
        std::move(additional_tracks_to_treat_as_inflexible));
  }
  return hypothetical_factor_unit_size;
}

void GridTrackSizingAlgorithm::ComputeFlexSizedTracksGrowth(
    double flex_fraction,
    Vector<LayoutUnit>& increments,
    LayoutUnit& total_growth) const {
  size_t num_flex_tracks = flexible_sized_tracks_index_.size();
  DCHECK_EQ(increments.size(), num_flex_tracks);
  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (size_t i = 0; i < num_flex_tracks; ++i) {
    size_t track_index = flexible_sized_tracks_index_[i];
    auto track_size = GetGridTrackSize(direction_, track_index);
    DCHECK(track_size.MaxTrackBreadth().IsFlex());
    LayoutUnit old_base_size = all_tracks[track_index].BaseSize();
    LayoutUnit new_base_size = std::max(
        old_base_size,
        LayoutUnit(flex_fraction * track_size.MaxTrackBreadth().Flex()));
    increments[i] = new_base_size - old_base_size;
    total_growth += increments[i];
  }
}

void GridTrackSizingAlgorithm::StretchFlexibleTracks(
    Optional<LayoutUnit> free_space) {
  if (flexible_sized_tracks_index_.IsEmpty())
    return;

  double flex_fraction = strategy_->FindUsedFlexFraction(
      flexible_sized_tracks_index_, direction_, free_space);

  LayoutUnit total_growth;
  Vector<LayoutUnit> increments;
  increments.Grow(flexible_sized_tracks_index_.size());
  ComputeFlexSizedTracksGrowth(flex_fraction, increments, total_growth);

  if (strategy_->RecomputeUsedFlexFractionIfNeeded(flexible_sized_tracks_index_,
                                                   flex_fraction, increments,
                                                   total_growth)) {
    total_growth = LayoutUnit(0);
    ComputeFlexSizedTracksGrowth(flex_fraction, increments, total_growth);
  }

  size_t i = 0;
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (auto track_index : flexible_sized_tracks_index_) {
    auto& track = all_tracks[track_index];
    if (LayoutUnit increment = increments[i++])
      track.SetBaseSize(track.BaseSize() + increment);
  }
  if (FreeSpace(direction_)) {
    SetFreeSpace(direction_, FreeSpace(direction_).value() - total_growth);
  }
  max_content_size_ += total_growth;
}

void GridTrackSizingAlgorithm::StretchAutoTracks() {
  LayoutUnit free_space = strategy_->FreeSpaceForStretchAutoTracksStep();
  if (auto_sized_tracks_for_stretch_index_.IsEmpty() || (free_space <= 0) ||
      (layout_grid_->ContentAlignment(direction_).Distribution() !=
       ContentDistributionType::kStretch))
    return;

  unsigned number_of_auto_sized_tracks =
      auto_sized_tracks_for_stretch_index_.size();
  LayoutUnit size_to_increase = free_space / number_of_auto_sized_tracks;
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (const auto& track_index : auto_sized_tracks_for_stretch_index_) {
    auto& track = all_tracks[track_index];
    LayoutUnit base_size = track.BaseSize() + size_to_increase;
    track.SetBaseSize(base_size);
  }
  SetFreeSpace(direction_, LayoutUnit());
}

void GridTrackSizingAlgorithm::AdvanceNextState() {
  switch (sizing_state_) {
    case kColumnSizingFirstIteration:
      sizing_state_ = kRowSizingFirstIteration;
      return;
    case kRowSizingFirstIteration:
      sizing_state_ = kColumnSizingSecondIteration;
      return;
    case kColumnSizingSecondIteration:
      sizing_state_ = kRowSizingSecondIteration;
      return;
    case kRowSizingSecondIteration:
      sizing_state_ = kColumnSizingFirstIteration;
      return;
  }
  NOTREACHED();
  sizing_state_ = kColumnSizingFirstIteration;
}

bool GridTrackSizingAlgorithm::IsValidTransition() const {
  switch (sizing_state_) {
    case kColumnSizingFirstIteration:
    case kColumnSizingSecondIteration:
      return direction_ == kForColumns;
    case kRowSizingFirstIteration:
    case kRowSizingSecondIteration:
      return direction_ == kForRows;
  }
  NOTREACHED();
  return false;
}

void GridTrackSizingAlgorithm::Setup(GridTrackSizingDirection direction,
                                     size_t num_tracks,
                                     Optional<LayoutUnit> available_space) {
  DCHECK(needs_setup_);
  direction_ = direction;
  SetAvailableSpace(
      direction, available_space ? available_space.value().ClampNegativeToZero()
                                 : available_space);

  if (available_space) {
    strategy_ = std::make_unique<DefiniteSizeStrategy>(*this);
  } else {
    strategy_ = std::make_unique<IndefiniteSizeStrategy>(*this);
    is_in_perform_layout_ =
        layout_grid_->GetDocument().View()->IsInPerformLayout();
  }

  content_sized_tracks_index_.Shrink(0);
  flexible_sized_tracks_index_.Shrink(0);
  auto_sized_tracks_for_stretch_index_.Shrink(0);

  if (available_space) {
    LayoutUnit gutters_size = layout_grid_->GuttersSize(
        grid_, direction, 0, grid_.NumTracks(direction), available_space);
    SetFreeSpace(direction, available_space.value() - gutters_size);
  } else {
    SetFreeSpace(direction, WTF::nullopt);
  }
  Tracks(direction).resize(num_tracks);

  baseline_alignment_.SetBlockFlow(layout_grid_->StyleRef().GetWritingMode());

  needs_setup_ = false;
}

// Described in https://drafts.csswg.org/css-grid/#algo-track-sizing
void GridTrackSizingAlgorithm::Run() {
  StateMachine state_machine(*this);

  // Step 1.
  Optional<LayoutUnit> initial_free_space = FreeSpace(direction_);
  InitializeTrackSizes();

  // Step 2.
  if (!content_sized_tracks_index_.IsEmpty())
    ResolveIntrinsicTrackSizes();

  // This is not exactly a step of the track sizing algorithm, but we use the
  // track sizes computed
  // up to this moment (before maximization) to calculate the grid container
  // intrinsic sizes.
  ComputeGridContainerIntrinsicSizes();

  if (FreeSpace(direction_)) {
    LayoutUnit updated_free_space =
        FreeSpace(direction_).value() - min_content_size_;
    SetFreeSpace(direction_, updated_free_space);
    if (updated_free_space <= 0)
      return;
  }

  // Step 3.
  strategy_->MaximizeTracks(Tracks(direction_), direction_ == kForColumns
                                                    ? free_space_columns_
                                                    : free_space_rows_);

  // Step 4.
  StretchFlexibleTracks(initial_free_space);

  // Step 5.
  StretchAutoTracks();
}

void GridTrackSizingAlgorithm::Reset() {
  sizing_state_ = kColumnSizingFirstIteration;
  columns_.Shrink(0);
  rows_.Shrink(0);
  content_sized_tracks_index_.Shrink(0);
  flexible_sized_tracks_index_.Shrink(0);
  auto_sized_tracks_for_stretch_index_.Shrink(0);
  SetAvailableSpace(kForRows, WTF::nullopt);
  SetAvailableSpace(kForColumns, WTF::nullopt);
}

#if DCHECK_IS_ON()
bool GridTrackSizingAlgorithm::TracksAreWiderThanMinTrackBreadth() const {
  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (size_t i = 0; i < all_tracks.size(); ++i) {
    GridTrackSize track_size = GetGridTrackSize(direction_, i);
    if (InitialBaseSize(track_size) > all_tracks[i].BaseSize())
      return false;
  }
  return true;
}
#endif

GridTrackSizingAlgorithm::StateMachine::StateMachine(
    GridTrackSizingAlgorithm& algorithm)
    : algorithm_(algorithm) {
  DCHECK(algorithm_.IsValidTransition());
  DCHECK(!algorithm_.needs_setup_);
}

GridTrackSizingAlgorithm::StateMachine::~StateMachine() {
  algorithm_.AdvanceNextState();
  algorithm_.needs_setup_ = true;
}

}  // namespace blink
