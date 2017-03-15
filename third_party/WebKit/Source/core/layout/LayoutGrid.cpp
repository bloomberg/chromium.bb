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

#include "core/frame/UseCounter.h"
#include "core/layout/LayoutState.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/GridPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "core/style/GridArea.h"
#include "platform/LengthFunctions.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>

namespace blink {

struct ContentAlignmentData {
  STACK_ALLOCATED();

 public:
  ContentAlignmentData(){};
  ContentAlignmentData(LayoutUnit position, LayoutUnit distribution)
      : positionOffset(position), distributionOffset(distribution) {}

  bool isValid() { return positionOffset >= 0 && distributionOffset >= 0; }

  LayoutUnit positionOffset = LayoutUnit(-1);
  LayoutUnit distributionOffset = LayoutUnit(-1);
};

LayoutGrid::LayoutGrid(Element* element)
    : LayoutBlock(element), m_grid(this), m_trackSizingAlgorithm(this, m_grid) {
  ASSERT(!childrenInline());
  if (!isAnonymous())
    UseCounter::count(document(), UseCounter::CSSGridLayout);
}

LayoutGrid::~LayoutGrid() {}

LayoutGrid* LayoutGrid::createAnonymous(Document* document) {
  LayoutGrid* layoutGrid = new LayoutGrid(nullptr);
  layoutGrid->setDocumentForAnonymous(document);
  return layoutGrid;
}

void LayoutGrid::addChild(LayoutObject* newChild, LayoutObject* beforeChild) {
  LayoutBlock::addChild(newChild, beforeChild);

  // Positioned grid items do not take up space or otherwise participate in the
  // layout of the grid, for that reason we don't need to mark the grid as dirty
  // when they are added.
  if (newChild->isOutOfFlowPositioned())
    return;

  // The grid needs to be recomputed as it might contain auto-placed items that
  // will change their position.
  dirtyGrid();
}

void LayoutGrid::removeChild(LayoutObject* child) {
  LayoutBlock::removeChild(child);

  // Positioned grid items do not take up space or otherwise participate in the
  // layout of the grid, for that reason we don't need to mark the grid as dirty
  // when they are removed.
  if (child->isOutOfFlowPositioned())
    return;

  // The grid needs to be recomputed as it might contain auto-placed items that
  // will change their position.
  dirtyGrid();
}

void LayoutGrid::styleDidChange(StyleDifference diff,
                                const ComputedStyle* oldStyle) {
  LayoutBlock::styleDidChange(diff, oldStyle);
  if (!oldStyle)
    return;

  // FIXME: The following checks could be narrowed down if we kept track of
  // which type of grid items we have:
  // - explicit grid size changes impact negative explicitely positioned and
  //   auto-placed grid items.
  // - named grid lines only impact grid items with named grid lines.
  // - auto-flow changes only impacts auto-placed children.

  if (explicitGridDidResize(*oldStyle) ||
      namedGridLinesDefinitionDidChange(*oldStyle) ||
      oldStyle->getGridAutoFlow() != styleRef().getGridAutoFlow() ||
      (diff.needsLayout() && (styleRef().gridAutoRepeatColumns().size() ||
                              styleRef().gridAutoRepeatRows().size())))
    dirtyGrid();
}

bool LayoutGrid::explicitGridDidResize(const ComputedStyle& oldStyle) const {
  return oldStyle.gridTemplateColumns().size() !=
             styleRef().gridTemplateColumns().size() ||
         oldStyle.gridTemplateRows().size() !=
             styleRef().gridTemplateRows().size() ||
         oldStyle.namedGridAreaColumnCount() !=
             styleRef().namedGridAreaColumnCount() ||
         oldStyle.namedGridAreaRowCount() !=
             styleRef().namedGridAreaRowCount() ||
         oldStyle.gridAutoRepeatColumns().size() !=
             styleRef().gridAutoRepeatColumns().size() ||
         oldStyle.gridAutoRepeatRows().size() !=
             styleRef().gridAutoRepeatRows().size();
}

bool LayoutGrid::namedGridLinesDefinitionDidChange(
    const ComputedStyle& oldStyle) const {
  return oldStyle.namedGridRowLines() != styleRef().namedGridRowLines() ||
         oldStyle.namedGridColumnLines() != styleRef().namedGridColumnLines();
}

LayoutUnit LayoutGrid::computeTrackBasedLogicalHeight() const {
  LayoutUnit logicalHeight;

  const Vector<GridTrack>& allRows = m_trackSizingAlgorithm.tracks(ForRows);
  for (const auto& row : allRows)
    logicalHeight += row.baseSize();

  logicalHeight += guttersSize(m_grid, ForRows, 0, allRows.size(), TrackSizing);

  return logicalHeight;
}

void LayoutGrid::computeTrackSizesForDefiniteSize(
    GridTrackSizingDirection direction,
    LayoutUnit availableSpace) {
  LayoutUnit freeSpace =
      availableSpace - guttersSize(m_grid, direction, 0,
                                   m_grid.numTracks(direction), TrackSizing);
  m_trackSizingAlgorithm.setup(direction, numTracks(direction, m_grid),
                               TrackSizing, availableSpace, freeSpace);
  m_trackSizingAlgorithm.run();

#if DCHECK_IS_ON()
  DCHECK(m_trackSizingAlgorithm.tracksAreWiderThanMinTrackBreadth());
#endif
}

void LayoutGrid::repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns,
                                            LayoutUnit availableSpaceForRows) {
  // In orthogonal flow cases column track's size is determined by using the
  // computed row track's size, which it was estimated during the first cycle of
  // the sizing algorithm.
  // Hence we need to repeat computeUsedBreadthOfGridTracks for both, columns
  // and rows, to determine the final values.
  // TODO (lajava): orthogonal flows is just one of the cases which may require
  // a new cycle of the sizing algorithm; there may be more. In addition, not
  // all the cases with orthogonal flows require this extra cycle; we need a
  // more specific condition to detect whether child's min-content contribution
  // has changed or not.
  if (m_grid.hasAnyOrthogonalGridItem()) {
    computeTrackSizesForDefiniteSize(ForColumns, availableSpaceForColumns);
    computeTrackSizesForDefiniteSize(ForRows, availableSpaceForRows);
  }
}

void LayoutGrid::layoutBlock(bool relayoutChildren) {
  ASSERT(needsLayout());

  // We cannot perform a simplifiedLayout() on a dirty grid that
  // has positioned items to be laid out.
  if (!relayoutChildren &&
      (!m_grid.needsItemsPlacement() || !posChildNeedsLayout()) &&
      simplifiedLayout())
    return;

  SubtreeLayoutScope layoutScope(*this);

  {
    // LayoutState needs this deliberate scope to pop before updating scroll
    // information (which may trigger relayout).
    LayoutState state(*this);

    LayoutSize previousSize = size();

    // We need to clear both own and containingBlock override sizes to
    // ensure we get the same result when grid's intrinsic size is
    // computed again in the updateLogicalWidth call bellow.
    if (sizesLogicalWidthToFitContent(styleRef().logicalWidth()) ||
        styleRef().logicalWidth().isIntrinsicOrAuto()) {
      for (auto* child = firstInFlowChildBox(); child;
           child = child->nextInFlowSiblingBox()) {
        if (!isOrthogonalChild(*child))
          continue;
        child->clearOverrideSize();
        child->clearContainingBlockOverrideSize();
        child->forceLayout();
      }
    }

    updateLogicalWidth();
    m_hasDefiniteLogicalHeight = hasDefiniteLogicalHeight();

    TextAutosizer::LayoutScope textAutosizerLayoutScope(this, &layoutScope);

    placeItemsOnGrid(m_grid, TrackSizing);

    // 1- First, the track sizing algorithm is used to resolve the sizes of the
    // grid columns.
    // At this point the logical width is always definite as the above call to
    // updateLogicalWidth() properly resolves intrinsic sizes. We cannot do the
    // same for heights though because many code paths inside
    // updateLogicalHeight() require a previous call to setLogicalHeight() to
    // resolve heights properly (like for positioned items for example).
    LayoutUnit availableSpaceForColumns = availableLogicalWidth();
    computeTrackSizesForDefiniteSize(ForColumns, availableSpaceForColumns);

    // 2- Next, the track sizing algorithm resolves the sizes of the grid rows,
    // using the grid column sizes calculated in the previous step.
    if (cachedHasDefiniteLogicalHeight()) {
      computeTrackSizesForDefiniteSize(
          ForRows, availableLogicalHeight(ExcludeMarginBorderPadding));
    } else {
      computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, ForRows,
                                         m_grid, m_minContentHeight,
                                         m_maxContentHeight);
    }
    LayoutUnit trackBasedLogicalHeight = computeTrackBasedLogicalHeight() +
                                         borderAndPaddingLogicalHeight() +
                                         scrollbarLogicalHeight();
    setLogicalHeight(trackBasedLogicalHeight);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    // Once grid's indefinite height is resolved, we can compute the
    // available free space for Content Alignment.
    if (!cachedHasDefiniteLogicalHeight()) {
      m_trackSizingAlgorithm.freeSpace(ForRows) =
          logicalHeight() - trackBasedLogicalHeight;
    }

    // 3- If the min-content contribution of any grid items have changed based
    // on the row sizes calculated in step 2, steps 1 and 2 are repeated with
    // the new min-content contribution (once only).
    repeatTracksSizingIfNeeded(availableSpaceForColumns,
                               contentLogicalHeight());

    // Grid container should have the minimum height of a line if it's editable.
    // That doesn't affect track sizing though.
    if (hasLineIfEmpty())
      setLogicalHeight(
          std::max(logicalHeight(), minimumLogicalHeightForEmptyLine()));

    applyStretchAlignmentToTracksIfNeeded(ForColumns);
    applyStretchAlignmentToTracksIfNeeded(ForRows);

    layoutGridItems();
    m_trackSizingAlgorithm.reset();

    if (size() != previousSize)
      relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isDocumentElement());

    computeOverflow(oldClientAfterEdge);
  }

  updateLayerTransformAfterLayout();
  updateAfterLayout();

  clearNeedsLayout();
}

LayoutUnit LayoutGrid::gridGapForDirection(
    GridTrackSizingDirection direction,
    SizingOperation sizingOperation) const {
  LayoutUnit availableSize;
  const Length& gap = direction == ForColumns ? styleRef().gridColumnGap()
                                              : styleRef().gridRowGap();
  if (sizingOperation == TrackSizing && gap.isPercent())
    availableSize = direction == ForColumns
                        ? availableLogicalWidth()
                        : availableLogicalHeightForPercentageComputation();

  // TODO(rego): Maybe we could cache the computed percentage as a performance
  // improvement.
  return valueForLength(gap, availableSize);
}

LayoutUnit LayoutGrid::guttersSize(const Grid& grid,
                                   GridTrackSizingDirection direction,
                                   size_t startLine,
                                   size_t span,
                                   SizingOperation sizingOperation) const {
  if (span <= 1)
    return LayoutUnit();

  LayoutUnit gap = gridGapForDirection(direction, sizingOperation);

  // Fast path, no collapsing tracks.
  if (!grid.hasAutoRepeatEmptyTracks(direction))
    return gap * (span - 1);

  // If there are collapsing tracks we need to be sure that gutters are properly
  // collapsed. Apart from that, if we have a collapsed track in the edges of
  // the span we're considering, we need to move forward (or backwards) in order
  // to know whether the collapsed tracks reach the end of the grid (so the gap
  // becomes 0) or there is a non empty track before that.

  LayoutUnit gapAccumulator;
  size_t endLine = startLine + span;

  for (size_t line = startLine; line < endLine - 1; ++line) {
    if (!grid.isEmptyAutoRepeatTrack(direction, line))
      gapAccumulator += gap;
  }

  // The above loop adds one extra gap for trailing collapsed tracks.
  if (gapAccumulator && grid.isEmptyAutoRepeatTrack(direction, endLine - 1)) {
    DCHECK_GE(gapAccumulator, gap);
    gapAccumulator -= gap;
  }

  // If the startLine is the start line of a collapsed track we need to go
  // backwards till we reach a non collapsed track. If we find a non collapsed
  // track we need to add that gap.
  if (startLine && grid.isEmptyAutoRepeatTrack(direction, startLine)) {
    size_t nonEmptyTracksBeforeStartLine = startLine;
    auto begin = grid.autoRepeatEmptyTracks(direction)->begin();
    for (auto it = begin; *it != startLine; ++it) {
      DCHECK(nonEmptyTracksBeforeStartLine);
      --nonEmptyTracksBeforeStartLine;
    }
    if (nonEmptyTracksBeforeStartLine)
      gapAccumulator += gap;
  }

  // If the endLine is the end line of a collapsed track we need to go forward
  // till we reach a non collapsed track. If we find a non collapsed track we
  // need to add that gap.
  if (grid.isEmptyAutoRepeatTrack(direction, endLine - 1)) {
    size_t nonEmptyTracksAfterEndLine = grid.numTracks(direction) - endLine;
    auto currentEmptyTrack =
        grid.autoRepeatEmptyTracks(direction)->find(endLine - 1);
    auto endEmptyTrack = grid.autoRepeatEmptyTracks(direction)->end();
    // HashSet iterators do not implement operator- so we have to manually
    // iterate to know the number of remaining empty tracks.
    for (auto it = ++currentEmptyTrack; it != endEmptyTrack; ++it) {
      DCHECK(nonEmptyTracksAfterEndLine);
      --nonEmptyTracksAfterEndLine;
    }
    if (nonEmptyTracksAfterEndLine)
      gapAccumulator += gap;
  }

  return gapAccumulator;
}

void LayoutGrid::computeIntrinsicLogicalWidths(
    LayoutUnit& minLogicalWidth,
    LayoutUnit& maxLogicalWidth) const {
  Grid grid(this);
  placeItemsOnGrid(grid, IntrinsicSizeComputation);

  GridTrackSizingAlgorithm algorithm(this, grid);
  computeTrackSizesForIndefiniteSize(algorithm, ForColumns, grid,
                                     minLogicalWidth, maxLogicalWidth);

  LayoutUnit scrollbarWidth = LayoutUnit(scrollbarLogicalWidth());
  minLogicalWidth += scrollbarWidth;
  maxLogicalWidth += scrollbarWidth;
}

void LayoutGrid::computeTrackSizesForIndefiniteSize(
    GridTrackSizingAlgorithm& algo,
    GridTrackSizingDirection direction,
    Grid& grid,
    LayoutUnit& minIntrinsicSize,
    LayoutUnit& maxIntrinsicSize) const {
  algo.setup(direction, numTracks(direction, grid), IntrinsicSizeComputation,
             LayoutUnit(), LayoutUnit());
  algo.run();

  minIntrinsicSize = algo.minContentSize();
  maxIntrinsicSize = algo.maxContentSize();

  size_t numberOfTracks = algo.tracks(direction).size();
  LayoutUnit totalGuttersSize =
      guttersSize(grid, direction, 0, numberOfTracks, IntrinsicSizeComputation);
  minIntrinsicSize += totalGuttersSize;
  maxIntrinsicSize += totalGuttersSize;

#if DCHECK_IS_ON()
  DCHECK(algo.tracksAreWiderThanMinTrackBreadth());
#endif
}

LayoutUnit LayoutGrid::computeIntrinsicLogicalContentHeightUsing(
    const Length& logicalHeightLength,
    LayoutUnit intrinsicContentHeight,
    LayoutUnit borderAndPadding) const {
  if (logicalHeightLength.isMinContent())
    return m_minContentHeight;

  if (logicalHeightLength.isMaxContent())
    return m_maxContentHeight;

  if (logicalHeightLength.isFitContent()) {
    if (m_minContentHeight == -1 || m_maxContentHeight == -1)
      return LayoutUnit(-1);
    LayoutUnit fillAvailableExtent =
        containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding);
    return std::min<LayoutUnit>(
        m_maxContentHeight, std::max(m_minContentHeight, fillAvailableExtent));
  }

  if (logicalHeightLength.isFillAvailable())
    return containingBlock()->availableLogicalHeight(
               ExcludeMarginBorderPadding) -
           borderAndPadding;
  ASSERT_NOT_REACHED();
  return LayoutUnit();
}

static LayoutUnit overrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == ForColumns
             ? child.overrideContainingBlockContentLogicalWidth()
             : child.overrideContainingBlockContentLogicalHeight();
}

bool LayoutGrid::isOrthogonalChild(const LayoutBox& child) const {
  return child.isHorizontalWritingMode() != isHorizontalWritingMode();
}

size_t LayoutGrid::computeAutoRepeatTracksCount(
    GridTrackSizingDirection direction,
    SizingOperation sizingOperation) const {
  bool isRowAxis = direction == ForColumns;
  const auto& autoRepeatTracks = isRowAxis ? styleRef().gridAutoRepeatColumns()
                                           : styleRef().gridAutoRepeatRows();
  size_t autoRepeatTrackListLength = autoRepeatTracks.size();

  if (!autoRepeatTrackListLength)
    return 0;

  LayoutUnit availableSize;
  if (isRowAxis) {
    availableSize = sizingOperation == IntrinsicSizeComputation
                        ? LayoutUnit(-1)
                        : availableLogicalWidth();
  } else {
    availableSize = availableLogicalHeightForPercentageComputation();
    if (availableSize == -1) {
      const Length& maxLength = styleRef().logicalMaxHeight();
      if (!maxLength.isMaxSizeNone()) {
        availableSize = constrainContentBoxLogicalHeightByMinMax(
            availableLogicalHeightUsing(maxLength, ExcludeMarginBorderPadding),
            LayoutUnit(-1));
      }
    }
  }

  bool needsToFulfillMinimumSize = false;
  bool indefiniteMainAndMaxSizes = availableSize == LayoutUnit(-1);
  if (indefiniteMainAndMaxSizes) {
    const Length& minSize = isRowAxis ? styleRef().logicalMinWidth()
                                      : styleRef().logicalMinHeight();
    if (!minSize.isSpecified())
      return autoRepeatTrackListLength;

    LayoutUnit containingBlockAvailableSize =
        isRowAxis ? containingBlockLogicalWidthForContent()
                  : containingBlockLogicalHeightForContent(
                        ExcludeMarginBorderPadding);
    availableSize = valueForLength(minSize, containingBlockAvailableSize);
    needsToFulfillMinimumSize = true;
  }

  LayoutUnit autoRepeatTracksSize;
  for (auto autoTrackSize : autoRepeatTracks) {
    DCHECK(autoTrackSize.minTrackBreadth().isLength());
    DCHECK(!autoTrackSize.minTrackBreadth().isFlex());
    bool hasDefiniteMaxTrackSizingFunction =
        autoTrackSize.maxTrackBreadth().isLength() &&
        !autoTrackSize.maxTrackBreadth().isContentSized();
    auto trackLength = hasDefiniteMaxTrackSizingFunction
                           ? autoTrackSize.maxTrackBreadth().length()
                           : autoTrackSize.minTrackBreadth().length();
    autoRepeatTracksSize += valueForLength(trackLength, availableSize);
  }
  // For the purpose of finding the number of auto-repeated tracks, the UA must
  // floor the track size to a UA-specified value to avoid division by zero. It
  // is suggested that this floor be 1px.
  autoRepeatTracksSize =
      std::max<LayoutUnit>(LayoutUnit(1), autoRepeatTracksSize);

  // There will be always at least 1 auto-repeat track, so take it already into
  // account when computing the total track size.
  LayoutUnit tracksSize = autoRepeatTracksSize;
  const Vector<GridTrackSize>& trackSizes =
      isRowAxis ? styleRef().gridTemplateColumns()
                : styleRef().gridTemplateRows();

  for (const auto& track : trackSizes) {
    bool hasDefiniteMaxTrackBreadth = track.maxTrackBreadth().isLength() &&
                                      !track.maxTrackBreadth().isContentSized();
    DCHECK(hasDefiniteMaxTrackBreadth ||
           (track.minTrackBreadth().isLength() &&
            !track.minTrackBreadth().isContentSized()));
    tracksSize += valueForLength(hasDefiniteMaxTrackBreadth
                                     ? track.maxTrackBreadth().length()
                                     : track.minTrackBreadth().length(),
                                 availableSize);
  }

  // Add gutters as if there where only 1 auto repeat track. Gaps between auto
  // repeat tracks will be added later when computing the repetitions.
  LayoutUnit gapSize = gridGapForDirection(direction, sizingOperation);
  tracksSize += gapSize * trackSizes.size();

  LayoutUnit freeSpace = availableSize - tracksSize;
  if (freeSpace <= 0)
    return autoRepeatTrackListLength;

  size_t repetitions =
      1 + (freeSpace / (autoRepeatTracksSize + gapSize)).toInt();

  // Provided the grid container does not have a definite size or max-size in
  // the relevant axis, if the min size is definite then the number of
  // repetitions is the largest possible positive integer that fulfills that
  // minimum requirement.
  if (needsToFulfillMinimumSize)
    ++repetitions;

  return repetitions * autoRepeatTrackListLength;
}

std::unique_ptr<OrderedTrackIndexSet>
LayoutGrid::computeEmptyTracksForAutoRepeat(
    Grid& grid,
    GridTrackSizingDirection direction) const {
  bool isRowAxis = direction == ForColumns;
  if ((isRowAxis && styleRef().gridAutoRepeatColumnsType() != AutoFit) ||
      (!isRowAxis && styleRef().gridAutoRepeatRowsType() != AutoFit))
    return nullptr;

  std::unique_ptr<OrderedTrackIndexSet> emptyTrackIndexes;
  size_t insertionPoint = isRowAxis
                              ? styleRef().gridAutoRepeatColumnsInsertionPoint()
                              : styleRef().gridAutoRepeatRowsInsertionPoint();
  size_t firstAutoRepeatTrack =
      insertionPoint + std::abs(grid.smallestTrackStart(direction));
  size_t lastAutoRepeatTrack =
      firstAutoRepeatTrack + grid.autoRepeatTracks(direction);

  if (!grid.hasGridItems()) {
    emptyTrackIndexes = WTF::wrapUnique(new OrderedTrackIndexSet);
    for (size_t trackIndex = firstAutoRepeatTrack;
         trackIndex < lastAutoRepeatTrack; ++trackIndex)
      emptyTrackIndexes->insert(trackIndex);
  } else {
    for (size_t trackIndex = firstAutoRepeatTrack;
         trackIndex < lastAutoRepeatTrack; ++trackIndex) {
      GridIterator iterator(grid, direction, trackIndex);
      if (!iterator.nextGridItem()) {
        if (!emptyTrackIndexes)
          emptyTrackIndexes = WTF::wrapUnique(new OrderedTrackIndexSet);
        emptyTrackIndexes->insert(trackIndex);
      }
    }
  }
  return emptyTrackIndexes;
}

size_t LayoutGrid::clampAutoRepeatTracks(GridTrackSizingDirection direction,
                                         size_t autoRepeatTracks) const {
  if (!autoRepeatTracks)
    return 0;

  size_t insertionPoint = direction == ForColumns
                              ? styleRef().gridAutoRepeatColumnsInsertionPoint()
                              : styleRef().gridAutoRepeatRowsInsertionPoint();

  if (insertionPoint == 0)
    return std::min<size_t>(autoRepeatTracks, kGridMaxTracks);

  if (insertionPoint >= kGridMaxTracks)
    return 0;

  return std::min(autoRepeatTracks,
                  static_cast<size_t>(kGridMaxTracks) - insertionPoint);
}

void LayoutGrid::placeItemsOnGrid(Grid& grid,
                                  SizingOperation sizingOperation) const {
  size_t autoRepeatRows =
      computeAutoRepeatTracksCount(ForRows, sizingOperation);
  size_t autoRepeatColumns =
      computeAutoRepeatTracksCount(ForColumns, sizingOperation);

  autoRepeatRows = clampAutoRepeatTracks(ForRows, autoRepeatRows);
  autoRepeatColumns = clampAutoRepeatTracks(ForColumns, autoRepeatColumns);

  if (autoRepeatRows != grid.autoRepeatTracks(ForRows) ||
      autoRepeatColumns != grid.autoRepeatTracks(ForColumns)) {
    grid.setNeedsItemsPlacement(true);
    grid.setAutoRepeatTracks(autoRepeatRows, autoRepeatColumns);
  }

  if (!grid.needsItemsPlacement())
    return;

  DCHECK(!grid.hasGridItems());
  populateExplicitGridAndOrderIterator(grid);

  Vector<LayoutBox*> autoMajorAxisAutoGridItems;
  Vector<LayoutBox*> specifiedMajorAxisAutoGridItems;
#if DCHECK_IS_ON()
  DCHECK(!grid.hasAnyGridItemPaintOrder());
#endif
  DCHECK(!grid.hasAnyOrthogonalGridItem());
  bool hasAnyOrthogonalGridItem = false;
  size_t childIndex = 0;
  for (LayoutBox* child = grid.orderIterator().first(); child;
       child = grid.orderIterator().next()) {
    if (child->isOutOfFlowPositioned())
      continue;

    hasAnyOrthogonalGridItem =
        hasAnyOrthogonalGridItem || isOrthogonalChild(*child);
    grid.setGridItemPaintOrder(*child, childIndex++);

    GridArea area = grid.gridItemArea(*child);
    if (!area.rows.isIndefinite())
      area.rows.translate(abs(grid.smallestTrackStart(ForRows)));
    if (!area.columns.isIndefinite())
      area.columns.translate(abs(grid.smallestTrackStart(ForColumns)));

    if (area.rows.isIndefinite() || area.columns.isIndefinite()) {
      grid.setGridItemArea(*child, area);
      GridSpan majorAxisPositions =
          (autoPlacementMajorAxisDirection() == ForColumns) ? area.columns
                                                            : area.rows;
      if (majorAxisPositions.isIndefinite())
        autoMajorAxisAutoGridItems.push_back(child);
      else
        specifiedMajorAxisAutoGridItems.push_back(child);
      continue;
    }
    grid.insert(*child, area);
  }
  grid.setHasAnyOrthogonalGridItem(hasAnyOrthogonalGridItem);

#if DCHECK_IS_ON()
  if (grid.hasGridItems()) {
    DCHECK_GE(grid.numTracks(ForRows),
              GridPositionsResolver::explicitGridRowCount(
                  *style(), grid.autoRepeatTracks(ForRows)));
    DCHECK_GE(grid.numTracks(ForColumns),
              GridPositionsResolver::explicitGridColumnCount(
                  *style(), grid.autoRepeatTracks(ForColumns)));
  }
#endif

  placeSpecifiedMajorAxisItemsOnGrid(grid, specifiedMajorAxisAutoGridItems);
  placeAutoMajorAxisItemsOnGrid(grid, autoMajorAxisAutoGridItems);

  // Compute collapsable tracks for auto-fit.
  grid.setAutoRepeatEmptyColumns(
      computeEmptyTracksForAutoRepeat(grid, ForColumns));
  grid.setAutoRepeatEmptyRows(computeEmptyTracksForAutoRepeat(grid, ForRows));

  grid.setNeedsItemsPlacement(false);

#if DCHECK_IS_ON()
  for (LayoutBox* child = grid.orderIterator().first(); child;
       child = grid.orderIterator().next()) {
    if (child->isOutOfFlowPositioned())
      continue;

    GridArea area = grid.gridItemArea(*child);
    ASSERT(area.rows.isTranslatedDefinite() &&
           area.columns.isTranslatedDefinite());
  }
#endif
}

void LayoutGrid::populateExplicitGridAndOrderIterator(Grid& grid) const {
  OrderIteratorPopulator populator(grid.orderIterator());
  int smallestRowStart = 0;
  int smallestColumnStart = 0;

  size_t autoRepeatRows = grid.autoRepeatTracks(ForRows);
  size_t autoRepeatColumns = grid.autoRepeatTracks(ForColumns);
  size_t maximumRowIndex =
      GridPositionsResolver::explicitGridRowCount(*style(), autoRepeatRows);
  size_t maximumColumnIndex = GridPositionsResolver::explicitGridColumnCount(
      *style(), autoRepeatColumns);

  for (LayoutBox* child = firstInFlowChildBox(); child;
       child = child->nextInFlowSiblingBox()) {
    populator.collectChild(child);

    // This function bypasses the cache (gridItemArea()) as it is used to
    // build it.
    GridSpan rowPositions =
        GridPositionsResolver::resolveGridPositionsFromStyle(
            *style(), *child, ForRows, autoRepeatRows);
    GridSpan columnPositions =
        GridPositionsResolver::resolveGridPositionsFromStyle(
            *style(), *child, ForColumns, autoRepeatColumns);
    grid.setGridItemArea(*child, GridArea(rowPositions, columnPositions));

    // |positions| is 0 if we need to run the auto-placement algorithm.
    if (!rowPositions.isIndefinite()) {
      smallestRowStart =
          std::min(smallestRowStart, rowPositions.untranslatedStartLine());
      maximumRowIndex =
          std::max<int>(maximumRowIndex, rowPositions.untranslatedEndLine());
    } else {
      // Grow the grid for items with a definite row span, getting the largest
      // such span.
      size_t spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(
          *style(), *child, ForRows);
      maximumRowIndex = std::max(maximumRowIndex, spanSize);
    }

    if (!columnPositions.isIndefinite()) {
      smallestColumnStart = std::min(smallestColumnStart,
                                     columnPositions.untranslatedStartLine());
      maximumColumnIndex = std::max<int>(maximumColumnIndex,
                                         columnPositions.untranslatedEndLine());
    } else {
      // Grow the grid for items with a definite column span, getting the
      // largest such span.
      size_t spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(
          *style(), *child, ForColumns);
      maximumColumnIndex = std::max(maximumColumnIndex, spanSize);
    }
  }

  grid.setSmallestTracksStart(smallestRowStart, smallestColumnStart);
  grid.ensureGridSize(maximumRowIndex + abs(smallestRowStart),
                      maximumColumnIndex + abs(smallestColumnStart));
}

std::unique_ptr<GridArea>
LayoutGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
    const Grid& grid,
    const LayoutBox& gridItem,
    GridTrackSizingDirection specifiedDirection,
    const GridSpan& specifiedPositions) const {
  GridTrackSizingDirection crossDirection =
      specifiedDirection == ForColumns ? ForRows : ForColumns;
  const size_t endOfCrossDirection = grid.numTracks(crossDirection);
  size_t crossDirectionSpanSize =
      GridPositionsResolver::spanSizeForAutoPlacedItem(*style(), gridItem,
                                                       crossDirection);
  GridSpan crossDirectionPositions = GridSpan::translatedDefiniteGridSpan(
      endOfCrossDirection, endOfCrossDirection + crossDirectionSpanSize);
  return WTF::wrapUnique(
      new GridArea(specifiedDirection == ForColumns ? crossDirectionPositions
                                                    : specifiedPositions,
                   specifiedDirection == ForColumns ? specifiedPositions
                                                    : crossDirectionPositions));
}

void LayoutGrid::placeSpecifiedMajorAxisItemsOnGrid(
    Grid& grid,
    const Vector<LayoutBox*>& autoGridItems) const {
  bool isForColumns = autoPlacementMajorAxisDirection() == ForColumns;
  bool isGridAutoFlowDense = style()->isGridAutoFlowAlgorithmDense();

  // Mapping between the major axis tracks (rows or columns) and the last
  // auto-placed item's position inserted on that track. This is needed to
  // implement "sparse" packing for items locked to a given track.
  // See http://dev.w3.org/csswg/css-grid/#auto-placement-algo
  HashMap<unsigned, unsigned, DefaultHash<unsigned>::Hash,
          WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      minorAxisCursors;

  for (const auto& autoGridItem : autoGridItems) {
    GridSpan majorAxisPositions =
        grid.gridItemSpan(*autoGridItem, autoPlacementMajorAxisDirection());
    ASSERT(majorAxisPositions.isTranslatedDefinite());
    DCHECK(!grid.gridItemSpan(*autoGridItem, autoPlacementMinorAxisDirection())
                .isTranslatedDefinite());
    size_t minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(
        *style(), *autoGridItem, autoPlacementMinorAxisDirection());
    unsigned majorAxisInitialPosition = majorAxisPositions.startLine();

    GridIterator iterator(
        grid, autoPlacementMajorAxisDirection(), majorAxisPositions.startLine(),
        isGridAutoFlowDense ? 0
                            : minorAxisCursors.at(majorAxisInitialPosition));
    std::unique_ptr<GridArea> emptyGridArea = iterator.nextEmptyGridArea(
        majorAxisPositions.integerSpan(), minorAxisSpanSize);
    if (!emptyGridArea) {
      emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, *autoGridItem, autoPlacementMajorAxisDirection(),
          majorAxisPositions);
    }

    grid.insert(*autoGridItem, *emptyGridArea);

    if (!isGridAutoFlowDense)
      minorAxisCursors.set(majorAxisInitialPosition,
                           isForColumns ? emptyGridArea->rows.startLine()
                                        : emptyGridArea->columns.startLine());
  }
}

void LayoutGrid::placeAutoMajorAxisItemsOnGrid(
    Grid& grid,
    const Vector<LayoutBox*>& autoGridItems) const {
  std::pair<size_t, size_t> autoPlacementCursor = std::make_pair(0, 0);
  bool isGridAutoFlowDense = style()->isGridAutoFlowAlgorithmDense();

  for (const auto& autoGridItem : autoGridItems) {
    placeAutoMajorAxisItemOnGrid(grid, *autoGridItem, autoPlacementCursor);

    // If grid-auto-flow is dense, reset auto-placement cursor.
    if (isGridAutoFlowDense) {
      autoPlacementCursor.first = 0;
      autoPlacementCursor.second = 0;
    }
  }
}

void LayoutGrid::placeAutoMajorAxisItemOnGrid(
    Grid& grid,
    LayoutBox& gridItem,
    std::pair<size_t, size_t>& autoPlacementCursor) const {
  GridSpan minorAxisPositions =
      grid.gridItemSpan(gridItem, autoPlacementMinorAxisDirection());
  DCHECK(!grid.gridItemSpan(gridItem, autoPlacementMajorAxisDirection())
              .isTranslatedDefinite());
  size_t majorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(
      *style(), gridItem, autoPlacementMajorAxisDirection());

  const size_t endOfMajorAxis =
      grid.numTracks(autoPlacementMajorAxisDirection());
  size_t majorAxisAutoPlacementCursor =
      autoPlacementMajorAxisDirection() == ForColumns
          ? autoPlacementCursor.second
          : autoPlacementCursor.first;
  size_t minorAxisAutoPlacementCursor =
      autoPlacementMajorAxisDirection() == ForColumns
          ? autoPlacementCursor.first
          : autoPlacementCursor.second;

  std::unique_ptr<GridArea> emptyGridArea;
  if (minorAxisPositions.isTranslatedDefinite()) {
    // Move to the next track in major axis if initial position in minor axis is
    // before auto-placement cursor.
    if (minorAxisPositions.startLine() < minorAxisAutoPlacementCursor)
      majorAxisAutoPlacementCursor++;

    if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
      GridIterator iterator(grid, autoPlacementMinorAxisDirection(),
                            minorAxisPositions.startLine(),
                            majorAxisAutoPlacementCursor);
      emptyGridArea = iterator.nextEmptyGridArea(
          minorAxisPositions.integerSpan(), majorAxisSpanSize);
    }

    if (!emptyGridArea) {
      emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, gridItem, autoPlacementMinorAxisDirection(),
          minorAxisPositions);
    }
  } else {
    size_t minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(
        *style(), gridItem, autoPlacementMinorAxisDirection());

    for (size_t majorAxisIndex = majorAxisAutoPlacementCursor;
         majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
      GridIterator iterator(grid, autoPlacementMajorAxisDirection(),
                            majorAxisIndex, minorAxisAutoPlacementCursor);
      emptyGridArea =
          iterator.nextEmptyGridArea(majorAxisSpanSize, minorAxisSpanSize);

      if (emptyGridArea) {
        // Check that it fits in the minor axis direction, as we shouldn't grow
        // in that direction here (it was already managed in
        // populateExplicitGridAndOrderIterator()).
        size_t minorAxisFinalPositionIndex =
            autoPlacementMinorAxisDirection() == ForColumns
                ? emptyGridArea->columns.endLine()
                : emptyGridArea->rows.endLine();
        const size_t endOfMinorAxis =
            grid.numTracks(autoPlacementMinorAxisDirection());
        if (minorAxisFinalPositionIndex <= endOfMinorAxis)
          break;

        // Discard empty grid area as it does not fit in the minor axis
        // direction. We don't need to create a new empty grid area yet as we
        // might find a valid one in the next iteration.
        emptyGridArea = nullptr;
      }

      // As we're moving to the next track in the major axis we should reset the
      // auto-placement cursor in the minor axis.
      minorAxisAutoPlacementCursor = 0;
    }

    if (!emptyGridArea)
      emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, gridItem, autoPlacementMinorAxisDirection(),
          GridSpan::translatedDefiniteGridSpan(0, minorAxisSpanSize));
  }

  grid.insert(gridItem, *emptyGridArea);
  // Move auto-placement cursor to the new position.
  autoPlacementCursor.first = emptyGridArea->rows.startLine();
  autoPlacementCursor.second = emptyGridArea->columns.startLine();
}

GridTrackSizingDirection LayoutGrid::autoPlacementMajorAxisDirection() const {
  return style()->isGridAutoFlowDirectionColumn() ? ForColumns : ForRows;
}

GridTrackSizingDirection LayoutGrid::autoPlacementMinorAxisDirection() const {
  return style()->isGridAutoFlowDirectionColumn() ? ForRows : ForColumns;
}

void LayoutGrid::dirtyGrid() {
  if (m_grid.needsItemsPlacement())
    return;

  m_grid.setNeedsItemsPlacement(true);
  m_gridItemsOverflowingGridArea.resize(0);
}

Vector<LayoutUnit> LayoutGrid::trackSizesForComputedStyle(
    GridTrackSizingDirection direction) const {
  bool isRowAxis = direction == ForColumns;
  auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
  size_t numPositions = positions.size();
  LayoutUnit offsetBetweenTracks =
      isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;

  Vector<LayoutUnit> tracks;
  if (numPositions < 2)
    return tracks;

  DCHECK(!m_grid.needsItemsPlacement());
  bool hasCollapsedTracks = m_grid.hasAutoRepeatEmptyTracks(direction);
  LayoutUnit gap = !hasCollapsedTracks
                       ? gridGapForDirection(direction, TrackSizing)
                       : LayoutUnit();
  tracks.reserveCapacity(numPositions - 1);
  for (size_t i = 0; i < numPositions - 2; ++i)
    tracks.push_back(positions[i + 1] - positions[i] - offsetBetweenTracks -
                     gap);
  tracks.push_back(positions[numPositions - 1] - positions[numPositions - 2]);

  if (!hasCollapsedTracks)
    return tracks;

  size_t remainingEmptyTracks = m_grid.autoRepeatEmptyTracks(direction)->size();
  size_t lastLine = tracks.size();
  gap = gridGapForDirection(direction, TrackSizing);
  for (size_t i = 1; i < lastLine; ++i) {
    if (m_grid.isEmptyAutoRepeatTrack(direction, i - 1)) {
      --remainingEmptyTracks;
    } else {
      // Remove the gap between consecutive non empty tracks. Remove it also
      // just once for an arbitrary number of empty tracks between two non empty
      // ones.
      bool allRemainingTracksAreEmpty = remainingEmptyTracks == (lastLine - i);
      if (!allRemainingTracksAreEmpty ||
          !m_grid.isEmptyAutoRepeatTrack(direction, i))
        tracks[i - 1] -= gap;
    }
  }

  return tracks;
}

static const StyleContentAlignmentData& contentAlignmentNormalBehavior() {
  static const StyleContentAlignmentData normalBehavior = {
      ContentPositionNormal, ContentDistributionStretch};
  return normalBehavior;
}

void LayoutGrid::applyStretchAlignmentToTracksIfNeeded(
    GridTrackSizingDirection direction) {
  LayoutUnit& availableSpace = m_trackSizingAlgorithm.freeSpace(direction);
  if (availableSpace <= 0 ||
      (direction == ForColumns &&
       styleRef().resolvedJustifyContentDistribution(
           contentAlignmentNormalBehavior()) != ContentDistributionStretch) ||
      (direction == ForRows &&
       styleRef().resolvedAlignContentDistribution(
           contentAlignmentNormalBehavior()) != ContentDistributionStretch))
    return;

  // Spec defines auto-sized tracks as the ones with an 'auto' max-sizing
  // function.
  Vector<GridTrack>& allTracks = m_trackSizingAlgorithm.tracks(direction);
  Vector<unsigned> autoSizedTracksIndex;
  for (unsigned i = 0; i < allTracks.size(); ++i) {
    const GridTrackSize& trackSize =
        m_trackSizingAlgorithm.gridTrackSize(direction, i, TrackSizing);
    if (trackSize.hasAutoMaxTrackBreadth())
      autoSizedTracksIndex.push_back(i);
  }

  unsigned numberOfAutoSizedTracks = autoSizedTracksIndex.size();
  if (numberOfAutoSizedTracks < 1)
    return;

  LayoutUnit sizeToIncrease = availableSpace / numberOfAutoSizedTracks;
  for (const auto& trackIndex : autoSizedTracksIndex) {
    GridTrack* track = allTracks.data() + trackIndex;
    LayoutUnit baseSize = track->baseSize() + sizeToIncrease;
    track->setBaseSize(baseSize);
  }

  availableSpace = LayoutUnit();
}

void LayoutGrid::layoutGridItems() {
  populateGridPositionsForDirection(ForColumns);
  populateGridPositionsForDirection(ForRows);
  m_gridItemsOverflowingGridArea.resize(0);

  for (LayoutBox* child = firstChildBox(); child;
       child = child->nextSiblingBox()) {
    if (child->isOutOfFlowPositioned()) {
      prepareChildForPositionedLayout(*child);
      continue;
    }

    // Because the grid area cannot be styled, we don't need to adjust
    // the grid breadth to account for 'box-sizing'.
    LayoutUnit oldOverrideContainingBlockContentLogicalWidth =
        child->hasOverrideContainingBlockLogicalWidth()
            ? child->overrideContainingBlockContentLogicalWidth()
            : LayoutUnit();
    LayoutUnit oldOverrideContainingBlockContentLogicalHeight =
        child->hasOverrideContainingBlockLogicalHeight()
            ? child->overrideContainingBlockContentLogicalHeight()
            : LayoutUnit();

    LayoutUnit overrideContainingBlockContentLogicalWidth =
        gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForColumns);
    LayoutUnit overrideContainingBlockContentLogicalHeight =
        gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForRows);

    if (oldOverrideContainingBlockContentLogicalWidth !=
            overrideContainingBlockContentLogicalWidth ||
        (oldOverrideContainingBlockContentLogicalHeight !=
             overrideContainingBlockContentLogicalHeight &&
         child->hasRelativeLogicalHeight()))
      child->setNeedsLayout(LayoutInvalidationReason::GridChanged);

    child->setOverrideContainingBlockContentLogicalWidth(
        overrideContainingBlockContentLogicalWidth);
    child->setOverrideContainingBlockContentLogicalHeight(
        overrideContainingBlockContentLogicalHeight);

    // Stretching logic might force a child layout, so we need to run it before
    // the layoutIfNeeded call to avoid unnecessary relayouts. This might imply
    // that child margins, needed to correctly determine the available space
    // before stretching, are not set yet.
    applyStretchAlignmentToChildIfNeeded(*child);

    child->layoutIfNeeded();

    // We need pending layouts to be done in order to compute auto-margins
    // properly.
    updateAutoMarginsInColumnAxisIfNeeded(*child);
    updateAutoMarginsInRowAxisIfNeeded(*child);

    const GridArea& area = m_grid.gridItemArea(*child);
#if DCHECK_IS_ON()
    DCHECK(area.columns.startLine() <
           m_trackSizingAlgorithm.tracks(ForColumns).size());
    DCHECK(area.rows.startLine() <
           m_trackSizingAlgorithm.tracks(ForRows).size());
#endif
    child->setLogicalLocation(findChildLogicalPosition(*child));

    // Keep track of children overflowing their grid area as we might need to
    // paint them even if the grid-area is not visible. Using physical
    // dimensions for simplicity, so we can forget about orthogonalty.
    LayoutUnit childGridAreaHeight =
        isHorizontalWritingMode() ? overrideContainingBlockContentLogicalHeight
                                  : overrideContainingBlockContentLogicalWidth;
    LayoutUnit childGridAreaWidth =
        isHorizontalWritingMode() ? overrideContainingBlockContentLogicalWidth
                                  : overrideContainingBlockContentLogicalHeight;
    LayoutRect gridAreaRect(
        gridAreaLogicalPosition(area),
        LayoutSize(childGridAreaWidth, childGridAreaHeight));
    if (!gridAreaRect.contains(child->frameRect()))
      m_gridItemsOverflowingGridArea.push_back(child);
  }
}

void LayoutGrid::prepareChildForPositionedLayout(LayoutBox& child) {
  ASSERT(child.isOutOfFlowPositioned());
  child.containingBlock()->insertPositionedObject(&child);

  PaintLayer* childLayer = child.layer();
  childLayer->setStaticInlinePosition(LayoutUnit(borderStart()));
  childLayer->setStaticBlockPosition(LayoutUnit(borderBefore()));
}

void LayoutGrid::layoutPositionedObjects(bool relayoutChildren,
                                         PositionedLayoutBehavior info) {
  TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
  if (!positionedDescendants)
    return;

  for (auto* child : *positionedDescendants) {
    if (isOrthogonalChild(*child)) {
      // FIXME: Properly support orthogonal writing mode.
      layoutPositionedObject(child, relayoutChildren, info);
      continue;
    }

    LayoutUnit columnOffset = LayoutUnit();
    LayoutUnit columnBreadth = LayoutUnit();
    offsetAndBreadthForPositionedChild(*child, ForColumns, columnOffset,
                                       columnBreadth);
    LayoutUnit rowOffset = LayoutUnit();
    LayoutUnit rowBreadth = LayoutUnit();
    offsetAndBreadthForPositionedChild(*child, ForRows, rowOffset, rowBreadth);

    child->setOverrideContainingBlockContentLogicalWidth(columnBreadth);
    child->setOverrideContainingBlockContentLogicalHeight(rowBreadth);
    child->setExtraInlineOffset(columnOffset);
    child->setExtraBlockOffset(rowOffset);

    if (child->parent() == this) {
      PaintLayer* childLayer = child->layer();
      childLayer->setStaticInlinePosition(borderStart() + columnOffset);
      childLayer->setStaticBlockPosition(borderBefore() + rowOffset);
    }

    layoutPositionedObject(child, relayoutChildren, info);
  }
}

void LayoutGrid::offsetAndBreadthForPositionedChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction,
    LayoutUnit& offset,
    LayoutUnit& breadth) {
  ASSERT(!isOrthogonalChild(child));
  bool isForColumns = direction == ForColumns;

  GridSpan positions = GridPositionsResolver::resolveGridPositionsFromStyle(
      *style(), child, direction, autoRepeatCountForDirection(direction));
  if (positions.isIndefinite()) {
    offset = LayoutUnit();
    breadth = isForColumns ? clientLogicalWidth() : clientLogicalHeight();
    return;
  }

  // For positioned items we cannot use GridSpan::translate(). Because we could
  // end up with negative values, as the positioned items do not create implicit
  // tracks per spec.
  int smallestStart = abs(m_grid.smallestTrackStart(direction));
  int startLine = positions.untranslatedStartLine() + smallestStart;
  int endLine = positions.untranslatedEndLine() + smallestStart;

  GridPosition startPosition = isForColumns ? child.style()->gridColumnStart()
                                            : child.style()->gridRowStart();
  GridPosition endPosition = isForColumns ? child.style()->gridColumnEnd()
                                          : child.style()->gridRowEnd();
  int lastLine = numTracks(direction, m_grid);

  bool startIsAuto =
      startPosition.isAuto() ||
      (startPosition.isNamedGridArea() &&
       !NamedLineCollection::isValidNamedLineOrArea(
           startPosition.namedGridLine(), styleRef(),
           GridPositionsResolver::initialPositionSide(direction))) ||
      (startLine < 0) || (startLine > lastLine);
  bool endIsAuto = endPosition.isAuto() ||
                   (endPosition.isNamedGridArea() &&
                    !NamedLineCollection::isValidNamedLineOrArea(
                        endPosition.namedGridLine(), styleRef(),
                        GridPositionsResolver::finalPositionSide(direction))) ||
                   (endLine < 0) || (endLine > lastLine);

  LayoutUnit start;
  if (!startIsAuto) {
    if (isForColumns) {
      if (styleRef().isLeftToRightDirection())
        start = m_columnPositions[startLine] - borderLogicalLeft();
      else
        start = logicalWidth() -
                translateRTLCoordinate(m_columnPositions[startLine]) -
                borderLogicalRight();
    } else {
      start = m_rowPositions[startLine] - borderBefore();
    }
  }

  LayoutUnit end = isForColumns ? clientLogicalWidth() : clientLogicalHeight();
  if (!endIsAuto) {
    if (isForColumns) {
      if (styleRef().isLeftToRightDirection())
        end = m_columnPositions[endLine] - borderLogicalLeft();
      else
        end = logicalWidth() -
              translateRTLCoordinate(m_columnPositions[endLine]) -
              borderLogicalRight();
    } else {
      end = m_rowPositions[endLine] - borderBefore();
    }

    // These vectors store line positions including gaps, but we shouldn't
    // consider them for the edges of the grid.
    if (endLine > 0 && endLine < lastLine) {
      DCHECK(!m_grid.needsItemsPlacement());
      end -= guttersSize(m_grid, direction, endLine - 1, 2, TrackSizing);
      end -= isForColumns ? m_offsetBetweenColumns : m_offsetBetweenRows;
    }
  }

  breadth = std::max(end - start, LayoutUnit());
  offset = start;

  if (isForColumns && !styleRef().isLeftToRightDirection() &&
      !child.styleRef().hasStaticInlinePosition(
          child.isHorizontalWritingMode())) {
    // If the child doesn't have a static inline position (i.e. "left" and/or
    // "right" aren't "auto", we need to calculate the offset from the left
    // (even if we're in RTL).
    if (endIsAuto) {
      offset = LayoutUnit();
    } else {
      offset = translateRTLCoordinate(m_columnPositions[endLine]) -
               borderLogicalLeft();

      if (endLine > 0 && endLine < lastLine) {
        DCHECK(!m_grid.needsItemsPlacement());
        offset += guttersSize(m_grid, direction, endLine - 1, 2, TrackSizing);
        offset += isForColumns ? m_offsetBetweenColumns : m_offsetBetweenRows;
      }
    }
  }
}

LayoutUnit LayoutGrid::gridAreaBreadthForChildIncludingAlignmentOffsets(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  // We need the cached value when available because Content Distribution
  // alignment properties may have some influence in the final grid area
  // breadth.
  const Vector<GridTrack>& tracks = m_trackSizingAlgorithm.tracks(direction);
  const GridSpan& span =
      m_trackSizingAlgorithm.grid().gridItemSpan(child, direction);
  const Vector<LayoutUnit>& linePositions =
      (direction == ForColumns) ? m_columnPositions : m_rowPositions;
  LayoutUnit initialTrackPosition = linePositions[span.startLine()];
  LayoutUnit finalTrackPosition = linePositions[span.endLine() - 1];
  // Track Positions vector stores the 'start' grid line of each track, so we
  // have to add last track's baseSize.
  return finalTrackPosition - initialTrackPosition +
         tracks[span.endLine() - 1].baseSize();
}

void LayoutGrid::populateGridPositionsForDirection(
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
  bool isRowAxis = direction == ForColumns;
  auto& tracks = m_trackSizingAlgorithm.tracks(direction);
  size_t numberOfTracks = tracks.size();
  size_t numberOfLines = numberOfTracks + 1;
  size_t lastLine = numberOfLines - 1;
  ContentAlignmentData offset = computeContentPositionAndDistributionOffset(
      direction, m_trackSizingAlgorithm.freeSpace(direction), numberOfTracks);
  auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
  positions.resize(numberOfLines);
  auto borderAndPadding =
      isRowAxis ? borderAndPaddingLogicalLeft() : borderAndPaddingBefore();
  positions[0] = borderAndPadding + offset.positionOffset;
  const Grid& grid = m_trackSizingAlgorithm.grid();
  if (numberOfLines > 1) {
    // If we have collapsed tracks we just ignore gaps here and add them later
    // as we might not compute the gap between two consecutive tracks without
    // examining the surrounding ones.
    bool hasCollapsedTracks = grid.hasAutoRepeatEmptyTracks(direction);
    LayoutUnit gap = !hasCollapsedTracks
                         ? gridGapForDirection(direction, TrackSizing)
                         : LayoutUnit();
    size_t nextToLastLine = numberOfLines - 2;
    for (size_t i = 0; i < nextToLastLine; ++i)
      positions[i + 1] =
          positions[i] + offset.distributionOffset + tracks[i].baseSize() + gap;
    positions[lastLine] =
        positions[nextToLastLine] + tracks[nextToLastLine].baseSize();

    // Adjust collapsed gaps. Collapsed tracks cause the surrounding gutters to
    // collapse (they coincide exactly) except on the edges of the grid where
    // they become 0.
    if (hasCollapsedTracks) {
      gap = gridGapForDirection(direction, TrackSizing);
      size_t remainingEmptyTracks =
          grid.autoRepeatEmptyTracks(direction)->size();
      LayoutUnit gapAccumulator;
      for (size_t i = 1; i < lastLine; ++i) {
        if (grid.isEmptyAutoRepeatTrack(direction, i - 1)) {
          --remainingEmptyTracks;
        } else {
          // Add gap between consecutive non empty tracks. Add it also just once
          // for an arbitrary number of empty tracks between two non empty ones.
          bool allRemainingTracksAreEmpty =
              remainingEmptyTracks == (lastLine - i);
          if (!allRemainingTracksAreEmpty ||
              !grid.isEmptyAutoRepeatTrack(direction, i))
            gapAccumulator += gap;
        }
        positions[i] += gapAccumulator;
      }
      positions[lastLine] += gapAccumulator;
    }
  }
  auto& offsetBetweenTracks =
      isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
  offsetBetweenTracks = offset.distributionOffset;
}

static LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow,
                                                 LayoutUnit trackSize,
                                                 LayoutUnit childSize) {
  LayoutUnit offset = trackSize - childSize;
  switch (overflow) {
    case OverflowAlignmentSafe:
      // If overflow is 'safe', we have to make sure we don't overflow the
      // 'start' edge (potentially cause some data loss as the overflow is
      // unreachable).
      return offset.clampNegativeToZero();
    case OverflowAlignmentUnsafe:
    case OverflowAlignmentDefault:
      // If we overflow our alignment container and overflow is 'true'
      // (default), we ignore the overflow and just return the value regardless
      // (which may cause data loss as we overflow the 'start' edge).
      return offset;
  }

  ASSERT_NOT_REACHED();
  return LayoutUnit();
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to
// LayoutBox.
LayoutUnit LayoutGrid::marginLogicalHeightForChild(
    const LayoutBox& child) const {
  return isHorizontalWritingMode() ? child.marginHeight() : child.marginWidth();
}

LayoutUnit LayoutGrid::computeMarginLogicalSizeForChild(
    MarginDirection forDirection,
    const LayoutBox& child) const {
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
      forDirection, this, child.containingBlockLogicalWidthForContent(),
      logicalSize, marginStart, marginEnd, marginStartLength, marginEndLength);

  return marginStart + marginEnd;
}

LayoutUnit LayoutGrid::availableAlignmentSpaceForChildBeforeStretching(
    LayoutUnit gridAreaBreadthForChild,
    const LayoutBox& child) const {
  // Because we want to avoid multiple layouts, stretching logic might be
  // performed before children are laid out, so we can't use the child cached
  // values. Hence, we need to compute margins in order to determine the
  // available height before stretching.
  return gridAreaBreadthForChild -
         (child.needsLayout()
              ? computeMarginLogicalSizeForChild(BlockDirection, child)
              : marginLogicalHeightForChild(child));
}

StyleSelfAlignmentData LayoutGrid::alignSelfForChild(
    const LayoutBox& child) const {
  if (!child.isAnonymous()) {
    return child.styleRef().resolvedAlignSelf(
        selfAlignmentNormalBehavior(&child));
  }
  // All the 'auto' values has been solved by the StyleAdjuster, but it's
  // possible that some grid items generate Anonymous boxes, which need to be
  // solved during layout.
  return child.styleRef().resolvedAlignSelf(selfAlignmentNormalBehavior(&child),
                                            style());
}

StyleSelfAlignmentData LayoutGrid::justifySelfForChild(
    const LayoutBox& child) const {
  if (!child.isAnonymous()) {
    return child.styleRef().resolvedJustifySelf(
        selfAlignmentNormalBehavior(&child));
  }
  // All the 'auto' values has been solved by the StyleAdjuster, but it's
  // possible that some grid items generate Anonymous boxes, which need to be
  // solved during layout.
  return child.styleRef().resolvedJustifySelf(
      selfAlignmentNormalBehavior(&child), style());
}

GridTrackSizingDirection LayoutGrid::flowAwareDirectionForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  return !isOrthogonalChild(child)
             ? direction
             : (direction == ForColumns ? ForRows : ForColumns);
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to
// LayoutBox.
void LayoutGrid::applyStretchAlignmentToChildIfNeeded(LayoutBox& child) {
  // We clear height override values because we will decide now whether it's
  // allowed or not, evaluating the conditions which might have changed since
  // the old values were set.
  child.clearOverrideLogicalContentHeight();

  GridTrackSizingDirection childBlockDirection =
      flowAwareDirectionForChild(child, ForRows);
  bool blockFlowIsColumnAxis = childBlockDirection == ForRows;
  bool allowedToStretchChildBlockSize =
      blockFlowIsColumnAxis ? allowedToStretchChildAlongColumnAxis(child)
                            : allowedToStretchChildAlongRowAxis(child);
  if (allowedToStretchChildBlockSize) {
    LayoutUnit stretchedLogicalHeight =
        availableAlignmentSpaceForChildBeforeStretching(
            overrideContainingBlockContentSizeForChild(child,
                                                       childBlockDirection),
            child);
    LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(
        stretchedLogicalHeight, LayoutUnit(-1));
    child.setOverrideLogicalContentHeight(
        (desiredLogicalHeight - child.borderAndPaddingLogicalHeight())
            .clampNegativeToZero());
    if (desiredLogicalHeight != child.logicalHeight()) {
      // TODO (lajava): Can avoid laying out here in some cases. See
      // https://webkit.org/b/87905.
      child.setLogicalHeight(LayoutUnit());
      child.setNeedsLayout(LayoutInvalidationReason::GridChanged);
    }
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
bool LayoutGrid::hasAutoMarginsInColumnAxis(const LayoutBox& child) const {
  if (isHorizontalWritingMode())
    return child.styleRef().marginTop().isAuto() ||
           child.styleRef().marginBottom().isAuto();
  return child.styleRef().marginLeft().isAuto() ||
         child.styleRef().marginRight().isAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
bool LayoutGrid::hasAutoMarginsInRowAxis(const LayoutBox& child) const {
  if (isHorizontalWritingMode())
    return child.styleRef().marginLeft().isAuto() ||
           child.styleRef().marginRight().isAuto();
  return child.styleRef().marginTop().isAuto() ||
         child.styleRef().marginBottom().isAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
DISABLE_CFI_PERF
void LayoutGrid::updateAutoMarginsInRowAxisIfNeeded(LayoutBox& child) {
  ASSERT(!child.isOutOfFlowPositioned());

  LayoutUnit availableAlignmentSpace =
      child.overrideContainingBlockContentLogicalWidth() -
      child.logicalWidth() - child.marginLogicalWidth();
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

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
DISABLE_CFI_PERF
void LayoutGrid::updateAutoMarginsInColumnAxisIfNeeded(LayoutBox& child) {
  ASSERT(!child.isOutOfFlowPositioned());

  LayoutUnit availableAlignmentSpace =
      child.overrideContainingBlockContentLogicalHeight() -
      child.logicalHeight() - child.marginLogicalHeight();
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

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it might be
// refactored somehow.
static int synthesizedBaselineFromContentBox(const LayoutBox& box,
                                             LineDirectionMode direction) {
  if (direction == HorizontalLine) {
    return (box.size().height() - box.borderBottom() - box.paddingBottom() -
            box.horizontalScrollbarHeight())
        .toInt();
  }
  return (box.size().width() - box.borderLeft() - box.paddingLeft() -
          box.verticalScrollbarWidth())
      .toInt();
}

static int synthesizedBaselineFromBorderBox(const LayoutBox& box,
                                            LineDirectionMode direction) {
  return (direction == HorizontalLine ? box.size().height()
                                      : box.size().width())
      .toInt();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it might be
// refactored somehow.
int LayoutGrid::baselinePosition(FontBaseline,
                                 bool,
                                 LineDirectionMode direction,
                                 LinePositionMode mode) const {
  DCHECK_EQ(mode, PositionOnContainingLine);
  int baseline = firstLineBoxBaseline();
  // We take content-box's bottom if no valid baseline.
  if (baseline == -1)
    baseline = synthesizedBaselineFromContentBox(*this, direction);

  return baseline + beforeMarginInLineDirection(direction);
}

bool LayoutGrid::isInlineBaselineAlignedChild(const LayoutBox* child) const {
  return alignSelfForChild(*child).position() == ItemPositionBaseline &&
         !isOrthogonalChild(*child) && !hasAutoMarginsInColumnAxis(*child);
}

int LayoutGrid::firstLineBoxBaseline() const {
  if (isWritingModeRoot() || !m_grid.hasGridItems())
    return -1;
  const LayoutBox* baselineChild = nullptr;
  const LayoutBox* firstChild = nullptr;
  bool isBaselineAligned = false;
  // Finding the first grid item in grid order.
  for (size_t column = 0;
       !isBaselineAligned && column < m_grid.numTracks(ForColumns); column++) {
    for (size_t index = 0; index < m_grid.cell(0, column).size(); index++) {
      const LayoutBox* child = m_grid.cell(0, column)[index];
      DCHECK(!child->isOutOfFlowPositioned());
      // If an item participates in baseline alignmen, we select such item.
      if (isInlineBaselineAlignedChild(child)) {
        // TODO (lajava): self-baseline and content-baseline alignment
        // still not implemented.
        baselineChild = child;
        isBaselineAligned = true;
        break;
      }
      if (!baselineChild) {
        // Use dom order for items in the same cell.
        if (!firstChild || (m_grid.gridItemPaintOrder(*child) <
                            m_grid.gridItemPaintOrder(*firstChild)))
          firstChild = child;
      }
    }
    if (!baselineChild && firstChild)
      baselineChild = firstChild;
  }

  if (!baselineChild)
    return -1;

  int baseline = isOrthogonalChild(*baselineChild)
                     ? -1
                     : baselineChild->firstLineBoxBaseline();
  // We take border-box's bottom if no valid baseline.
  if (baseline == -1) {
    // TODO (lajava): We should pass |direction| into
    // firstLineBoxBaseline and stop bailing out if we're a writing
    // mode root.  This would also fix some cases where the grid is
    // orthogonal to its container.
    LineDirectionMode direction =
        isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
    return (synthesizedBaselineFromBorderBox(*baselineChild, direction) +
            baselineChild->logicalTop())
        .toInt();
  }

  return (baseline + baselineChild->logicalTop()).toInt();
}

int LayoutGrid::inlineBlockBaseline(LineDirectionMode direction) const {
  int baseline = firstLineBoxBaseline();
  if (baseline != -1)
    return baseline;

  int marginHeight =
      (direction == HorizontalLine ? marginTop() : marginRight()).toInt();
  return synthesizedBaselineFromContentBox(*this, direction) + marginHeight;
}

GridAxisPosition LayoutGrid::columnAxisPositionForChild(
    const LayoutBox& child) const {
  bool hasSameWritingMode =
      child.styleRef().getWritingMode() == styleRef().getWritingMode();
  bool childIsLTR = child.styleRef().isLeftToRightDirection();

  switch (alignSelfForChild(child).position()) {
    case ItemPositionSelfStart:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'start' side in the
      // column axis.
      if (isOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-start will be based on the child's
        // inline-axis direction (inline-start), because it's the one parallel
        // to the column axis.
        if (styleRef().isFlippedBlocksWritingMode())
          return childIsLTR ? GridAxisEnd : GridAxisStart;
        return childIsLTR ? GridAxisStart : GridAxisEnd;
      }
      // self-start is based on the child's block-flow direction. That's why we
      // need to check against the grid container's block-flow direction.
      return hasSameWritingMode ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'end' side in the
      // column axis.
      if (isOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-end will be based on the child's
        // inline-axis direction, (inline-end) because it's the one parallel to
        // the column axis.
        if (styleRef().isFlippedBlocksWritingMode())
          return childIsLTR ? GridAxisStart : GridAxisEnd;
        return childIsLTR ? GridAxisEnd : GridAxisStart;
      }
      // self-end is based on the child's block-flow direction. That's why we
      // need to check against the grid container's block-flow direction.
      return hasSameWritingMode ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-left' edge. The alignment axis (column axis) is always orthogonal
      // to the inline axis, hence this value behaves as 'start'.
      return GridAxisStart;
    case ItemPositionRight:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-right' edge. The alignment axis (column axis) is always
      // orthogonal to the inline axis, hence this value behaves as 'start'.
      return GridAxisStart;
    case ItemPositionCenter:
      return GridAxisCenter;
    // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPositionFlexStart:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'start' edge (block-start) in the column axis.
    case ItemPositionStart:
      return GridAxisStart;
    // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPositionFlexEnd:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'end' edge (block-end) in the column axis.
    case ItemPositionEnd:
      return GridAxisEnd;
    case ItemPositionStretch:
      return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
      // FIXME: These two require implementing Baseline Alignment. For now, we
      // always 'start' align the child. crbug.com/234191
      return GridAxisStart;
    case ItemPositionAuto:
    case ItemPositionNormal:
      break;
  }

  ASSERT_NOT_REACHED();
  return GridAxisStart;
}

GridAxisPosition LayoutGrid::rowAxisPositionForChild(
    const LayoutBox& child) const {
  bool hasSameDirection =
      child.styleRef().direction() == styleRef().direction();
  bool gridIsLTR = styleRef().isLeftToRightDirection();

  switch (justifySelfForChild(child).position()) {
    case ItemPositionSelfStart:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'start' side in the
      // row axis.
      if (isOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-start will be based on the child's
        // block-axis direction, because it's the one parallel to the row axis.
        if (child.styleRef().isFlippedBlocksWritingMode())
          return gridIsLTR ? GridAxisEnd : GridAxisStart;
        return gridIsLTR ? GridAxisStart : GridAxisEnd;
      }
      // self-start is based on the child's inline-flow direction. That's why we
      // need to check against the grid container's direction.
      return hasSameDirection ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'end' side in the
      // row axis.
      if (isOrthogonalChild(child)) {
        // If orthogonal writing-modes, self-end will be based on the child's
        // block-axis direction, because it's the one parallel to the row axis.
        if (child.styleRef().isFlippedBlocksWritingMode())
          return gridIsLTR ? GridAxisStart : GridAxisEnd;
        return gridIsLTR ? GridAxisEnd : GridAxisStart;
      }
      // self-end is based on the child's inline-flow direction. That's why we
      // need to check against the grid container's direction.
      return hasSameDirection ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-left' edge. We want the physical 'left' side, so we have to take
      // account, container's inline-flow direction.
      return gridIsLTR ? GridAxisStart : GridAxisEnd;
    case ItemPositionRight:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-right' edge. We want the physical 'right' side, so we have to
      // take account, container's inline-flow direction.
      return gridIsLTR ? GridAxisEnd : GridAxisStart;
    case ItemPositionCenter:
      return GridAxisCenter;
    // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPositionFlexStart:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'start' edge (inline-start) in the row axis.
    case ItemPositionStart:
      return GridAxisStart;
    // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPositionFlexEnd:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'end' edge (inline-end) in the row axis.
    case ItemPositionEnd:
      return GridAxisEnd;
    case ItemPositionStretch:
      return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
      // FIXME: These two require implementing Baseline Alignment. For now, we
      // always 'start' align the child. crbug.com/234191
      return GridAxisStart;
    case ItemPositionAuto:
    case ItemPositionNormal:
      break;
  }

  ASSERT_NOT_REACHED();
  return GridAxisStart;
}

LayoutUnit LayoutGrid::columnAxisOffsetForChild(const LayoutBox& child) const {
  const GridSpan& rowsSpan =
      m_trackSizingAlgorithm.grid().gridItemSpan(child, ForRows);
  size_t childStartLine = rowsSpan.startLine();
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
      size_t childEndLine = rowsSpan.endLine();
      LayoutUnit endOfRow = m_rowPositions[childEndLine];
      // m_rowPositions include distribution offset (because of content
      // alignment) and gutters so we need to subtract them to get the actual
      // end position for a given row (this does not have to be done for the
      // last track as there are no more m_columnPositions after it).
      LayoutUnit trackGap = gridGapForDirection(ForRows, TrackSizing);
      if (childEndLine < m_rowPositions.size() - 1) {
        endOfRow -= trackGap;
        endOfRow -= m_offsetBetweenRows;
      }
      LayoutUnit columnAxisChildSize =
          isOrthogonalChild(child)
              ? child.logicalWidth() + child.marginLogicalWidth()
              : child.logicalHeight() + child.marginLogicalHeight();
      OverflowAlignment overflow = alignSelfForChild(child).overflow();
      LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(
          overflow, endOfRow - startOfRow, columnAxisChildSize);
      return startPosition + (axisPosition == GridAxisEnd
                                  ? offsetFromStartPosition
                                  : offsetFromStartPosition / 2);
    }
  }

  ASSERT_NOT_REACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::rowAxisOffsetForChild(const LayoutBox& child) const {
  const GridSpan& columnsSpan =
      m_trackSizingAlgorithm.grid().gridItemSpan(child, ForColumns);
  size_t childStartLine = columnsSpan.startLine();
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
      size_t childEndLine = columnsSpan.endLine();
      LayoutUnit endOfColumn = m_columnPositions[childEndLine];
      // m_columnPositions include distribution offset (because of content
      // alignment) and gutters so we need to subtract them to get the actual
      // end position for a given column (this does not have to be done for the
      // last track as there are no more m_columnPositions after it).
      LayoutUnit trackGap = gridGapForDirection(ForColumns, TrackSizing);
      if (childEndLine < m_columnPositions.size() - 1) {
        endOfColumn -= trackGap;
        endOfColumn -= m_offsetBetweenColumns;
      }
      LayoutUnit rowAxisChildSize =
          isOrthogonalChild(child)
              ? child.logicalHeight() + child.marginLogicalHeight()
              : child.logicalWidth() + child.marginLogicalWidth();
      OverflowAlignment overflow = justifySelfForChild(child).overflow();
      LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(
          overflow, endOfColumn - startOfColumn, rowAxisChildSize);
      return startPosition + (axisPosition == GridAxisEnd
                                  ? offsetFromStartPosition
                                  : offsetFromStartPosition / 2);
    }
  }

  ASSERT_NOT_REACHED();
  return LayoutUnit();
}

ContentPosition static resolveContentDistributionFallback(
    ContentDistributionType distribution) {
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
      return ContentPositionNormal;
  }

  ASSERT_NOT_REACHED();
  return ContentPositionNormal;
}

static ContentAlignmentData contentDistributionOffset(
    const LayoutUnit& availableFreeSpace,
    ContentPosition& fallbackPosition,
    ContentDistributionType distribution,
    unsigned numberOfGridTracks) {
  if (distribution != ContentDistributionDefault &&
      fallbackPosition == ContentPositionNormal)
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
    case ContentDistributionDefault:
      return {};
  }

  ASSERT_NOT_REACHED();
  return {};
}

ContentAlignmentData LayoutGrid::computeContentPositionAndDistributionOffset(
    GridTrackSizingDirection direction,
    const LayoutUnit& availableFreeSpace,
    unsigned numberOfGridTracks) const {
  bool isRowAxis = direction == ForColumns;
  ContentPosition position = isRowAxis
                                 ? styleRef().resolvedJustifyContentPosition(
                                       contentAlignmentNormalBehavior())
                                 : styleRef().resolvedAlignContentPosition(
                                       contentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      isRowAxis
          ? styleRef().resolvedJustifyContentDistribution(
                contentAlignmentNormalBehavior())
          : styleRef().resolvedAlignContentDistribution(
                contentAlignmentNormalBehavior());
  // If <content-distribution> value can't be applied, 'position' will become
  // the associated <content-position> fallback value.
  ContentAlignmentData contentAlignment = contentDistributionOffset(
      availableFreeSpace, position, distribution, numberOfGridTracks);
  if (contentAlignment.isValid())
    return contentAlignment;

  OverflowAlignment overflow =
      isRowAxis ? styleRef().justifyContentOverflowAlignment()
                : styleRef().alignContentOverflowAlignment();
  // TODO (lajava): Default value for overflow isn't exaclty as 'unsafe'.
  // https://drafts.csswg.org/css-align/#overflow-values
  if (availableFreeSpace == 0 ||
      (availableFreeSpace < 0 && overflow == OverflowAlignmentSafe))
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
    // Only used in flex layout, for other layout, it's equivalent to 'End'.
    case ContentPositionFlexEnd:
    case ContentPositionEnd:
      if (isRowAxis)
        return {styleRef().isLeftToRightDirection() ? availableFreeSpace
                                                    : LayoutUnit(),
                LayoutUnit()};
      return {availableFreeSpace, LayoutUnit()};
    // Only used in flex layout, for other layout, it's equivalent to 'Start'.
    case ContentPositionFlexStart:
    case ContentPositionStart:
      if (isRowAxis)
        return {styleRef().isLeftToRightDirection() ? LayoutUnit()
                                                    : availableFreeSpace,
                LayoutUnit()};
      return {LayoutUnit(), LayoutUnit()};
    case ContentPositionBaseline:
    case ContentPositionLastBaseline:
      // FIXME: These two require implementing Baseline Alignment. For now, we
      // always 'start' align the child. crbug.com/234191
      if (isRowAxis)
        return {styleRef().isLeftToRightDirection() ? LayoutUnit()
                                                    : availableFreeSpace,
                LayoutUnit()};
      return {LayoutUnit(), LayoutUnit()};
    case ContentPositionNormal:
      break;
  }

  ASSERT_NOT_REACHED();
  return {LayoutUnit(), LayoutUnit()};
}

LayoutUnit LayoutGrid::translateRTLCoordinate(LayoutUnit coordinate) const {
  ASSERT(!styleRef().isLeftToRightDirection());

  LayoutUnit alignmentOffset = m_columnPositions[0];
  LayoutUnit rightGridEdgePosition =
      m_columnPositions[m_columnPositions.size() - 1];
  return rightGridEdgePosition + alignmentOffset - coordinate;
}

LayoutPoint LayoutGrid::findChildLogicalPosition(const LayoutBox& child) const {
  LayoutUnit columnAxisOffset = columnAxisOffsetForChild(child);
  LayoutUnit rowAxisOffset = rowAxisOffsetForChild(child);
  // We stored m_columnPosition's data ignoring the direction, hence we might
  // need now to translate positions from RTL to LTR, as it's more convenient
  // for painting.
  if (!style()->isLeftToRightDirection())
    rowAxisOffset = translateRTLCoordinate(rowAxisOffset) -
                    (isOrthogonalChild(child) ? child.logicalHeight()
                                              : child.logicalWidth());

  // "In the positioning phase [...] calculations are performed according to the
  // writing mode of the containing block of the box establishing the orthogonal
  // flow." However, the resulting LayoutPoint will be used in
  // 'setLogicalPosition' in order to set the child's logical position, which
  // will only take into account the child's writing-mode.
  LayoutPoint childLocation(rowAxisOffset, columnAxisOffset);
  return isOrthogonalChild(child) ? childLocation.transposedPoint()
                                  : childLocation;
}

LayoutPoint LayoutGrid::gridAreaLogicalPosition(const GridArea& area) const {
  LayoutUnit columnAxisOffset = m_rowPositions[area.rows.startLine()];
  LayoutUnit rowAxisOffset = m_columnPositions[area.columns.startLine()];

  // See comment in findChildLogicalPosition() about why we need sometimes to
  // translate from RTL to LTR the rowAxisOffset coordinate.
  return LayoutPoint(style()->isLeftToRightDirection()
                         ? rowAxisOffset
                         : translateRTLCoordinate(rowAxisOffset),
                     columnAxisOffset);
}

void LayoutGrid::paintChildren(const PaintInfo& paintInfo,
                               const LayoutPoint& paintOffset) const {
  DCHECK(!m_grid.needsItemsPlacement());
  if (m_grid.hasGridItems())
    GridPainter(*this).paintChildren(paintInfo, paintOffset);
}

bool LayoutGrid::cachedHasDefiniteLogicalHeight() const {
  SECURITY_DCHECK(m_hasDefiniteLogicalHeight);
  return m_hasDefiniteLogicalHeight.value();
}

size_t LayoutGrid::numTracks(GridTrackSizingDirection direction,
                             const Grid& grid) const {
  // Due to limitations in our internal representation, we cannot know the
  // number of columns from m_grid *if* there is no row (because m_grid would be
  // empty). That's why in that case we need to get it from the style. Note that
  // we know for sure that there are't any implicit tracks, because not having
  // rows implies that there are no "normal" children (out-of-flow children are
  // not stored in m_grid).
  DCHECK(!grid.needsItemsPlacement());
  if (direction == ForRows)
    return grid.numTracks(ForRows);

  return grid.numTracks(ForRows)
             ? grid.numTracks(ForColumns)
             : GridPositionsResolver::explicitGridColumnCount(
                   styleRef(), grid.autoRepeatTracks(ForColumns));
}

}  // namespace blink
