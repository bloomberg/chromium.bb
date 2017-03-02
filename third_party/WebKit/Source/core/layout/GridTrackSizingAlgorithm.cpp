// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/GridTrackSizingAlgorithm.h"

#include "core/layout/Grid.h"
#include "core/layout/LayoutGrid.h"
#include "platform/LengthFunctions.h"

namespace blink {

class GridSizingData;

LayoutUnit GridTrack::baseSize() const {
  DCHECK(isGrowthLimitBiggerThanBaseSize());
  return m_baseSize;
}

LayoutUnit GridTrack::growthLimit() const {
  DCHECK(isGrowthLimitBiggerThanBaseSize());
  DCHECK(!m_growthLimitCap || m_growthLimitCap.value() >= m_growthLimit ||
         m_baseSize >= m_growthLimitCap.value());
  return m_growthLimit;
}

void GridTrack::setBaseSize(LayoutUnit baseSize) {
  m_baseSize = baseSize;
  ensureGrowthLimitIsBiggerThanBaseSize();
}

void GridTrack::setGrowthLimit(LayoutUnit growthLimit) {
  m_growthLimit =
      growthLimit == infinity
          ? growthLimit
          : std::min(growthLimit, m_growthLimitCap.value_or(growthLimit));
  ensureGrowthLimitIsBiggerThanBaseSize();
}

bool GridTrack::infiniteGrowthPotential() const {
  return growthLimitIsInfinite() || m_infinitelyGrowable;
}

void GridTrack::setPlannedSize(LayoutUnit plannedSize) {
  DCHECK(plannedSize >= 0 || plannedSize == infinity);
  m_plannedSize = plannedSize;
}

void GridTrack::setSizeDuringDistribution(LayoutUnit sizeDuringDistribution) {
  DCHECK_GE(sizeDuringDistribution, 0);
  DCHECK(growthLimitIsInfinite() || growthLimit() >= sizeDuringDistribution);
  m_sizeDuringDistribution = sizeDuringDistribution;
}

void GridTrack::growSizeDuringDistribution(LayoutUnit sizeDuringDistribution) {
  DCHECK_GE(sizeDuringDistribution, 0);
  m_sizeDuringDistribution += sizeDuringDistribution;
}

void GridTrack::setInfinitelyGrowable(bool infinitelyGrowable) {
  m_infinitelyGrowable = infinitelyGrowable;
}

void GridTrack::setGrowthLimitCap(Optional<LayoutUnit> growthLimitCap) {
  DCHECK(!growthLimitCap || *growthLimitCap >= 0);
  m_growthLimitCap = growthLimitCap;
}

bool GridTrack::isGrowthLimitBiggerThanBaseSize() const {
  return growthLimitIsInfinite() || m_growthLimit >= m_baseSize;
}

void GridTrack::ensureGrowthLimitIsBiggerThanBaseSize() {
  if (m_growthLimit != infinity && m_growthLimit < m_baseSize)
    m_growthLimit = m_baseSize;
}

class IndefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
 public:
  IndefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
      : GridTrackSizingAlgorithmStrategy(algorithm) {}

 private:
  LayoutUnit minLogicalWidthForChild(LayoutBox&,
                                     Length childMinSize,
                                     GridTrackSizingDirection) const override;
  void layoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool overrideSizeHasChanged) const override;
  void maximizeTracks(Vector<GridTrack>&, LayoutUnit& freeSpace) override;
  double findUsedFlexFraction(Vector<size_t>& flexibleSizedTracksIndex,
                              GridTrackSizingDirection,
                              LayoutUnit freeSpace) const override;
  bool recomputeUsedFlexFractionIfNeeded(
      Vector<size_t>& flexibleSizedTracksIndex,
      double& flexFraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& totalGrowth) const override;
};

class DefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
 public:
  DefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
      : GridTrackSizingAlgorithmStrategy(algorithm) {}

 private:
  LayoutUnit minLogicalWidthForChild(LayoutBox&,
                                     Length childMinSize,
                                     GridTrackSizingDirection) const override;
  void layoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool overrideSizeHasChanged) const override;
  void maximizeTracks(Vector<GridTrack>&, LayoutUnit& freeSpace) override;
  double findUsedFlexFraction(Vector<size_t>& flexibleSizedTracksIndex,
                              GridTrackSizingDirection,
                              LayoutUnit freeSpace) const override;
  bool recomputeUsedFlexFractionIfNeeded(
      Vector<size_t>& flexibleSizedTracksIndex,
      double& flexFraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& totalGrowth) const override {
    return false;
  }
};

// TODO(svillar): Repeated in LayoutGrid.
static LayoutUnit computeMarginLogicalSizeForChild(MarginDirection forDirection,
                                                   const LayoutGrid* grid,
                                                   const LayoutBox& child) {
  if (!child.styleRef().hasMargin())
    return LayoutUnit();

  bool isRowAxis = forDirection == InlineDirection;
  LayoutUnit marginStart;
  LayoutUnit marginEnd;
  LayoutUnit logicalSize =
      isRowAxis ? child.logicalWidth() : child.logicalHeight();
  Length marginStartLength = isRowAxis ? child.styleRef().marginStart()
                                       : child.styleRef().marginBefore();
  Length marginEndLength =
      isRowAxis ? child.styleRef().marginEnd() : child.styleRef().marginAfter();
  child.computeMarginsForDirection(
      forDirection, grid, child.containingBlockLogicalWidthForContent(),
      logicalSize, marginStart, marginEnd, marginStartLength, marginEndLength);

  return marginStart + marginEnd;
}

static bool hasOverrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == ForColumns
             ? child.hasOverrideContainingBlockLogicalWidth()
             : child.hasOverrideContainingBlockLogicalHeight();
}

static LayoutUnit overrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == ForColumns
             ? child.overrideContainingBlockContentLogicalWidth()
             : child.overrideContainingBlockContentLogicalHeight();
}

static bool shouldClearOverrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  if (direction == ForColumns) {
    return child.hasRelativeLogicalWidth() ||
           child.styleRef().logicalWidth().isIntrinsicOrAuto();
  }
  return child.hasRelativeLogicalHeight() ||
         child.styleRef().logicalHeight().isIntrinsicOrAuto();
}

static void setOverrideContainingBlockContentSizeForChild(
    LayoutBox& child,
    GridTrackSizingDirection direction,
    LayoutUnit size) {
  if (direction == ForColumns)
    child.setOverrideContainingBlockContentLogicalWidth(size);
  else
    child.setOverrideContainingBlockContentLogicalHeight(size);
}

static GridTrackSizingDirection flowAwareDirectionForChild(
    const LayoutGrid* layoutGrid,
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return child.isHorizontalWritingMode() ==
                 layoutGrid->isHorizontalWritingMode()
             ? direction
             : (direction == ForColumns ? ForRows : ForColumns);
}

LayoutUnit GridTrackSizingAlgorithm::assumedRowsSizeForOrthogonalChild(
    const LayoutBox& child) const {
  DCHECK(m_layoutGrid->isOrthogonalChild(child));
  const GridSpan& span = m_grid.gridItemSpan(child, ForRows);
  LayoutUnit gridAreaSize;
  bool gridAreaIsIndefinite = false;
  LayoutUnit containingBlockAvailableSize =
      m_layoutGrid->containingBlockLogicalHeightForContent(
          ExcludeMarginBorderPadding);
  for (auto trackPosition : span) {
    GridLength maxTrackSize =
        gridTrackSize(ForRows, trackPosition).maxTrackBreadth();
    if (maxTrackSize.isContentSized() || maxTrackSize.isFlex()) {
      gridAreaIsIndefinite = true;
    } else {
      gridAreaSize +=
          valueForLength(maxTrackSize.length(), containingBlockAvailableSize);
    }
  }

  gridAreaSize += m_layoutGrid->guttersSize(
      m_grid, ForRows, span.startLine(), span.integerSpan(), m_sizingOperation);

  return gridAreaIsIndefinite
             ? std::max(child.maxPreferredLogicalWidth(), gridAreaSize)
             : gridAreaSize;
}

LayoutUnit GridTrackSizingAlgorithm::gridAreaBreadthForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  if (direction == ForRows && m_sizingState == ColumnSizingFirstIteration)
    return assumedRowsSizeForOrthogonalChild(child);

  Vector<GridTrack>& allTracks = tracks(direction);
  const GridSpan& span = m_grid.gridItemSpan(child, direction);
  LayoutUnit gridAreaBreadth;
  for (const auto& trackPosition : span)
    gridAreaBreadth += allTracks[trackPosition].baseSize();

  gridAreaBreadth +=
      m_layoutGrid->guttersSize(m_grid, direction, span.startLine(),
                                span.integerSpan(), m_sizingOperation);

  return gridAreaBreadth;
}

bool GridTrackSizingAlgorithmStrategy::
    updateOverrideContainingBlockContentSizeForChild(
        LayoutBox& child,
        GridTrackSizingDirection direction) const {
  LayoutUnit overrideSize =
      m_algorithm.gridAreaBreadthForChild(child, direction);
  if (hasOverrideContainingBlockContentSizeForChild(child, direction) &&
      overrideContainingBlockContentSizeForChild(child, direction) ==
          overrideSize)
    return false;

  setOverrideContainingBlockContentSizeForChild(child, direction, overrideSize);
  return true;
}

LayoutUnit GridTrackSizingAlgorithmStrategy::logicalHeightForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection childBlockDirection =
      flowAwareDirectionForChild(layoutGrid(), child, ForRows);

  // If |child| has a relative logical height, we shouldn't let it override its
  // intrinsic height, which is what we are interested in here. Thus we need to
  // set the block-axis override size to -1 (no possible resolution).
  if (shouldClearOverrideContainingBlockContentSizeForChild(child, ForRows)) {
    setOverrideContainingBlockContentSizeForChild(child, childBlockDirection,
                                                  LayoutUnit(-1));
    child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
  }

  // We need to clear the stretched height to properly compute logical height
  // during layout.
  if (child.needsLayout())
    child.clearOverrideLogicalContentHeight();

  child.layoutIfNeeded();
  return child.logicalHeight() + child.marginLogicalHeight();
}

DISABLE_CFI_PERF
LayoutUnit GridTrackSizingAlgorithmStrategy::minContentForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection childInlineDirection =
      flowAwareDirectionForChild(layoutGrid(), child, ForColumns);
  if (direction() == childInlineDirection) {
    // If |child| has a relative logical width, we shouldn't let it override its
    // intrinsic width, which is what we are interested in here. Thus we need to
    // set the inline-axis override size to -1 (no possible resolution).
    if (shouldClearOverrideContainingBlockContentSizeForChild(child,
                                                              ForColumns)) {
      setOverrideContainingBlockContentSizeForChild(child, childInlineDirection,
                                                    LayoutUnit(-1));
    }

    // FIXME: It's unclear if we should return the intrinsic width or the
    // preferred width.
    // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
    LayoutUnit marginLogicalWidth =
        child.needsLayout() ? computeMarginLogicalSizeForChild(
                                  InlineDirection, layoutGrid(), child)
                            : child.marginLogicalWidth();
    return child.minPreferredLogicalWidth() + marginLogicalWidth;
  }

  // All orthogonal flow boxes were already laid out during an early layout
  // phase performed in FrameView::performLayout.
  // It's true that grid track sizing was not completed at that time and it may
  // afffect the final height of a grid item, but since it's forbidden to
  // perform a layout during intrinsic width computation, we have to use that
  // computed height for now.
  if (direction() == ForColumns &&
      m_algorithm.m_sizingOperation == IntrinsicSizeComputation) {
    DCHECK(layoutGrid()->isOrthogonalChild(child));
    return child.logicalHeight() + child.marginLogicalHeight();
  }

  if (updateOverrideContainingBlockContentSizeForChild(child,
                                                       childInlineDirection))
    child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
  return logicalHeightForChild(child);
}

DISABLE_CFI_PERF
LayoutUnit GridTrackSizingAlgorithmStrategy::maxContentForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection childInlineDirection =
      flowAwareDirectionForChild(layoutGrid(), child, ForColumns);
  if (direction() == childInlineDirection) {
    // If |child| has a relative logical width, we shouldn't let it override its
    // intrinsic width, which is what we are interested in here. Thus we need to
    // set the inline-axis override size to -1 (no possible resolution).
    if (shouldClearOverrideContainingBlockContentSizeForChild(child,
                                                              ForColumns)) {
      setOverrideContainingBlockContentSizeForChild(child, childInlineDirection,
                                                    LayoutUnit(-1));
    }

    // FIXME: It's unclear if we should return the intrinsic width or the
    // preferred width.
    // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
    LayoutUnit marginLogicalWidth =
        child.needsLayout() ? computeMarginLogicalSizeForChild(
                                  InlineDirection, layoutGrid(), child)
                            : child.marginLogicalWidth();
    return child.maxPreferredLogicalWidth() + marginLogicalWidth;
  }

  if (direction() == ForColumns &&
      m_algorithm.m_sizingOperation == IntrinsicSizeComputation) {
    // All orthogonal flow boxes were already laid out during an early layout
    // phase performed in FrameView::performLayout. It's true that grid track
    // sizing was not completed at that time and it may afffect the final height
    // of a grid item, but since it's forbidden to perform a layout during
    // intrinsic width computation, we have to use that computed height for now.
    DCHECK(layoutGrid()->isOrthogonalChild(child));
    return child.logicalHeight() + child.marginLogicalHeight();
  }

  if (updateOverrideContainingBlockContentSizeForChild(child,
                                                       childInlineDirection))
    child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
  return logicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minSizeForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection childInlineDirection =
      flowAwareDirectionForChild(layoutGrid(), child, ForColumns);
  bool isRowAxis = direction() == childInlineDirection;
  const Length& childSize = isRowAxis ? child.styleRef().logicalWidth()
                                      : child.styleRef().logicalHeight();
  const Length& childMinSize = isRowAxis ? child.styleRef().logicalMinWidth()
                                         : child.styleRef().logicalMinHeight();
  bool overflowIsVisible =
      isRowAxis
          ? child.styleRef().overflowInlineDirection() == EOverflow::kVisible
          : child.styleRef().overflowBlockDirection() == EOverflow::kVisible;
  if (!childSize.isAuto() || (childMinSize.isAuto() && overflowIsVisible)) {
    if (child.isLayoutReplaced() && childSize.isAuto()) {
      // If the box has an aspect ratio and no specified size, its automatic
      // minimum size is the smaller of its content size and its transferred
      // size.
      return isRowAxis ? std::min(child.intrinsicLogicalWidth(),
                                  minContentForChild(child))
                       : std::min(child.intrinsicLogicalHeight(),
                                  minContentForChild(child));
    }
    return minContentForChild(child);
  }

  bool overrideSizeHasChanged =
      updateOverrideContainingBlockContentSizeForChild(child,
                                                       childInlineDirection);
  if (isRowAxis)
    return minLogicalWidthForChild(child, childMinSize, childInlineDirection);

  layoutGridItemForMinSizeComputation(child, overrideSizeHasChanged);

  return child.computeLogicalHeightUsing(MinSize, childMinSize,
                                         child.intrinsicLogicalHeight()) +
         child.marginLogicalHeight() + child.scrollbarLogicalHeight();
}

LayoutUnit GridTrackSizingAlgorithmStrategy::computeTrackBasedSize() const {
  return m_algorithm.computeTrackBasedSize();
}

double GridTrackSizingAlgorithmStrategy::findFrUnitSize(
    const GridSpan& tracksSpan,
    LayoutUnit leftOverSpace) const {
  return m_algorithm.findFrUnitSize(tracksSpan, leftOverSpace);
}

void GridTrackSizingAlgorithmStrategy::distributeSpaceToTracks(
    Vector<GridTrack*>& tracks,
    LayoutUnit& availableLogicalSpace) const {
  m_algorithm.distributeSpaceToTracks<MaximizeTracks>(tracks, nullptr,
                                                      availableLogicalSpace);
}

LayoutUnit DefiniteSizeStrategy::minLogicalWidthForChild(
    LayoutBox& child,
    Length childMinSize,
    GridTrackSizingDirection childInlineDirection) const {
  LayoutUnit marginLogicalWidth =
      computeMarginLogicalSizeForChild(InlineDirection, layoutGrid(), child);
  return child.computeLogicalWidthUsing(
             MinSize, childMinSize, overrideContainingBlockContentSizeForChild(
                                        child, childInlineDirection),
             layoutGrid()) +
         marginLogicalWidth;
}

void DefiniteSizeStrategy::layoutGridItemForMinSizeComputation(
    LayoutBox& child,
    bool overrideSizeHasChanged) const {
  if (overrideSizeHasChanged)
    child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
  child.layoutIfNeeded();
}

void DefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks,
                                          LayoutUnit& freeSpace) {
  size_t tracksSize = tracks.size();
  Vector<GridTrack*> tracksForDistribution(tracksSize);
  for (size_t i = 0; i < tracksSize; ++i) {
    tracksForDistribution[i] = tracks.data() + i;
    tracksForDistribution[i]->setPlannedSize(
        tracksForDistribution[i]->baseSize());
  }

  distributeSpaceToTracks(tracksForDistribution, freeSpace);

  for (auto* track : tracksForDistribution)
    track->setBaseSize(track->plannedSize());
}

double DefiniteSizeStrategy::findUsedFlexFraction(
    Vector<size_t>& flexibleSizedTracksIndex,
    GridTrackSizingDirection direction,
    LayoutUnit freeSpace) const {
  GridSpan allTracksSpan = GridSpan::translatedDefiniteGridSpan(
      0, m_algorithm.tracks(direction).size());
  return findFrUnitSize(allTracksSpan, freeSpace);
}

LayoutUnit IndefiniteSizeStrategy::minLogicalWidthForChild(
    LayoutBox& child,
    Length childMinSize,
    GridTrackSizingDirection childInlineDirection) const {
  // TODO(svillar): we should use marginIntrinsicLogicalWidthForChild() instead
  // but it is protected for LayoutObjects. Apparently none of the current tests
  // fail, so we need a test case for this too.
  LayoutUnit marginLogicalWidth = LayoutUnit();
  return child.computeLogicalWidthUsing(
             MinSize, childMinSize, overrideContainingBlockContentSizeForChild(
                                        child, childInlineDirection),
             layoutGrid()) +
         marginLogicalWidth;
}

void IndefiniteSizeStrategy::layoutGridItemForMinSizeComputation(
    LayoutBox& child,
    bool overrideSizeHasChanged) const {
  if (overrideSizeHasChanged && direction() != ForColumns)
    child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
  child.layoutIfNeeded();
}

void IndefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks,
                                            LayoutUnit&) {
  for (auto& track : tracks)
    track.setBaseSize(track.growthLimit());
}

static inline double normalizedFlexFraction(const GridTrack& track,
                                            double flexFactor) {
  return track.baseSize() / std::max<double>(1, flexFactor);
}

double IndefiniteSizeStrategy::findUsedFlexFraction(
    Vector<size_t>& flexibleSizedTracksIndex,
    GridTrackSizingDirection direction,
    LayoutUnit freeSpace) const {
  auto allTracks = m_algorithm.tracks(direction);

  double flexFraction = 0;
  for (const auto& trackIndex : flexibleSizedTracksIndex) {
    // TODO(svillar): we pass TrackSizing to gridTrackSize() because it does not
    // really matter as we know the track is a flex sized track. It'd be nice
    // not to have to do that.
    flexFraction = std::max(
        flexFraction,
        normalizedFlexFraction(
            allTracks[trackIndex],
            m_algorithm.gridTrackSize(direction, trackIndex, TrackSizing)
                .maxTrackBreadth()
                .flex()));
  }

  const Grid& grid = m_algorithm.grid();
  if (!grid.hasGridItems())
    return flexFraction;

  for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
    GridIterator iterator(grid, direction, flexibleSizedTracksIndex[i]);
    while (LayoutBox* gridItem = iterator.nextGridItem()) {
      const GridSpan& span = grid.gridItemSpan(*gridItem, direction);

      // Do not include already processed items.
      if (i > 0 && span.startLine() <= flexibleSizedTracksIndex[i - 1])
        continue;

      flexFraction = std::max(
          flexFraction, findFrUnitSize(span, maxContentForChild(*gridItem)));
    }
  }

  return flexFraction;
}

bool IndefiniteSizeStrategy::recomputeUsedFlexFractionIfNeeded(
    Vector<size_t>& flexibleSizedTracksIndex,
    double& flexFraction,
    Vector<LayoutUnit>& increments,
    LayoutUnit& totalGrowth) const {
  if (direction() == ForColumns)
    return false;

  const LayoutGrid* layoutGrid = this->layoutGrid();
  LayoutUnit minSize = layoutGrid->computeContentLogicalHeight(
      MinSize, layoutGrid->styleRef().logicalMinHeight(), LayoutUnit(-1));
  LayoutUnit maxSize = layoutGrid->computeContentLogicalHeight(
      MaxSize, layoutGrid->styleRef().logicalMaxHeight(), LayoutUnit(-1));

  // Redo the flex fraction computation using min|max-height as definite
  // available space in case the total height is smaller than min-height or
  // larger than max-height.
  LayoutUnit rowsSize = totalGrowth + computeTrackBasedSize();
  bool checkMinSize = minSize && rowsSize < minSize;
  bool checkMaxSize = maxSize != -1 && rowsSize > maxSize;
  if (!checkMinSize && !checkMaxSize)
    return false;

  LayoutUnit freeSpace = checkMaxSize ? maxSize : LayoutUnit(-1);
  const Grid& grid = m_algorithm.grid();
  freeSpace = std::max(freeSpace, minSize) -
              layoutGrid->guttersSize(grid, ForRows, 0, grid.numTracks(ForRows),
                                      IntrinsicSizeComputation);

  size_t numberOfTracks = m_algorithm.tracks(direction()).size();
  flexFraction = findFrUnitSize(
      GridSpan::translatedDefiniteGridSpan(0, numberOfTracks), freeSpace);
  return true;
}

LayoutUnit& GridTrackSizingAlgorithm::freeSpace(
    GridTrackSizingDirection direction) {
  return direction == ForRows ? m_freeSpaceRows : m_freeSpaceColumns;
}

Vector<GridTrack>& GridTrackSizingAlgorithm::tracks(
    GridTrackSizingDirection direction) {
  return direction == ForColumns ? m_columns : m_rows;
}

const Vector<GridTrack>& GridTrackSizingAlgorithm::tracks(
    GridTrackSizingDirection direction) const {
  return direction == ForColumns ? m_columns : m_rows;
}

GridTrackSize GridTrackSizingAlgorithm::rawGridTrackSize(
    GridTrackSizingDirection direction,
    size_t translatedIndex) const {
  bool isRowAxis = direction == ForColumns;
  const Vector<GridTrackSize>& trackStyles =
      isRowAxis ? m_layoutGrid->styleRef().gridTemplateColumns()
                : m_layoutGrid->styleRef().gridTemplateRows();
  const Vector<GridTrackSize>& autoRepeatTrackStyles =
      isRowAxis ? m_layoutGrid->styleRef().gridAutoRepeatColumns()
                : m_layoutGrid->styleRef().gridAutoRepeatRows();
  const Vector<GridTrackSize>& autoTrackStyles =
      isRowAxis ? m_layoutGrid->styleRef().gridAutoColumns()
                : m_layoutGrid->styleRef().gridAutoRows();
  size_t insertionPoint =
      isRowAxis ? m_layoutGrid->styleRef().gridAutoRepeatColumnsInsertionPoint()
                : m_layoutGrid->styleRef().gridAutoRepeatRowsInsertionPoint();
  size_t autoRepeatTracksCount = m_grid.autoRepeatTracks(direction);

  // We should not use GridPositionsResolver::explicitGridXXXCount() for this
  // because the explicit grid might be larger than the number of tracks in
  // grid-template-rows|columns (if grid-template-areas is specified for
  // example).
  size_t explicitTracksCount = trackStyles.size() + autoRepeatTracksCount;

  int untranslatedIndexAsInt =
      translatedIndex + m_grid.smallestTrackStart(direction);
  size_t autoTrackStylesSize = autoTrackStyles.size();
  if (untranslatedIndexAsInt < 0) {
    int index = untranslatedIndexAsInt % static_cast<int>(autoTrackStylesSize);
    // We need to traspose the index because the first negative implicit line
    // will get the last defined auto track and so on.
    index += index ? autoTrackStylesSize : 0;
    return autoTrackStyles[index];
  }

  size_t untranslatedIndex = static_cast<size_t>(untranslatedIndexAsInt);
  if (untranslatedIndex >= explicitTracksCount) {
    return autoTrackStyles[(untranslatedIndex - explicitTracksCount) %
                           autoTrackStylesSize];
  }

  if (LIKELY(!autoRepeatTracksCount) || untranslatedIndex < insertionPoint)
    return trackStyles[untranslatedIndex];

  if (untranslatedIndex < (insertionPoint + autoRepeatTracksCount)) {
    size_t autoRepeatLocalIndex = untranslatedIndexAsInt - insertionPoint;
    return autoRepeatTrackStyles[autoRepeatLocalIndex %
                                 autoRepeatTrackStyles.size()];
  }

  return trackStyles[untranslatedIndex - autoRepeatTracksCount];
}

GridTrackSize GridTrackSizingAlgorithm::gridTrackSize(
    GridTrackSizingDirection direction,
    size_t translatedIndex) const {
  return gridTrackSize(direction, translatedIndex, m_sizingOperation);
}

GridTrackSize GridTrackSizingAlgorithm::gridTrackSize(
    GridTrackSizingDirection direction,
    size_t translatedIndex,
    SizingOperation sizingOperation) const {
  // Collapse empty auto repeat tracks if auto-fit.
  if (m_grid.hasAutoRepeatEmptyTracks(direction) &&
      m_grid.isEmptyAutoRepeatTrack(direction, translatedIndex))
    return {Length(Fixed), LengthTrackSizing};

  const GridTrackSize& trackSize = rawGridTrackSize(direction, translatedIndex);
  if (trackSize.isFitContent())
    return trackSize;

  GridLength minTrackBreadth = trackSize.minTrackBreadth();
  GridLength maxTrackBreadth = trackSize.maxTrackBreadth();
  // If the logical width/height of the grid container is indefinite, percentage
  // values are treated as <auto>.
  if (minTrackBreadth.hasPercentage() || maxTrackBreadth.hasPercentage()) {
    // For the inline axis this only happens when we're computing the intrinsic
    // sizes.
    if ((sizingOperation == IntrinsicSizeComputation) ||
        (direction == ForRows &&
         !m_layoutGrid->cachedHasDefiniteLogicalHeight())) {
      if (minTrackBreadth.hasPercentage())
        minTrackBreadth = Length(Auto);
      if (maxTrackBreadth.hasPercentage())
        maxTrackBreadth = Length(Auto);
    }
  }

  // Flex sizes are invalid as a min sizing function. However we still can have
  // a flexible |minTrackBreadth| if the track had a flex size directly (e.g.
  // "1fr"), the spec says that in this case it implies an automatic minimum.
  if (minTrackBreadth.isFlex())
    minTrackBreadth = Length(Auto);

  return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

LayoutUnit GridTrackSizingAlgorithm::initialBaseSize(
    const GridTrackSize& trackSize) const {
  const GridLength& gridLength = trackSize.minTrackBreadth();
  if (gridLength.isFlex())
    return LayoutUnit();

  const Length& trackLength = gridLength.length();
  if (trackLength.isSpecified())
    return valueForLength(trackLength, m_availableSpace.clampNegativeToZero());

  DCHECK(trackLength.isMinContent() || trackLength.isAuto() ||
         trackLength.isMaxContent());
  return LayoutUnit();
}

LayoutUnit GridTrackSizingAlgorithm::initialGrowthLimit(
    const GridTrackSize& trackSize,
    LayoutUnit baseSize) const {
  const GridLength& gridLength = trackSize.maxTrackBreadth();
  if (gridLength.isFlex())
    return baseSize;

  const Length& trackLength = gridLength.length();
  if (trackLength.isSpecified())
    return valueForLength(trackLength, m_availableSpace.clampNegativeToZero());

  DCHECK(trackLength.isMinContent() || trackLength.isAuto() ||
         trackLength.isMaxContent());
  return LayoutUnit(infinity);
}

void GridTrackSizingAlgorithm::initializeTrackSizes() {
  DCHECK(m_contentSizedTracksIndex.isEmpty());
  DCHECK(m_flexibleSizedTracksIndex.isEmpty());
  Vector<GridTrack>& trackList = tracks(m_direction);
  bool hasDefiniteFreeSpace = m_sizingOperation == TrackSizing;
  size_t numTracks = trackList.size();
  for (size_t i = 0; i < numTracks; ++i) {
    GridTrackSize trackSize = gridTrackSize(m_direction, i);
    GridTrack& track = trackList[i];
    track.setBaseSize(initialBaseSize(trackSize));
    track.setGrowthLimit(initialGrowthLimit(trackSize, track.baseSize()));
    track.setInfinitelyGrowable(false);

    if (trackSize.isFitContent()) {
      GridLength gridLength = trackSize.fitContentTrackBreadth();
      if (!gridLength.hasPercentage() || hasDefiniteFreeSpace) {
        track.setGrowthLimitCap(valueForLength(
            gridLength.length(), m_availableSpace.clampNegativeToZero()));
      }
    }

    if (trackSize.isContentSized())
      m_contentSizedTracksIndex.push_back(i);
    if (trackSize.maxTrackBreadth().isFlex())
      m_flexibleSizedTracksIndex.push_back(i);
  }
}

void GridTrackSizingAlgorithm::sizeTrackToFitNonSpanningItem(
    const GridSpan& span,
    LayoutBox& gridItem,
    GridTrack& track) {
  const size_t trackPosition = span.startLine();
  GridTrackSize trackSize = gridTrackSize(m_direction, trackPosition);

  if (trackSize.hasMinContentMinTrackBreadth()) {
    track.setBaseSize(
        std::max(track.baseSize(), m_strategy->minContentForChild(gridItem)));
  } else if (trackSize.hasMaxContentMinTrackBreadth()) {
    track.setBaseSize(
        std::max(track.baseSize(), m_strategy->maxContentForChild(gridItem)));
  } else if (trackSize.hasAutoMinTrackBreadth()) {
    track.setBaseSize(
        std::max(track.baseSize(), m_strategy->minSizeForChild(gridItem)));
  }

  if (trackSize.hasMinContentMaxTrackBreadth()) {
    track.setGrowthLimit(std::max(track.growthLimit(),
                                  m_strategy->minContentForChild(gridItem)));
  } else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth()) {
    LayoutUnit growthLimit = m_strategy->maxContentForChild(gridItem);
    if (trackSize.isFitContent()) {
      growthLimit =
          std::min(growthLimit,
                   valueForLength(trackSize.fitContentTrackBreadth().length(),
                                  m_availableSpace));
    }
    track.setGrowthLimit(std::max(track.growthLimit(), growthLimit));
  }
}

bool GridTrackSizingAlgorithm::spanningItemCrossesFlexibleSizedTracks(
    const GridSpan& span) const {
  for (const auto& trackPosition : span) {
    const GridTrackSize& trackSize = gridTrackSize(m_direction, trackPosition);
    if (trackSize.minTrackBreadth().isFlex() ||
        trackSize.maxTrackBreadth().isFlex())
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
  GridItemWithSpan(LayoutBox& gridItem, const GridSpan& gridSpan)
      : m_gridItem(&gridItem), m_gridSpan(gridSpan) {}

  LayoutBox& gridItem() const { return *m_gridItem; }
  GridSpan getGridSpan() const { return m_gridSpan; }

  bool operator<(const GridItemWithSpan other) const {
    return m_gridSpan.integerSpan() < other.m_gridSpan.integerSpan();
  }

 private:
  LayoutBox* m_gridItem;
  GridSpan m_gridSpan;
};

struct GridItemsSpanGroupRange {
  Vector<GridItemWithSpan>::iterator rangeStart;
  Vector<GridItemWithSpan>::iterator rangeEnd;
};

enum TrackSizeRestriction {
  AllowInfinity,
  ForbidInfinity,
};

static LayoutUnit trackSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrack& track,
    TrackSizeRestriction restriction) {
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

  NOTREACHED();
  return track.baseSize();
}

static bool shouldProcessTrackForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrackSize& trackSize) {
  switch (phase) {
    case ResolveIntrinsicMinimums:
      return trackSize.hasIntrinsicMinTrackBreadth();
    case ResolveContentBasedMinimums:
      return trackSize.hasMinOrMaxContentMinTrackBreadth();
    case ResolveMaxContentMinimums:
      return trackSize.hasMaxContentMinTrackBreadth();
    case ResolveIntrinsicMaximums:
      return trackSize.hasIntrinsicMaxTrackBreadth();
    case ResolveMaxContentMaximums:
      return trackSize.hasMaxContentOrAutoMaxTrackBreadth();
    case MaximizeTracks:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

static bool trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrackSize& trackSize) {
  switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
      return trackSize
          .hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case ResolveMaxContentMinimums:
      return trackSize
          .hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
      return true;
    case MaximizeTracks:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

static void markAsInfinitelyGrowableForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    GridTrack& track) {
  switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
      return;
    case ResolveIntrinsicMaximums:
      if (trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity) ==
              infinity &&
          track.plannedSize() != infinity)
        track.setInfinitelyGrowable(true);
      return;
    case ResolveMaxContentMaximums:
      if (track.infinitelyGrowable())
        track.setInfinitelyGrowable(false);
      return;
    case MaximizeTracks:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

static void updateTrackSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    GridTrack& track) {
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
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

LayoutUnit GridTrackSizingAlgorithm::itemSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    LayoutBox& gridItem) const {
  switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveIntrinsicMaximums:
      return m_strategy->minSizeForChild(gridItem);
    case ResolveContentBasedMinimums:
      return m_strategy->minContentForChild(gridItem);
    case ResolveMaxContentMinimums:
    case ResolveMaxContentMaximums:
      return m_strategy->maxContentForChild(gridItem);
    case MaximizeTracks:
      NOTREACHED();
      return LayoutUnit();
  }

  NOTREACHED();
  return LayoutUnit();
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1,
                                           const GridTrack* track2) {
  // This check ensures that we respect the irreflexivity property of the strict
  // weak ordering required by std::sort(forall x: NOT x < x).
  bool track1HasInfiniteGrowthPotentialWithoutCap =
      track1->infiniteGrowthPotential() && !track1->growthLimitCap();
  bool track2HasInfiniteGrowthPotentialWithoutCap =
      track2->infiniteGrowthPotential() && !track2->growthLimitCap();

  if (track1HasInfiniteGrowthPotentialWithoutCap &&
      track2HasInfiniteGrowthPotentialWithoutCap)
    return false;

  if (track1HasInfiniteGrowthPotentialWithoutCap ||
      track2HasInfiniteGrowthPotentialWithoutCap)
    return track2HasInfiniteGrowthPotentialWithoutCap;

  LayoutUnit track1Limit =
      track1->growthLimitCap().value_or(track1->growthLimit());
  LayoutUnit track2Limit =
      track2->growthLimitCap().value_or(track2->growthLimit());
  return (track1Limit - track1->baseSize()) <
         (track2Limit - track2->baseSize());
}

static void clampGrowthShareIfNeeded(TrackSizeComputationPhase phase,
                                     const GridTrack& track,
                                     LayoutUnit& growthShare) {
  if (phase != ResolveMaxContentMaximums || !track.growthLimitCap())
    return;

  LayoutUnit distanceToCap =
      track.growthLimitCap().value() - track.sizeDuringDistribution();
  if (distanceToCap <= 0)
    return;

  growthShare = std::min(growthShare, distanceToCap);
}

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::distributeSpaceToTracks(
    Vector<GridTrack*>& tracks,
    Vector<GridTrack*>* growBeyondGrowthLimitsTracks,
    LayoutUnit& availableLogicalSpace) const {
  DCHECK_GE(availableLogicalSpace, 0);

  for (auto* track : tracks) {
    track->setSizeDuringDistribution(
        trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity));
  }

  if (availableLogicalSpace > 0) {
    std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

    size_t tracksSize = tracks.size();
    for (size_t i = 0; i < tracksSize; ++i) {
      GridTrack& track = *tracks[i];
      LayoutUnit availableLogicalSpaceShare =
          availableLogicalSpace / (tracksSize - i);
      const LayoutUnit& trackBreadth =
          trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
      LayoutUnit growthShare =
          track.infiniteGrowthPotential()
              ? availableLogicalSpaceShare
              : std::min(availableLogicalSpaceShare,
                         track.growthLimit() - trackBreadth);
      clampGrowthShareIfNeeded(phase, track, growthShare);
      DCHECK_GE(growthShare, 0) << "We must never shrink any grid track or "
                                   "else we can't guarantee we abide by our "
                                   "min-sizing function.";
      track.growSizeDuringDistribution(growthShare);
      availableLogicalSpace -= growthShare;
    }
  }

  if (availableLogicalSpace > 0 && growBeyondGrowthLimitsTracks) {
    // We need to sort them because there might be tracks with growth limit caps
    // (like the ones with fit-content()) which cannot indefinitely grow over
    // the limits.
    if (phase == ResolveMaxContentMaximums) {
      std::sort(growBeyondGrowthLimitsTracks->begin(),
                growBeyondGrowthLimitsTracks->end(),
                sortByGridTrackGrowthPotential);
    }

    size_t tracksGrowingAboveMaxBreadthSize =
        growBeyondGrowthLimitsTracks->size();
    for (size_t i = 0; i < tracksGrowingAboveMaxBreadthSize; ++i) {
      GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
      LayoutUnit growthShare =
          availableLogicalSpace / (tracksGrowingAboveMaxBreadthSize - i);
      clampGrowthShareIfNeeded(phase, *track, growthShare);
      DCHECK_GE(growthShare, 0) << "We must never shrink any grid track or "
                                   "else we can't guarantee we abide by our "
                                   "min-sizing function.";
      track->growSizeDuringDistribution(growthShare);
      availableLogicalSpace -= growthShare;
    }
  }

  for (auto* track : tracks) {
    track->setPlannedSize(
        track->plannedSize() == infinity
            ? track->sizeDuringDistribution()
            : std::max(track->plannedSize(), track->sizeDuringDistribution()));
  }
}

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItems(
    const GridItemsSpanGroupRange& gridItemsWithSpan) {
  Vector<GridTrack>& allTracks = tracks(m_direction);
  for (const auto& trackIndex : m_contentSizedTracksIndex) {
    GridTrack& track = allTracks[trackIndex];
    track.setPlannedSize(
        trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity));
  }

  Vector<GridTrack*> growBeyondGrowthLimitsTracks;
  Vector<GridTrack*> filteredTracks;
  for (auto it = gridItemsWithSpan.rangeStart; it != gridItemsWithSpan.rangeEnd;
       ++it) {
    GridItemWithSpan& gridItemWithSpan = *it;
    DCHECK_GT(gridItemWithSpan.getGridSpan().integerSpan(), 1u);
    const GridSpan& itemSpan = gridItemWithSpan.getGridSpan();

    growBeyondGrowthLimitsTracks.shrink(0);
    filteredTracks.shrink(0);
    LayoutUnit spanningTracksSize;
    for (const auto& trackPosition : itemSpan) {
      GridTrackSize trackSize = gridTrackSize(m_direction, trackPosition);
      GridTrack& track = tracks(m_direction)[trackPosition];
      spanningTracksSize +=
          trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
      if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
        continue;

      filteredTracks.push_back(&track);

      if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(
              phase, trackSize))
        growBeyondGrowthLimitsTracks.push_back(&track);
    }

    if (filteredTracks.isEmpty())
      continue;

    spanningTracksSize +=
        m_layoutGrid->guttersSize(m_grid, m_direction, itemSpan.startLine(),
                                  itemSpan.integerSpan(), m_sizingOperation);

    LayoutUnit extraSpace = itemSizeForTrackSizeComputationPhase(
                                phase, gridItemWithSpan.gridItem()) -
                            spanningTracksSize;
    extraSpace = extraSpace.clampNegativeToZero();
    auto& tracksToGrowBeyondGrowthLimits =
        growBeyondGrowthLimitsTracks.isEmpty() ? filteredTracks
                                               : growBeyondGrowthLimitsTracks;
    distributeSpaceToTracks<phase>(filteredTracks,
                                   &tracksToGrowBeyondGrowthLimits, extraSpace);
  }

  for (const auto& trackIndex : m_contentSizedTracksIndex) {
    GridTrack& track = allTracks[trackIndex];
    markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
    updateTrackSizeForTrackSizeComputationPhase(phase, track);
  }
}

void GridTrackSizingAlgorithm::resolveIntrinsicTrackSizes() {
  Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
  if (m_grid.hasGridItems()) {
    HashSet<LayoutBox*> itemsSet;
    for (const auto& trackIndex : m_contentSizedTracksIndex) {
      GridIterator iterator(m_grid, m_direction, trackIndex);
      GridTrack& track = tracks(m_direction)[trackIndex];
      while (LayoutBox* gridItem = iterator.nextGridItem()) {
        if (itemsSet.insert(gridItem).isNewEntry) {
          const GridSpan& span = m_grid.gridItemSpan(*gridItem, m_direction);
          if (span.integerSpan() == 1) {
            sizeTrackToFitNonSpanningItem(span, *gridItem, track);
          } else if (!spanningItemCrossesFlexibleSizedTracks(span)) {
            itemsSortedByIncreasingSpan.push_back(
                GridItemWithSpan(*gridItem, span));
          }
        }
      }
    }
    std::sort(itemsSortedByIncreasingSpan.begin(),
              itemsSortedByIncreasingSpan.end());
  }

  auto it = itemsSortedByIncreasingSpan.begin();
  auto end = itemsSortedByIncreasingSpan.end();
  while (it != end) {
    GridItemsSpanGroupRange spanGroupRange = {it,
                                              std::upper_bound(it, end, *it)};
    increaseSizesToAccommodateSpanningItems<ResolveIntrinsicMinimums>(
        spanGroupRange);
    increaseSizesToAccommodateSpanningItems<ResolveContentBasedMinimums>(
        spanGroupRange);
    increaseSizesToAccommodateSpanningItems<ResolveMaxContentMinimums>(
        spanGroupRange);
    increaseSizesToAccommodateSpanningItems<ResolveIntrinsicMaximums>(
        spanGroupRange);
    increaseSizesToAccommodateSpanningItems<ResolveMaxContentMaximums>(
        spanGroupRange);
    it = spanGroupRange.rangeEnd;
  }

  for (const auto& trackIndex : m_contentSizedTracksIndex) {
    GridTrack& track = tracks(m_direction)[trackIndex];
    if (track.growthLimit() == infinity)
      track.setGrowthLimit(track.baseSize());
  }
}

void GridTrackSizingAlgorithm::computeGridContainerIntrinsicSizes() {
  m_minContentSize = m_maxContentSize = LayoutUnit();

  Vector<GridTrack>& allTracks = tracks(m_direction);
  for (auto& track : allTracks) {
    DCHECK(!track.infiniteGrowthPotential());
    m_minContentSize += track.baseSize();
    m_maxContentSize += track.growthLimit();
    // The growth limit caps must be cleared now in order to properly sort
    // tracks by growth potential on an eventual "Maximize Tracks".
    track.setGrowthLimitCap(WTF::nullopt);
  }
}

LayoutUnit GridTrackSizingAlgorithm::computeTrackBasedSize() const {
  LayoutUnit size;

  const Vector<GridTrack>& allTracks = tracks(m_direction);
  for (auto& track : allTracks)
    size += track.baseSize();

  size += m_layoutGrid->guttersSize(m_grid, m_direction, 0, allTracks.size(),
                                    m_sizingOperation);

  return size;
}

double GridTrackSizingAlgorithm::findFrUnitSize(
    const GridSpan& tracksSpan,
    LayoutUnit leftOverSpace) const {
  if (leftOverSpace <= 0)
    return 0;

  const Vector<GridTrack>& allTracks = tracks(m_direction);
  double flexFactorSum = 0;
  Vector<size_t, 8> flexibleTracksIndexes;
  for (const auto& trackIndex : tracksSpan) {
    GridTrackSize trackSize = gridTrackSize(m_direction, trackIndex);
    if (!trackSize.maxTrackBreadth().isFlex()) {
      leftOverSpace -= allTracks[trackIndex].baseSize();
    } else {
      flexibleTracksIndexes.push_back(trackIndex);
      flexFactorSum += trackSize.maxTrackBreadth().flex();
    }
  }

  // The function is not called if we don't have <flex> grid tracks.
  DCHECK(!flexibleTracksIndexes.isEmpty());

  return computeFlexFactorUnitSize(allTracks, flexFactorSum, leftOverSpace,
                                   flexibleTracksIndexes);
}

double GridTrackSizingAlgorithm::computeFlexFactorUnitSize(
    const Vector<GridTrack>& tracks,
    double flexFactorSum,
    LayoutUnit& leftOverSpace,
    const Vector<size_t, 8>& flexibleTracksIndexes,
    std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible) const {
  // We want to avoid the effect of flex factors sum below 1 making the factor
  // unit size to grow exponentially.
  double hypotheticalFactorUnitSize =
      leftOverSpace / std::max<double>(1, flexFactorSum);

  // product of the hypothetical "flex factor unit" and any flexible track's
  // "flex factor" must be grater than such track's "base size".
  std::unique_ptr<TrackIndexSet> additionalTracksToTreatAsInflexible =
      std::move(tracksToTreatAsInflexible);
  bool validFlexFactorUnit = true;
  for (auto index : flexibleTracksIndexes) {
    if (additionalTracksToTreatAsInflexible &&
        additionalTracksToTreatAsInflexible->contains(index))
      continue;
    LayoutUnit baseSize = tracks[index].baseSize();
    double flexFactor =
        gridTrackSize(m_direction, index).maxTrackBreadth().flex();
    // treating all such tracks as inflexible.
    if (baseSize > hypotheticalFactorUnitSize * flexFactor) {
      leftOverSpace -= baseSize;
      flexFactorSum -= flexFactor;
      if (!additionalTracksToTreatAsInflexible)
        additionalTracksToTreatAsInflexible = WTF::makeUnique<TrackIndexSet>();
      additionalTracksToTreatAsInflexible->insert(index);
      validFlexFactorUnit = false;
    }
  }
  if (!validFlexFactorUnit) {
    return computeFlexFactorUnitSize(
        tracks, flexFactorSum, leftOverSpace, flexibleTracksIndexes,
        std::move(additionalTracksToTreatAsInflexible));
  }
  return hypotheticalFactorUnitSize;
}

void GridTrackSizingAlgorithm::computeFlexSizedTracksGrowth(
    double flexFraction,
    Vector<LayoutUnit>& increments,
    LayoutUnit& totalGrowth) const {
  size_t numFlexTracks = m_flexibleSizedTracksIndex.size();
  DCHECK_EQ(increments.size(), numFlexTracks);
  const Vector<GridTrack>& allTracks = tracks(m_direction);
  for (size_t i = 0; i < numFlexTracks; ++i) {
    size_t trackIndex = m_flexibleSizedTracksIndex[i];
    auto trackSize = gridTrackSize(m_direction, trackIndex);
    DCHECK(trackSize.maxTrackBreadth().isFlex());
    LayoutUnit oldBaseSize = allTracks[trackIndex].baseSize();
    LayoutUnit newBaseSize =
        std::max(oldBaseSize,
                 LayoutUnit(flexFraction * trackSize.maxTrackBreadth().flex()));
    increments[i] = newBaseSize - oldBaseSize;
    totalGrowth += increments[i];
  }
}

void GridTrackSizingAlgorithm::stretchFlexibleTracks(LayoutUnit freeSpace) {
  double flexFraction = m_strategy->findUsedFlexFraction(
      m_flexibleSizedTracksIndex, m_direction, freeSpace);

  LayoutUnit totalGrowth;
  Vector<LayoutUnit> increments;
  increments.grow(m_flexibleSizedTracksIndex.size());
  computeFlexSizedTracksGrowth(flexFraction, increments, totalGrowth);

  if (m_strategy->recomputeUsedFlexFractionIfNeeded(
          m_flexibleSizedTracksIndex, flexFraction, increments, totalGrowth)) {
    totalGrowth = LayoutUnit(0);
    computeFlexSizedTracksGrowth(flexFraction, increments, totalGrowth);
  }

  size_t i = 0;
  Vector<GridTrack>& allTracks = tracks(m_direction);
  for (auto trackIndex : m_flexibleSizedTracksIndex) {
    auto& track = allTracks[trackIndex];
    if (LayoutUnit increment = increments[i++])
      track.setBaseSize(track.baseSize() + increment);
  }
  this->freeSpace(m_direction) -= totalGrowth;
  m_maxContentSize += totalGrowth;
}

void GridTrackSizingAlgorithm::advanceNextState() {
  switch (m_sizingState) {
    case ColumnSizingFirstIteration:
      m_sizingState = RowSizingFirstIteration;
      return;
    case RowSizingFirstIteration:
      m_sizingState = ColumnSizingSecondIteration;
      return;
    case ColumnSizingSecondIteration:
      m_sizingState = RowSizingSecondIteration;
      return;
    case RowSizingSecondIteration:
      m_sizingState = ColumnSizingFirstIteration;
      return;
  }
  NOTREACHED();
  m_sizingState = ColumnSizingFirstIteration;
}

bool GridTrackSizingAlgorithm::isValidTransition() const {
  switch (m_sizingState) {
    case ColumnSizingFirstIteration:
    case ColumnSizingSecondIteration:
      return m_direction == ForColumns;
    case RowSizingFirstIteration:
    case RowSizingSecondIteration:
      return m_direction == ForRows;
  }
  NOTREACHED();
  return false;
}

void GridTrackSizingAlgorithm::setup(GridTrackSizingDirection direction,
                                     size_t numTracks,
                                     SizingOperation sizingOperation,
                                     LayoutUnit availableSpace,
                                     LayoutUnit freeSpace) {
  DCHECK(m_needsSetup);
  m_direction = direction;
  m_availableSpace = availableSpace;

  m_sizingOperation = sizingOperation;
  switch (m_sizingOperation) {
    case IntrinsicSizeComputation:
      m_strategy = WTF::makeUnique<IndefiniteSizeStrategy>(*this);
      break;
    case TrackSizing:
      m_strategy = WTF::makeUnique<DefiniteSizeStrategy>(*this);
      break;
  }

  m_contentSizedTracksIndex.shrink(0);
  m_flexibleSizedTracksIndex.shrink(0);

  this->freeSpace(direction) = freeSpace;
  tracks(direction).resize(numTracks);

  m_needsSetup = false;
}

// Described in https://drafts.csswg.org/css-grid/#algo-track-sizing
void GridTrackSizingAlgorithm::run() {
  StateMachine stateMachine(*this);

  // Step 1.
  LayoutUnit initialFreeSpace = freeSpace(m_direction);
  initializeTrackSizes();

  // Step 2.
  if (!m_contentSizedTracksIndex.isEmpty())
    resolveIntrinsicTrackSizes();

  // This is not exactly a step of the track sizing algorithm, but we use the
  // track sizes computed
  // up to this moment (before maximization) to calculate the grid container
  // intrinsic sizes.
  computeGridContainerIntrinsicSizes();
  freeSpace(m_direction) -= m_minContentSize;

  if (m_sizingOperation == TrackSizing && freeSpace(m_direction) <= 0)
    return;

  // Step 3.
  m_strategy->maximizeTracks(tracks(m_direction), freeSpace(m_direction));

  if (m_flexibleSizedTracksIndex.isEmpty())
    return;

  // Step 4.
  stretchFlexibleTracks(initialFreeSpace);
}

void GridTrackSizingAlgorithm::reset() {
  m_sizingState = ColumnSizingFirstIteration;
  m_columns.shrink(0);
  m_rows.shrink(0);
  m_contentSizedTracksIndex.shrink(0);
  m_flexibleSizedTracksIndex.shrink(0);
}

#if DCHECK_IS_ON()
bool GridTrackSizingAlgorithm::tracksAreWiderThanMinTrackBreadth() const {
  const Vector<GridTrack>& allTracks = tracks(m_direction);
  for (size_t i = 0; i < allTracks.size(); ++i) {
    GridTrackSize trackSize = gridTrackSize(m_direction, i);
    if (initialBaseSize(trackSize) > allTracks[i].baseSize())
      return false;
  }
  return true;
}
#endif

GridTrackSizingAlgorithm::StateMachine::StateMachine(
    GridTrackSizingAlgorithm& algorithm)
    : m_algorithm(algorithm) {
  DCHECK(m_algorithm.isValidTransition());
  DCHECK(!m_algorithm.m_needsSetup);
}

GridTrackSizingAlgorithm::StateMachine::~StateMachine() {
  m_algorithm.advanceNextState();
  m_algorithm.m_needsSetup = true;
}

}  // namespace blink
