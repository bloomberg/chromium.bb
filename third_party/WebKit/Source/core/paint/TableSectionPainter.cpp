// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableSectionPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"
#include "core/paint/TableRowPainter.h"
#include <algorithm>

namespace blink {

void TableSectionPainter::paintRepeatingHeaderGroup(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset,
    const CollapsedBorderValue& currentBorderValue,
    ItemToPaint itemToPaint) {
  if (!m_layoutTableSection.isRepeatingHeaderGroup())
    return;

  LayoutTable* table = m_layoutTableSection.table();
  LayoutPoint paginationOffset = paintOffset;
  LayoutUnit pageHeight = table->pageLogicalHeightForOffset(LayoutUnit());

  LayoutUnit headerGroupOffset = table->blockOffsetToFirstRepeatableHeader();
  // The header may have a pagination strut before it so we need to account for
  // that when establishing its position.
  if (LayoutTableRow* row = m_layoutTableSection.firstRow())
    headerGroupOffset += row->paginationStrut();
  LayoutUnit offsetToNextPage =
      pageHeight - intMod(headerGroupOffset, pageHeight);
  // Move paginationOffset to the top of the next page.
  paginationOffset.move(LayoutUnit(), offsetToNextPage);
  // Now move paginationOffset to the top of the page the cull rect starts on.
  if (paintInfo.cullRect().m_rect.y() > paginationOffset.y()) {
    paginationOffset.move(LayoutUnit(), pageHeight *
                                            ((paintInfo.cullRect().m_rect.y() -
                                              paginationOffset.y()) /
                                             pageHeight)
                                                .toInt());
  }

  // We only want to consider pages where we going to paint a row, so exclude
  // captions and border spacing from the table.
  LayoutRect sectionsRect(LayoutPoint(), table->size());
  table->subtractCaptionRect(sectionsRect);
  LayoutUnit totalHeightOfRows =
      sectionsRect.height() - table->vBorderSpacing();
  LayoutUnit bottomBound =
      std::min(LayoutUnit(paintInfo.cullRect().m_rect.maxY()),
               paintOffset.y() + totalHeightOfRows);

  while (paginationOffset.y() < bottomBound) {
    if (itemToPaint == PaintCollapsedBorders) {
      paintCollapsedSectionBorders(paintInfo, paginationOffset,
                                   currentBorderValue);
    } else {
      paintSection(paintInfo, paginationOffset);
    }
    paginationOffset.move(0, pageHeight.toInt());
  }
}

void TableSectionPainter::paint(const PaintInfo& paintInfo,
                                const LayoutPoint& paintOffset) {
  ObjectPainter(m_layoutTableSection).checkPaintOffset(paintInfo, paintOffset);
  paintSection(paintInfo, paintOffset);
  LayoutTable* table = m_layoutTableSection.table();
  if (table->header() == m_layoutTableSection)
    paintRepeatingHeaderGroup(paintInfo, paintOffset, CollapsedBorderValue(),
                              PaintSection);
}

void TableSectionPainter::paintSection(const PaintInfo& paintInfo,
                                       const LayoutPoint& paintOffset) {
  DCHECK(!m_layoutTableSection.needsLayout());
  // avoid crashing on bugs that cause us to paint with dirty layout
  if (m_layoutTableSection.needsLayout())
    return;

  unsigned totalRows = m_layoutTableSection.numRows();
  unsigned totalCols = m_layoutTableSection.table()->numEffectiveColumns();

  if (!totalRows || !totalCols)
    return;

  LayoutPoint adjustedPaintOffset =
      paintOffset + m_layoutTableSection.location();

  if (paintInfo.phase != PaintPhaseSelfOutlineOnly) {
    Optional<BoxClipper> boxClipper;
    if (paintInfo.phase != PaintPhaseSelfBlockBackgroundOnly)
      boxClipper.emplace(m_layoutTableSection, paintInfo, adjustedPaintOffset,
                         ForceContentsClip);
    paintObject(paintInfo, adjustedPaintOffset);
  }

  if (shouldPaintSelfOutline(paintInfo.phase))
    ObjectPainter(m_layoutTableSection)
        .paintOutline(paintInfo, adjustedPaintOffset);
}

static inline bool compareCellPositions(const LayoutTableCell* elem1,
                                        const LayoutTableCell* elem2) {
  return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an
// unsorted array to sort. We thus need to sort both on rows and columns to
// properly issue paint invalidations.
static inline bool compareCellPositionsWithOverflowingCells(
    const LayoutTableCell* elem1,
    const LayoutTableCell* elem2) {
  if (elem1->rowIndex() != elem2->rowIndex())
    return elem1->rowIndex() < elem2->rowIndex();

  return elem1->absoluteColumnIndex() < elem2->absoluteColumnIndex();
}

void TableSectionPainter::paintCollapsedBorders(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset,
    const CollapsedBorderValue& currentBorderValue) {
  paintCollapsedSectionBorders(paintInfo, paintOffset, currentBorderValue);
  LayoutTable* table = m_layoutTableSection.table();
  if (table->header() == m_layoutTableSection)
    paintRepeatingHeaderGroup(paintInfo, paintOffset, currentBorderValue,
                              PaintCollapsedBorders);
}

void TableSectionPainter::paintCollapsedSectionBorders(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset,
    const CollapsedBorderValue& currentBorderValue) {
  if (!m_layoutTableSection.numRows() ||
      !m_layoutTableSection.table()->effectiveColumns().size())
    return;

  LayoutPoint adjustedPaintOffset =
      paintOffset + m_layoutTableSection.location();
  BoxClipper boxClipper(m_layoutTableSection, paintInfo, adjustedPaintOffset,
                        ForceContentsClip);

  LayoutRect localVisualRect = LayoutRect(paintInfo.cullRect().m_rect);
  localVisualRect.moveBy(-adjustedPaintOffset);

  LayoutRect tableAlignedRect =
      m_layoutTableSection.logicalRectForWritingModeAndDirection(
          localVisualRect);

  CellSpan dirtiedRows = m_layoutTableSection.dirtiedRows(tableAlignedRect);
  CellSpan dirtiedColumns =
      m_layoutTableSection.dirtiedEffectiveColumns(tableAlignedRect);

  if (dirtiedColumns.start() >= dirtiedColumns.end())
    return;

  // Collapsed borders are painted from the bottom right to the top left so that
  // precedence due to cell position is respected.
  for (unsigned r = dirtiedRows.end(); r > dirtiedRows.start(); r--) {
    unsigned row = r - 1;
    unsigned nCols = m_layoutTableSection.numCols(row);
    for (unsigned c = std::min(dirtiedColumns.end(), nCols);
         c > dirtiedColumns.start(); c--) {
      unsigned col = c - 1;
      if (const LayoutTableCell* cell =
              m_layoutTableSection.originatingCellAt(row, col)) {
        LayoutPoint cellPoint = m_layoutTableSection.flipForWritingModeForChild(
            cell, adjustedPaintOffset);
        TableCellPainter(*cell).paintCollapsedBorders(paintInfo, cellPoint,
                                                      currentBorderValue);
      }
    }
  }
}

void TableSectionPainter::paintObject(const PaintInfo& paintInfo,
                                      const LayoutPoint& paintOffset) {
  LayoutRect localVisualRect = LayoutRect(paintInfo.cullRect().m_rect);
  localVisualRect.moveBy(-paintOffset);

  LayoutRect tableAlignedRect =
      m_layoutTableSection.logicalRectForWritingModeAndDirection(
          localVisualRect);

  CellSpan dirtiedRows = m_layoutTableSection.dirtiedRows(tableAlignedRect);
  CellSpan dirtiedColumns =
      m_layoutTableSection.dirtiedEffectiveColumns(tableAlignedRect);

  PaintInfo paintInfoForDescendants = paintInfo.forDescendants();

  if (shouldPaintSelfBlockBackground(paintInfo.phase)) {
    paintBoxDecorationBackground(paintInfo, paintOffset, dirtiedRows,
                                 dirtiedColumns);
  }

  if (paintInfo.phase == PaintPhaseSelfBlockBackgroundOnly)
    return;

  if (shouldPaintDescendantBlockBackgrounds(paintInfo.phase)) {
    for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
      const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
      // If a row has a layer, we'll paint row background though
      // TableRowPainter::paint().
      if (!row || row->hasSelfPaintingLayer())
        continue;
      TableRowPainter(*row).paintBoxDecorationBackground(
          paintInfoForDescendants, paintOffset, dirtiedColumns);
    }
  }

  // This is tested after background painting because during background painting
  // we need to check validity of the previous background display item based on
  // dirtyRows and dirtyColumns.
  if (dirtiedRows.start() >= dirtiedRows.end() ||
      dirtiedColumns.start() >= dirtiedColumns.end())
    return;

  const auto& overflowingCells = m_layoutTableSection.overflowingCells();
  if (!m_layoutTableSection.hasMultipleCellLevels() &&
      overflowingCells.isEmpty()) {
    for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
      const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
      // TODO(crbug.com/577282): This painting order is inconsistent with other
      // outlines.
      if (row && !row->hasSelfPaintingLayer() &&
          shouldPaintSelfOutline(paintInfoForDescendants.phase)) {
        TableRowPainter(*row).paintOutline(paintInfoForDescendants,
                                           paintOffset);
      }
      for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
        if (const LayoutTableCell* cell =
                m_layoutTableSection.originatingCellAt(r, c))
          paintCell(*cell, paintInfoForDescendants, paintOffset);
      }
    }
  } else {
    // The overflowing cells should be scarce to avoid adding a lot of cells to
    // the HashSet.
    DCHECK(overflowingCells.size() <
           m_layoutTableSection.numRows() *
               m_layoutTableSection.table()->effectiveColumns().size() *
               gMaxAllowedOverflowingCellRatioForFastPaintPath);

    // To make sure we properly paint the section, we paint all the overflowing
    // cells that we collected.
    Vector<const LayoutTableCell*> cells;
    copyToVector(overflowingCells, cells);

    HashSet<const LayoutTableCell*> spanningCells;
    for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
      const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
      // TODO(crbug.com/577282): This painting order is inconsistent with other
      // outlines.
      if (row && !row->hasSelfPaintingLayer() &&
          shouldPaintSelfOutline(paintInfoForDescendants.phase)) {
        TableRowPainter(*row).paintOutline(paintInfoForDescendants,
                                           paintOffset);
      }
      unsigned nCols = m_layoutTableSection.numCols(r);
      for (unsigned c = dirtiedColumns.start();
           c < nCols && c < dirtiedColumns.end(); c++) {
        const auto& cellStruct = m_layoutTableSection.cellAt(r, c);
        for (const auto* cell : cellStruct.cells) {
          if (overflowingCells.contains(cell))
            continue;
          if (cell->rowSpan() > 1 || cell->colSpan() > 1) {
            if (!spanningCells.insert(cell).isNewEntry)
              continue;
          }
          cells.push_back(cell);
        }
      }
    }

    // Sort the dirty cells by paint order.
    if (!overflowingCells.size()) {
      std::stable_sort(cells.begin(), cells.end(), compareCellPositions);
    } else {
      std::sort(cells.begin(), cells.end(),
                compareCellPositionsWithOverflowingCells);
    }

    for (const auto* cell : cells)
      paintCell(*cell, paintInfoForDescendants, paintOffset);
  }
}

void TableSectionPainter::paintBoxDecorationBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset,
    const CellSpan& dirtiedRows,
    const CellSpan& dirtiedColumns) {
  bool mayHaveBackground = m_layoutTableSection.table()->hasColElements() ||
                           m_layoutTableSection.styleRef().hasBackground();
  bool hasBoxShadow = m_layoutTableSection.styleRef().boxShadow();
  if (!mayHaveBackground && !hasBoxShadow)
    return;

  PaintResult paintResult =
      dirtiedColumns == m_layoutTableSection.fullTableEffectiveColumnSpan() &&
              dirtiedRows == m_layoutTableSection.fullSectionRowSpan()
          ? FullyPainted
          : MayBeClippedByPaintDirtyRect;
  m_layoutTableSection.getMutableForPainting().updatePaintResult(
      paintResult, paintInfo.cullRect());

  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          paintInfo.context, m_layoutTableSection,
          DisplayItem::kBoxDecorationBackground))
    return;

  LayoutRect bounds = BoxPainter(m_layoutTableSection)
                          .boundsForDrawingRecorder(paintInfo, paintOffset);
  LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableSection,
                                       DisplayItem::kBoxDecorationBackground,
                                       bounds);
  LayoutRect paintRect(paintOffset, m_layoutTableSection.size());

  if (hasBoxShadow) {
    BoxPainter::paintNormalBoxShadow(paintInfo, paintRect,
                                     m_layoutTableSection.styleRef());
  }

  if (mayHaveBackground) {
    PaintInfo paintInfoForCells = paintInfo.forDescendants();
    for (auto r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
      for (auto c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
        if (const auto* cell = m_layoutTableSection.originatingCellAt(r, c)) {
          paintBackgroundsBehindCell(*cell, paintInfoForCells, paintOffset);
        }
      }
    }
  }

  if (hasBoxShadow) {
    // TODO(wangxianzhu): Calculate the inset shadow bounds by insetting
    // paintRect by half widths of collapsed borders.
    BoxPainter::paintInsetBoxShadow(paintInfo, paintRect,
                                    m_layoutTableSection.styleRef());
  }
}

void TableSectionPainter::paintBackgroundsBehindCell(
    const LayoutTableCell& cell,
    const PaintInfo& paintInfoForCells,
    const LayoutPoint& paintOffset) {
  LayoutPoint cellPoint =
      m_layoutTableSection.flipForWritingModeForChild(&cell, paintOffset);

  // We need to handle painting a stack of backgrounds. This stack (from bottom
  // to top) consists of the column group, column, row group, row, and then the
  // cell.

  LayoutTable::ColAndColGroup colAndColGroup =
      m_layoutTableSection.table()->colElementAtAbsoluteColumn(
          cell.absoluteColumnIndex());
  LayoutTableCol* column = colAndColGroup.col;
  LayoutTableCol* columnGroup = colAndColGroup.colgroup;
  TableCellPainter tableCellPainter(cell);

  // Column groups and columns first.
  // FIXME: Columns and column groups do not currently support opacity, and they
  // are being painted "too late" in the stack, since we have already opened a
  // transparency layer (potentially) for the table row group.  Note that we
  // deliberately ignore whether or not the cell has a layer, since these
  // backgrounds paint "behind" the cell.
  if (columnGroup && columnGroup->styleRef().hasBackground()) {
    tableCellPainter.paintContainerBackgroundBehindCell(
        paintInfoForCells, cellPoint, *columnGroup);
  }
  if (column && column->styleRef().hasBackground()) {
    tableCellPainter.paintContainerBackgroundBehindCell(paintInfoForCells,
                                                        cellPoint, *column);
  }

  // Paint the row group next.
  if (m_layoutTableSection.styleRef().hasBackground()) {
    tableCellPainter.paintContainerBackgroundBehindCell(
        paintInfoForCells, cellPoint, m_layoutTableSection);
  }
}

void TableSectionPainter::paintCell(const LayoutTableCell& cell,
                                    const PaintInfo& paintInfoForCells,
                                    const LayoutPoint& paintOffset) {
  if (!cell.hasSelfPaintingLayer() && !cell.row()->hasSelfPaintingLayer()) {
    LayoutPoint cellPoint =
        m_layoutTableSection.flipForWritingModeForChild(&cell, paintOffset);
    cell.paint(paintInfoForCells, cellPoint);
  }
}

}  // namespace blink
