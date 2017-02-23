// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/Grid.h"

#include "core/layout/LayoutGrid.h"

namespace blink {

Grid::Grid(const LayoutGrid* grid) : m_orderIterator(grid) {}

size_t Grid::numTracks(GridTrackSizingDirection direction) const {
  if (direction == ForRows)
    return m_grid.size();
  return m_grid.size() ? m_grid[0].size() : 0;
}

void Grid::ensureGridSize(size_t maximumRowSize, size_t maximumColumnSize) {
  const size_t oldRowSize = numTracks(ForRows);
  if (maximumRowSize > oldRowSize) {
    m_grid.grow(maximumRowSize);
    for (size_t row = oldRowSize; row < numTracks(ForRows); ++row)
      m_grid[row].grow(numTracks(ForColumns));
  }

  if (maximumColumnSize > numTracks(ForColumns)) {
    for (size_t row = 0; row < numTracks(ForRows); ++row)
      m_grid[row].grow(maximumColumnSize);
  }
}

void Grid::insert(LayoutBox& child, const GridArea& area) {
  DCHECK(area.rows.isTranslatedDefinite() &&
         area.columns.isTranslatedDefinite());
  ensureGridSize(area.rows.endLine(), area.columns.endLine());

  for (const auto& row : area.rows) {
    for (const auto& column : area.columns)
      m_grid[row][column].push_back(&child);
  }

  setGridItemArea(child, area);
}

void Grid::setSmallestTracksStart(int rowStart, int columnStart) {
  m_smallestRowStart = rowStart;
  m_smallestColumnStart = columnStart;
}

int Grid::smallestTrackStart(GridTrackSizingDirection direction) const {
  return direction == ForRows ? m_smallestRowStart : m_smallestColumnStart;
}

GridArea Grid::gridItemArea(const LayoutBox& item) const {
  DCHECK(m_gridItemArea.contains(&item));
  return m_gridItemArea.at(&item);
}

void Grid::setGridItemArea(const LayoutBox& item, GridArea area) {
  m_gridItemArea.set(&item, area);
}

size_t Grid::gridItemPaintOrder(const LayoutBox& item) const {
  return m_gridItemsIndexesMap.at(&item);
}

void Grid::setGridItemPaintOrder(const LayoutBox& item, size_t order) {
  m_gridItemsIndexesMap.set(&item, order);
}

const GridCell& Grid::cell(size_t row, size_t column) const {
  return m_grid[row][column];
}

#if DCHECK_IS_ON()
bool Grid::hasAnyGridItemPaintOrder() const {
  return !m_gridItemsIndexesMap.isEmpty();
}
#endif

void Grid::setAutoRepeatTracks(size_t autoRepeatRows,
                               size_t autoRepeatColumns) {
  DCHECK_GE(static_cast<unsigned>(kGridMaxTracks),
            numTracks(ForRows) + autoRepeatRows);
  DCHECK_GE(static_cast<unsigned>(kGridMaxTracks),
            numTracks(ForColumns) + autoRepeatColumns);
  m_autoRepeatRows = autoRepeatRows;
  m_autoRepeatColumns = autoRepeatColumns;
}

size_t Grid::autoRepeatTracks(GridTrackSizingDirection direction) const {
  return direction == ForRows ? m_autoRepeatRows : m_autoRepeatColumns;
}

void Grid::setAutoRepeatEmptyColumns(
    std::unique_ptr<OrderedTrackIndexSet> autoRepeatEmptyColumns) {
  m_autoRepeatEmptyColumns = std::move(autoRepeatEmptyColumns);
}

void Grid::setAutoRepeatEmptyRows(
    std::unique_ptr<OrderedTrackIndexSet> autoRepeatEmptyRows) {
  m_autoRepeatEmptyRows = std::move(autoRepeatEmptyRows);
}

bool Grid::hasAutoRepeatEmptyTracks(GridTrackSizingDirection direction) const {
  return direction == ForColumns ? !!m_autoRepeatEmptyColumns
                                 : !!m_autoRepeatEmptyRows;
}

bool Grid::isEmptyAutoRepeatTrack(GridTrackSizingDirection direction,
                                  size_t line) const {
  DCHECK(hasAutoRepeatEmptyTracks(direction));
  return autoRepeatEmptyTracks(direction)->contains(line);
}

OrderedTrackIndexSet* Grid::autoRepeatEmptyTracks(
    GridTrackSizingDirection direction) const {
  DCHECK(hasAutoRepeatEmptyTracks(direction));
  return direction == ForColumns ? m_autoRepeatEmptyColumns.get()
                                 : m_autoRepeatEmptyRows.get();
}

GridSpan Grid::gridItemSpan(const LayoutBox& gridItem,
                            GridTrackSizingDirection direction) const {
  GridArea area = gridItemArea(gridItem);
  return direction == ForColumns ? area.columns : area.rows;
}

void Grid::setHasAnyOrthogonalGridItem(bool hasAnyOrthogonalGridItem) {
  m_hasAnyOrthogonalGridItem = hasAnyOrthogonalGridItem;
}

void Grid::setNeedsItemsPlacement(bool needsItemsPlacement) {
  m_needsItemsPlacement = needsItemsPlacement;

  if (!needsItemsPlacement) {
    m_grid.shrinkToFit();
    return;
  }

  m_grid.resize(0);
  m_gridItemArea.clear();
  m_gridItemsIndexesMap.clear();
  m_hasAnyOrthogonalGridItem = false;
  m_smallestRowStart = 0;
  m_smallestColumnStart = 0;
  m_autoRepeatColumns = 0;
  m_autoRepeatRows = 0;
  m_autoRepeatEmptyColumns = nullptr;
  m_autoRepeatEmptyRows = nullptr;
}

GridIterator::GridIterator(const Grid& grid,
                           GridTrackSizingDirection direction,
                           size_t fixedTrackIndex,
                           size_t varyingTrackIndex)
    : m_grid(grid.m_grid),
      m_direction(direction),
      m_rowIndex((direction == ForColumns) ? varyingTrackIndex
                                           : fixedTrackIndex),
      m_columnIndex((direction == ForColumns) ? fixedTrackIndex
                                              : varyingTrackIndex),
      m_childIndex(0) {
  DCHECK(!m_grid.isEmpty());
  DCHECK(!m_grid[0].isEmpty());
  DCHECK(m_rowIndex < m_grid.size());
  DCHECK(m_columnIndex < m_grid[0].size());
}

LayoutBox* GridIterator::nextGridItem() {
  DCHECK(!m_grid.isEmpty());
  DCHECK(!m_grid[0].isEmpty());

  size_t& varyingTrackIndex =
      (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
  const size_t endOfVaryingTrackIndex =
      (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
  for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
    const GridCell& children = m_grid[m_rowIndex][m_columnIndex];
    if (m_childIndex < children.size())
      return children[m_childIndex++];

    m_childIndex = 0;
  }
  return nullptr;
}

bool GridIterator::checkEmptyCells(size_t rowSpan, size_t columnSpan) const {
  DCHECK(!m_grid.isEmpty());
  DCHECK(!m_grid[0].isEmpty());

  // Ignore cells outside current grid as we will grow it later if needed.
  size_t maxRows = std::min(m_rowIndex + rowSpan, m_grid.size());
  size_t maxColumns = std::min(m_columnIndex + columnSpan, m_grid[0].size());

  // This adds a O(N^2) behavior that shouldn't be a big deal as we expect
  // spanning areas to be small.
  for (size_t row = m_rowIndex; row < maxRows; ++row) {
    for (size_t column = m_columnIndex; column < maxColumns; ++column) {
      const GridCell& children = m_grid[row][column];
      if (!children.isEmpty())
        return false;
    }
  }

  return true;
}

std::unique_ptr<GridArea> GridIterator::nextEmptyGridArea(
    size_t fixedTrackSpan,
    size_t varyingTrackSpan) {
  DCHECK(!m_grid.isEmpty());
  DCHECK(!m_grid[0].isEmpty());
  DCHECK(fixedTrackSpan >= 1 && varyingTrackSpan >= 1);

  size_t rowSpan =
      (m_direction == ForColumns) ? varyingTrackSpan : fixedTrackSpan;
  size_t columnSpan =
      (m_direction == ForColumns) ? fixedTrackSpan : varyingTrackSpan;

  size_t& varyingTrackIndex =
      (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
  const size_t endOfVaryingTrackIndex =
      (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
  for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
    if (checkEmptyCells(rowSpan, columnSpan)) {
      std::unique_ptr<GridArea> result = WTF::wrapUnique(
          new GridArea(GridSpan::translatedDefiniteGridSpan(
                           m_rowIndex, m_rowIndex + rowSpan),
                       GridSpan::translatedDefiniteGridSpan(
                           m_columnIndex, m_columnIndex + columnSpan)));
      // Advance the iterator to avoid an infinite loop where we would return
      // the same grid area over and over.
      ++varyingTrackIndex;
      return result;
    }
  }
  return nullptr;
}

}  // namespace blink
