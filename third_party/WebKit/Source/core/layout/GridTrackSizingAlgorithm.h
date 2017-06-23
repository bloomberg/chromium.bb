// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridTrackSizingAlgorithm_h
#define GridTrackSizingAlgorithm_h

#include <memory>
#include "core/layout/LayoutBox.h"
#include "core/style/GridPositionsResolver.h"
#include "core/style/GridTrackSize.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Optional.h"

namespace blink {

static const int kInfinity = -1;

class Grid;
class GridTrackSizingAlgorithmStrategy;
class LayoutGrid;

enum TrackSizeComputationPhase {
  kResolveIntrinsicMinimums,
  kResolveContentBasedMinimums,
  kResolveMaxContentMinimums,
  kResolveIntrinsicMaximums,
  kResolveMaxContentMaximums,
  kMaximizeTracks,
};

class GridTrack {
 public:
  GridTrack() : infinitely_growable_(false) {}

  LayoutUnit BaseSize() const;
  void SetBaseSize(LayoutUnit);

  LayoutUnit GrowthLimit() const;
  void SetGrowthLimit(LayoutUnit);

  bool InfiniteGrowthPotential() const;

  LayoutUnit PlannedSize() const { return planned_size_; }
  void SetPlannedSize(LayoutUnit);

  LayoutUnit SizeDuringDistribution() const {
    return size_during_distribution_;
  }
  void SetSizeDuringDistribution(LayoutUnit);

  void GrowSizeDuringDistribution(LayoutUnit);

  bool InfinitelyGrowable() const { return infinitely_growable_; }
  void SetInfinitelyGrowable(bool);

  Optional<LayoutUnit> GrowthLimitCap() const { return growth_limit_cap_; }
  void SetGrowthLimitCap(Optional<LayoutUnit>);

 private:
  bool GrowthLimitIsInfinite() const { return growth_limit_ == kInfinity; }
  bool IsGrowthLimitBiggerThanBaseSize() const;
  void EnsureGrowthLimitIsBiggerThanBaseSize();

  LayoutUnit base_size_;
  LayoutUnit growth_limit_;
  LayoutUnit planned_size_;
  LayoutUnit size_during_distribution_;
  Optional<LayoutUnit> growth_limit_cap_;
  bool infinitely_growable_;
};

class GridTrackSizingAlgorithm final {
  friend class GridTrackSizingAlgorithmStrategy;

 public:
  GridTrackSizingAlgorithm(const LayoutGrid* layout_grid, Grid& grid)
      : grid_(grid),
        layout_grid_(layout_grid),
        sizing_state_(kColumnSizingFirstIteration) {}

  // setup() must be run before calling run() as it configures the behaviour of
  // the algorithm.
  void Setup(GridTrackSizingDirection,
             size_t num_tracks,
             Optional<LayoutUnit> available_space,
             Optional<LayoutUnit> free_space);
  void Run();
  void Reset();

  // Required by LayoutGrid. Try to minimize the exposed surface.
  const Grid& GetGrid() const { return grid_; }
  GridTrackSize GetGridTrackSize(GridTrackSizingDirection,
                                 size_t translated_index) const;
  LayoutUnit MinContentSize() const { return min_content_size_; };
  LayoutUnit MaxContentSize() const { return max_content_size_; };

  Vector<GridTrack>& Tracks(GridTrackSizingDirection);
  const Vector<GridTrack>& Tracks(GridTrackSizingDirection) const;

  Optional<LayoutUnit> FreeSpace(GridTrackSizingDirection) const;
  void SetFreeSpace(GridTrackSizingDirection, Optional<LayoutUnit>);

  Optional<LayoutUnit> AvailableSpace(GridTrackSizingDirection) const;
  void SetAvailableSpace(GridTrackSizingDirection, Optional<LayoutUnit>);

#if DCHECK_IS_ON()
  bool TracksAreWiderThanMinTrackBreadth() const;
#endif

 private:
  Optional<LayoutUnit> AvailableSpace() const;
  GridTrackSize RawGridTrackSize(GridTrackSizingDirection,
                                 size_t translated_index) const;
  LayoutUnit AssumedRowsSizeForOrthogonalChild(const LayoutBox&) const;
  LayoutUnit ComputeTrackBasedSize() const;

  // Helper methods for step 1. initializeTrackSizes().
  LayoutUnit InitialBaseSize(const GridTrackSize&) const;
  LayoutUnit InitialGrowthLimit(const GridTrackSize&,
                                LayoutUnit base_size) const;

  // Helper methods for step 2. resolveIntrinsicTrackSizes().
  void SizeTrackToFitNonSpanningItem(const GridSpan&,
                                     LayoutBox& grid_item,
                                     GridTrack&);
  bool SpanningItemCrossesFlexibleSizedTracks(const GridSpan&) const;
  typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
  template <TrackSizeComputationPhase phase>
  void IncreaseSizesToAccommodateSpanningItems(
      const GridItemsSpanGroupRange& grid_items_with_span);
  LayoutUnit ItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase,
                                                  LayoutBox&) const;
  template <TrackSizeComputationPhase phase>
  void DistributeSpaceToTracks(
      Vector<GridTrack*>& tracks,
      Vector<GridTrack*>* grow_beyond_growth_limits_tracks,
      LayoutUnit& available_logical_space) const;
  LayoutUnit GridAreaBreadthForChild(const LayoutBox&,
                                     GridTrackSizingDirection);

  void ComputeGridContainerIntrinsicSizes();

  // Helper methods for step 4. Strech flexible tracks.
  typedef HashSet<size_t,
                  DefaultHash<size_t>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<size_t>>
      TrackIndexSet;
  double ComputeFlexFactorUnitSize(
      const Vector<GridTrack>& tracks,
      double flex_factor_sum,
      LayoutUnit& left_over_space,
      const Vector<size_t, 8>& flexible_tracks_indexes,
      std::unique_ptr<TrackIndexSet> tracks_to_treat_as_inflexible =
          nullptr) const;
  void ComputeFlexSizedTracksGrowth(double flex_fraction,
                                    Vector<LayoutUnit>& increments,
                                    LayoutUnit& total_growth) const;
  double FindFrUnitSize(const GridSpan& tracks_span,
                        LayoutUnit left_over_space) const;

  // Track sizing algorithm steps. Note that the "Maximize Tracks" step is done
  // entirely inside the strategies, that's why we don't need an additional
  // method at thise level.
  void InitializeTrackSizes();
  void ResolveIntrinsicTrackSizes();
  void StretchFlexibleTracks(Optional<LayoutUnit> free_space);

  // State machine.
  void AdvanceNextState();
  bool IsValidTransition() const;

  // Data.
  bool needs_setup_{true};
  Optional<LayoutUnit> available_space_columns_;
  Optional<LayoutUnit> available_space_rows_;

  Optional<LayoutUnit> free_space_columns_;
  Optional<LayoutUnit> free_space_rows_;

  // We need to keep both alive in order to properly size grids with orthogonal
  // writing modes.
  Vector<GridTrack> columns_;
  Vector<GridTrack> rows_;
  Vector<size_t> content_sized_tracks_index_;
  Vector<size_t> flexible_sized_tracks_index_;

  GridTrackSizingDirection direction_;

  Grid& grid_;

  const LayoutGrid* layout_grid_;
  std::unique_ptr<GridTrackSizingAlgorithmStrategy> strategy_;

  // The track sizing algorithm is used for both layout and intrinsic size
  // computation. We're normally just interested in intrinsic inline sizes
  // (a.k.a widths in most of the cases) for the computeIntrinsicLogicalWidths()
  // computations. That's why we don't need to keep around different values for
  // rows/columns.
  LayoutUnit min_content_size_;
  LayoutUnit max_content_size_;

  enum SizingState {
    kColumnSizingFirstIteration,
    kRowSizingFirstIteration,
    kColumnSizingSecondIteration,
    kRowSizingSecondIteration
  };
  SizingState sizing_state_;

  // This is a RAII class used to ensure that the track sizing algorithm is
  // executed as it is suppossed to be, i.e., first resolve columns and then
  // rows. Only if required a second iteration is run following the same order,
  // first columns and then rows.
  class StateMachine {
   public:
    StateMachine(GridTrackSizingAlgorithm&);
    ~StateMachine();

   private:
    GridTrackSizingAlgorithm& algorithm_;
  };
};

class GridTrackSizingAlgorithmStrategy {
  WTF_MAKE_NONCOPYABLE(GridTrackSizingAlgorithmStrategy);
  USING_FAST_MALLOC(GridTrackSizingAlgorithmStrategy);

 public:
  LayoutUnit MinContentForChild(LayoutBox&) const;
  LayoutUnit MaxContentForChild(LayoutBox&) const;
  LayoutUnit MinSizeForChild(LayoutBox&) const;

  virtual void MaximizeTracks(Vector<GridTrack>&,
                              Optional<LayoutUnit>& free_space) = 0;
  virtual double FindUsedFlexFraction(
      Vector<size_t>& flexible_sized_tracks_index,
      GridTrackSizingDirection,
      Optional<LayoutUnit> initial_free_space) const = 0;
  virtual bool RecomputeUsedFlexFractionIfNeeded(
      Vector<size_t>& flexible_sized_tracks_index,
      double& flex_fraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& total_growth) const = 0;

 protected:
  GridTrackSizingAlgorithmStrategy(GridTrackSizingAlgorithm& algorithm)
      : algorithm_(algorithm) {}

  virtual LayoutUnit MinLogicalWidthForChild(
      LayoutBox&,
      Length child_min_size,
      GridTrackSizingDirection) const = 0;
  virtual void LayoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool override_size_has_changed) const = 0;

  LayoutUnit LogicalHeightForChild(LayoutBox&) const;

  bool UpdateOverrideContainingBlockContentSizeForChild(
      LayoutBox&,
      GridTrackSizingDirection) const;
  LayoutUnit ComputeTrackBasedSize() const;
  Optional<LayoutUnit> ExtentForBaselineAlignment(LayoutBox&) const;

  GridTrackSizingDirection Direction() const { return algorithm_.direction_; }
  double FindFrUnitSize(const GridSpan& tracks_span,
                        LayoutUnit left_over_space) const;
  void DistributeSpaceToTracks(Vector<GridTrack*>& tracks,
                               LayoutUnit& available_logical_space) const;
  const LayoutGrid* GetLayoutGrid() const { return algorithm_.layout_grid_; }
  Optional<LayoutUnit> AvailableSpace() const {
    return algorithm_.AvailableSpace();
  }

  // Helper functions
  static bool HasOverrideContainingBlockContentSizeForChild(
      const LayoutBox& child,
      GridTrackSizingDirection);
  static LayoutUnit OverrideContainingBlockContentSizeForChild(
      const LayoutBox& child,
      GridTrackSizingDirection);
  static bool ShouldClearOverrideContainingBlockContentSizeForChild(
      const LayoutGrid&,
      const LayoutBox& child,
      GridTrackSizingDirection);
  static void SetOverrideContainingBlockContentSizeForChild(
      LayoutBox& child,
      GridTrackSizingDirection,
      LayoutUnit size);
  static GridTrackSizingDirection FlowAwareDirectionForChild(
      const LayoutGrid*,
      const LayoutBox& child,
      GridTrackSizingDirection);

  GridTrackSizingAlgorithm& algorithm_;
};
}

#endif  // GridTrackSizingAlgorithm_h
