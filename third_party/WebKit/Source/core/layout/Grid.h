// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Grid_h
#define Grid_h

#include "core/layout/OrderIterator.h"
#include "core/style/GridArea.h"
#include "core/style/GridPositionsResolver.h"
#include "wtf/Assertions.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"

namespace blink {

// TODO(svillar): Perhaps we should use references here.
typedef Vector<LayoutBox*, 1> GridCell;
typedef Vector<Vector<GridCell>> GridAsMatrix;
typedef ListHashSet<size_t> OrderedTrackIndexSet;

class LayoutGrid;
class GridIterator;

// The Grid class represent a generic storage for grid items. It's currently
// implemented as a matrix (vector of vectors) but it can be eventually replaced
// by a more memory efficient representation. This class is used by the
// LayoutGrid object to place the grid items on a grid like structure, so that
// they could be accessed by rows/columns instead of just traversing the DOM or
// Layout trees.
class Grid final {
 public:
  Grid(const LayoutGrid*);

  size_t numTracks(GridTrackSizingDirection) const;

  void ensureGridSize(size_t maximumRowSize, size_t maximumColumnSize);
  void insert(LayoutBox&, const GridArea&);

  // Note that out of flow children are not grid items.
  bool hasGridItems() const { return !m_gridItemArea.isEmpty(); }

  bool hasAnyOrthogonalGridItem() const { return m_hasAnyOrthogonalGridItem; }
  void setHasAnyOrthogonalGridItem(bool);

  GridArea gridItemArea(const LayoutBox&) const;
  void setGridItemArea(const LayoutBox&, GridArea);

  GridSpan gridItemSpan(const LayoutBox&, GridTrackSizingDirection) const;

  size_t gridItemPaintOrder(const LayoutBox&) const;
  void setGridItemPaintOrder(const LayoutBox&, size_t order);

  const GridCell& cell(size_t row, size_t column) const;

  int smallestTrackStart(GridTrackSizingDirection) const;
  void setSmallestTracksStart(int rowStart, int columnStart);

  size_t autoRepeatTracks(GridTrackSizingDirection) const;
  void setAutoRepeatTracks(size_t autoRepeatRows, size_t autoRepeatColumns);

  typedef ListHashSet<size_t> OrderedTrackIndexSet;
  void setAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet>);
  void setAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet>);

  size_t autoRepeatEmptyTracksCount(GridTrackSizingDirection) const;
  bool hasAutoRepeatEmptyTracks(GridTrackSizingDirection) const;
  bool isEmptyAutoRepeatTrack(GridTrackSizingDirection, size_t) const;

  OrderedTrackIndexSet* autoRepeatEmptyTracks(GridTrackSizingDirection) const;

  OrderIterator& orderIterator() { return m_orderIterator; }

  void setNeedsItemsPlacement(bool);
  bool needsItemsPlacement() const { return m_needsItemsPlacement; };

#if DCHECK_IS_ON()
  bool hasAnyGridItemPaintOrder() const;
#endif

 private:
  friend class GridIterator;

  OrderIterator m_orderIterator;

  int m_smallestColumnStart{0};
  int m_smallestRowStart{0};

  size_t m_autoRepeatColumns{0};
  size_t m_autoRepeatRows{0};

  bool m_hasAnyOrthogonalGridItem{false};
  bool m_needsItemsPlacement{true};

  GridAsMatrix m_grid;

  HashMap<const LayoutBox*, GridArea> m_gridItemArea;
  HashMap<const LayoutBox*, size_t> m_gridItemsIndexesMap;

  std::unique_ptr<OrderedTrackIndexSet> m_autoRepeatEmptyColumns{nullptr};
  std::unique_ptr<OrderedTrackIndexSet> m_autoRepeatEmptyRows{nullptr};
};

// TODO(svillar): ideally the Grid class should be the one returning an iterator
// for its contents.
class GridIterator final {
  WTF_MAKE_NONCOPYABLE(GridIterator);

 public:
  // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
  // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd
  // column.
  GridIterator(const Grid&,
               GridTrackSizingDirection,
               size_t fixedTrackIndex,
               size_t varyingTrackIndex = 0);

  LayoutBox* nextGridItem();

  bool checkEmptyCells(size_t rowSpan, size_t columnSpan) const;

  std::unique_ptr<GridArea> nextEmptyGridArea(size_t fixedTrackSpan,
                                              size_t varyingTrackSpan);

 private:
  const GridAsMatrix& m_grid;
  GridTrackSizingDirection m_direction;
  size_t m_rowIndex;
  size_t m_columnIndex;
  size_t m_childIndex;
};

}  // namespace blink

#endif  // Grid_h
