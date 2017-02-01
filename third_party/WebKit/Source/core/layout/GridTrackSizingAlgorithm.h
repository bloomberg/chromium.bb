// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridTrackSizingAlgorithm_h
#define GridTrackSizingAlgorithm_h

#include "core/style/GridPositionsResolver.h"
#include "core/style/GridTrackSize.h"
#include "platform/LayoutUnit.h"
#include "wtf/HashSet.h"
#include "wtf/Optional.h"
#include <memory>

namespace blink {

static const int infinity = -1;

class Grid;
class GridTrackSizingAlgorithmStrategy;
class LayoutGrid;

enum SizingOperation { TrackSizing, IntrinsicSizeComputation };

enum TrackSizeComputationPhase {
  ResolveIntrinsicMinimums,
  ResolveContentBasedMinimums,
  ResolveMaxContentMinimums,
  ResolveIntrinsicMaximums,
  ResolveMaxContentMaximums,
  MaximizeTracks,
};

class GridTrack {
 public:
  GridTrack() : m_infinitelyGrowable(false) {}

  LayoutUnit baseSize() const;
  void setBaseSize(LayoutUnit);

  LayoutUnit growthLimit() const;
  void setGrowthLimit(LayoutUnit);

  bool infiniteGrowthPotential() const;

  LayoutUnit plannedSize() const { return m_plannedSize; }
  void setPlannedSize(LayoutUnit);

  LayoutUnit sizeDuringDistribution() const { return m_sizeDuringDistribution; }
  void setSizeDuringDistribution(LayoutUnit);

  void growSizeDuringDistribution(LayoutUnit);

  bool infinitelyGrowable() const { return m_infinitelyGrowable; }
  void setInfinitelyGrowable(bool);

  Optional<LayoutUnit> growthLimitCap() const { return m_growthLimitCap; }
  void setGrowthLimitCap(Optional<LayoutUnit>);

 private:
  bool growthLimitIsInfinite() const { return m_growthLimit == infinity; }
  bool isGrowthLimitBiggerThanBaseSize() const;
  void ensureGrowthLimitIsBiggerThanBaseSize();

  LayoutUnit m_baseSize;
  LayoutUnit m_growthLimit;
  LayoutUnit m_plannedSize;
  LayoutUnit m_sizeDuringDistribution;
  Optional<LayoutUnit> m_growthLimitCap;
  bool m_infinitelyGrowable;
};

class GridTrackSizingAlgorithm final {
  friend class GridTrackSizingAlgorithmStrategy;

 public:
  GridTrackSizingAlgorithm(const LayoutGrid* layoutGrid, Grid& grid)
      : m_grid(grid),
        m_layoutGrid(layoutGrid),
        m_sizingState(ColumnSizingFirstIteration) {}

  // setup() must be run before calling run() as it configures the behaviour of
  // the algorithm.
  void setup(GridTrackSizingDirection,
             size_t numTracks,
             SizingOperation,
             LayoutUnit availableSpace,
             LayoutUnit freeSpace);
  void run();
  void reset();

  // Required by LayoutGrid. Try to minimize the exposed surface.
  const Grid& grid() const { return m_grid; }
  GridTrackSize gridTrackSize(GridTrackSizingDirection,
                              size_t translatedIndex,
                              SizingOperation) const;
  LayoutUnit minContentSize() const { return m_minContentSize; };
  LayoutUnit maxContentSize() const { return m_maxContentSize; };

  Vector<GridTrack>& tracks(GridTrackSizingDirection);
  const Vector<GridTrack>& tracks(GridTrackSizingDirection) const;

  LayoutUnit& freeSpace(GridTrackSizingDirection);

#if DCHECK_IS_ON()
  bool tracksAreWiderThanMinTrackBreadth() const;
#endif

 private:
  GridTrackSize gridTrackSize(GridTrackSizingDirection,
                              size_t translatedIndex) const;
  GridTrackSize rawGridTrackSize(GridTrackSizingDirection,
                                 size_t translatedIndex) const;
  LayoutUnit assumedRowsSizeForOrthogonalChild(const LayoutBox&) const;
  LayoutUnit computeTrackBasedSize() const;

  // Helper methods for step 1. initializeTrackSizes().
  LayoutUnit initialBaseSize(const GridTrackSize&) const;
  LayoutUnit initialGrowthLimit(const GridTrackSize&,
                                LayoutUnit baseSize) const;

  // Helper methods for step 2. resolveIntrinsicTrackSizes().
  void sizeTrackToFitNonSpanningItem(const GridSpan&,
                                     LayoutBox& gridItem,
                                     GridTrack&);
  bool spanningItemCrossesFlexibleSizedTracks(const GridSpan&) const;
  typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
  template <TrackSizeComputationPhase phase>
  void increaseSizesToAccommodateSpanningItems(
      const GridItemsSpanGroupRange& gridItemsWithSpan);
  LayoutUnit itemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase,
                                                  LayoutBox&) const;
  template <TrackSizeComputationPhase phase>
  void distributeSpaceToTracks(Vector<GridTrack*>& tracks,
                               Vector<GridTrack*>* growBeyondGrowthLimitsTracks,
                               LayoutUnit& availableLogicalSpace) const;
  LayoutUnit gridAreaBreadthForChild(const LayoutBox&,
                                     GridTrackSizingDirection);

  void computeGridContainerIntrinsicSizes();

  // Helper methods for step 4. Strech flexible tracks.
  typedef HashSet<size_t,
                  DefaultHash<size_t>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<size_t>>
      TrackIndexSet;
  double computeFlexFactorUnitSize(
      const Vector<GridTrack>& tracks,
      double flexFactorSum,
      LayoutUnit& leftOverSpace,
      const Vector<size_t, 8>& flexibleTracksIndexes,
      std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible = nullptr) const;
  void computeFlexSizedTracksGrowth(double flexFraction,
                                    Vector<LayoutUnit>& increments,
                                    LayoutUnit& totalGrowth) const;
  double findFrUnitSize(const GridSpan& tracksSpan,
                        LayoutUnit leftOverSpace) const;

  // Track sizing algorithm steps. Note that the "Maximize Tracks" step is done
  // entirely inside the strategies, that's why we don't need an additional
  // method at thise level.
  void initializeTrackSizes();
  void resolveIntrinsicTrackSizes();
  void stretchFlexibleTracks(LayoutUnit freeSpace);

  // State machine.
  void advanceNextState();
  bool isValidTransition() const;

  // Data.
  bool m_needsSetup{true};
  LayoutUnit m_availableSpace;

  LayoutUnit m_freeSpaceColumns;
  LayoutUnit m_freeSpaceRows;

  // We need to keep both alive in order to properly size grids with orthogonal
  // writing modes.
  Vector<GridTrack> m_columns;
  Vector<GridTrack> m_rows;
  Vector<size_t> m_contentSizedTracksIndex;
  Vector<size_t> m_flexibleSizedTracksIndex;

  GridTrackSizingDirection m_direction;
  SizingOperation m_sizingOperation;

  Grid& m_grid;

  const LayoutGrid* m_layoutGrid;
  std::unique_ptr<GridTrackSizingAlgorithmStrategy> m_strategy;

  // The track sizing algorithm is used for both layout and intrinsic size
  // computation. We're normally just interested in intrinsic inline sizes
  // (a.k.a widths in most of the cases) for the computeIntrinsicLogicalWidths()
  // computations. That's why we don't need to keep around different values for
  // rows/columns.
  LayoutUnit m_minContentSize;
  LayoutUnit m_maxContentSize;

  enum SizingState {
    ColumnSizingFirstIteration,
    RowSizingFirstIteration,
    ColumnSizingSecondIteration,
    RowSizingSecondIteration
  };
  SizingState m_sizingState;

  // This is a RAII class used to ensure that the track sizing algorithm is
  // executed as it is suppossed to be, i.e., first resolve columns and then
  // rows. Only if required a second iteration is run following the same order,
  // first columns and then rows.
  class StateMachine {
   public:
    StateMachine(GridTrackSizingAlgorithm&);
    ~StateMachine();

   private:
    GridTrackSizingAlgorithm& m_algorithm;
  };
};

class GridTrackSizingAlgorithmStrategy {
  WTF_MAKE_NONCOPYABLE(GridTrackSizingAlgorithmStrategy);
  USING_FAST_MALLOC(GridTrackSizingAlgorithmStrategy);

 public:
  LayoutUnit minContentForChild(LayoutBox&) const;
  LayoutUnit maxContentForChild(LayoutBox&) const;
  LayoutUnit minSizeForChild(LayoutBox&) const;

  virtual void maximizeTracks(Vector<GridTrack>&, LayoutUnit& freeSpace) = 0;
  virtual double findUsedFlexFraction(Vector<size_t>& flexibleSizedTracksIndex,
                                      GridTrackSizingDirection,
                                      LayoutUnit initialFreeSpace) const = 0;
  virtual bool recomputeUsedFlexFractionIfNeeded(
      Vector<size_t>& flexibleSizedTracksIndex,
      double& flexFraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& totalGrowth) const = 0;

 protected:
  GridTrackSizingAlgorithmStrategy(GridTrackSizingAlgorithm& algorithm)
      : m_algorithm(algorithm) {}

  virtual LayoutUnit minLogicalWidthForChild(
      LayoutBox&,
      Length childMinSize,
      GridTrackSizingDirection) const = 0;
  virtual void layoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool overrideSizeHasChanged) const = 0;

  LayoutUnit logicalHeightForChild(LayoutBox&) const;

  bool updateOverrideContainingBlockContentSizeForChild(
      LayoutBox&,
      GridTrackSizingDirection) const;
  LayoutUnit computeTrackBasedSize() const;
  GridTrackSizingDirection direction() const { return m_algorithm.m_direction; }
  double findFrUnitSize(const GridSpan& tracksSpan,
                        LayoutUnit leftOverSpace) const;
  void distributeSpaceToTracks(Vector<GridTrack*>& tracks,
                               LayoutUnit& availableLogicalSpace) const;
  const LayoutGrid* layoutGrid() const { return m_algorithm.m_layoutGrid; }

  GridTrackSizingAlgorithm& m_algorithm;
};
}

#endif  // GridTrackSizingAlgorithm_h
