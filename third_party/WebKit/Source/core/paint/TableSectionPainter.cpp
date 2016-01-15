// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableSectionPainter.h"

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"
#include "core/paint/TableRowPainter.h"
#include <algorithm>

namespace blink {

void TableSectionPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!m_layoutTableSection.needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (m_layoutTableSection.needsLayout())
        return;

    // Table sections don't paint self background. The cells paint table section's background
    // behind them when needed during PaintPhaseBlockBackground or PaintPhaseDescendantBlockBackgroundOnly.
    if (paintInfo.phase == PaintPhaseSelfBlockBackgroundOnly)
        return;

    unsigned totalRows = m_layoutTableSection.numRows();
    unsigned totalCols = m_layoutTableSection.table()->columns().size();

    if (!totalRows || !totalCols)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableSection.location();

    if (paintInfo.phase != PaintPhaseSelfOutlineOnly) {
        BoxClipper boxClipper(m_layoutTableSection, paintInfo, adjustedPaintOffset, ForceContentsClip);
        paintObject(paintInfo, adjustedPaintOffset);
    }

    if (shouldPaintSelfOutline(paintInfo.phase))
        ObjectPainter(m_layoutTableSection).paintOutline(paintInfo, adjustedPaintOffset);
}

static inline bool compareCellPositions(LayoutTableCell* elem1, LayoutTableCell* elem2)
{
    return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly issue paint invalidations.
static inline bool compareCellPositionsWithOverflowingCells(LayoutTableCell* elem1, LayoutTableCell* elem2)
{
    if (elem1->rowIndex() != elem2->rowIndex())
        return elem1->rowIndex() < elem2->rowIndex();

    return elem1->col() < elem2->col();
}

void TableSectionPainter::paintCollapsedBorders(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorderValue)
{
    if (!m_layoutTableSection.numRows() || !m_layoutTableSection.table()->columns().size())
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableSection.location();
    BoxClipper boxClipper(m_layoutTableSection, paintInfo, adjustedPaintOffset, ForceContentsClip);

    LayoutRect localPaintInvalidationRect = LayoutRect(paintInfo.cullRect().m_rect);
    localPaintInvalidationRect.moveBy(-adjustedPaintOffset);

    LayoutRect tableAlignedRect = m_layoutTableSection.logicalRectForWritingModeAndDirection(localPaintInvalidationRect);

    CellSpan dirtiedRows = m_layoutTableSection.dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = m_layoutTableSection.dirtiedColumns(tableAlignedRect);

    if (dirtiedColumns.start() >= dirtiedColumns.end())
        return;

    // Collapsed borders are painted from the bottom right to the top left so that precedence
    // due to cell position is respected.
    for (unsigned r = dirtiedRows.end(); r > dirtiedRows.start(); r--) {
        unsigned row = r - 1;
        for (unsigned c = dirtiedColumns.end(); c > dirtiedColumns.start(); c--) {
            unsigned col = c - 1;
            const LayoutTableSection::CellStruct& current = m_layoutTableSection.cellAt(row, col);
            const LayoutTableCell* cell = current.primaryCell();
            if (!cell || (row > dirtiedRows.start() && m_layoutTableSection.primaryCellAt(row - 1, col) == cell) || (col > dirtiedColumns.start() && m_layoutTableSection.primaryCellAt(row, col - 1) == cell))
                continue;
            LayoutPoint cellPoint = m_layoutTableSection.flipForWritingModeForChild(cell, adjustedPaintOffset);
            TableCellPainter(*cell).paintCollapsedBorders(paintInfo, cellPoint, currentBorderValue);
        }
    }
}

void TableSectionPainter::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect localPaintInvalidationRect = LayoutRect(paintInfo.cullRect().m_rect);
    localPaintInvalidationRect.moveBy(-paintOffset);

    LayoutRect tableAlignedRect = m_layoutTableSection.logicalRectForWritingModeAndDirection(localPaintInvalidationRect);

    CellSpan dirtiedRows = m_layoutTableSection.dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = m_layoutTableSection.dirtiedColumns(tableAlignedRect);

    if (dirtiedColumns.start() >= dirtiedColumns.end())
        return;

    PaintInfo paintInfoForCells = paintInfo.forDescendants();
    const HashSet<LayoutTableCell*>& overflowingCells = m_layoutTableSection.overflowingCells();
    if (!m_layoutTableSection.hasMultipleCellLevels() && !overflowingCells.size()) {
        // Draw the dirty cells in the order that they appear.
        for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
            const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
            // TODO(wangxianzhu): This painting order is inconsistent with other outlines. crbug.com/577282.
            if (row && !row->hasSelfPaintingLayer())
                TableRowPainter(*row).paintOutlineForRowIfNeeded(paintInfo, paintOffset);
            for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                const LayoutTableSection::CellStruct& current = m_layoutTableSection.cellAt(r, c);
                const LayoutTableCell* cell = current.primaryCell();
                if (!cell || (r > dirtiedRows.start() && m_layoutTableSection.primaryCellAt(r - 1, c) == cell) || (c > dirtiedColumns.start() && m_layoutTableSection.primaryCellAt(r, c - 1) == cell))
                    continue;
                paintCell(*cell, paintInfoForCells, paintOffset);
            }
        }
    } else {
        // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
#if ENABLE(ASSERT)
        unsigned totalRows = m_layoutTableSection.numRows();
        unsigned totalCols = m_layoutTableSection.table()->columns().size();
        ASSERT(overflowingCells.size() < totalRows * totalCols * gMaxAllowedOverflowingCellRatioForFastPaintPath);
#endif

        // To make sure we properly paint invalidate the section, we paint invalidated all the overflowing cells that we collected.
        Vector<LayoutTableCell*> cells;
        copyToVector(overflowingCells, cells);

        HashSet<LayoutTableCell*> spanningCells;

        for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
            const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
            // TODO(wangxianzhu): This painting order is inconsistent with other outlines. crbug.com/577282.
            if (row && !row->hasSelfPaintingLayer())
                TableRowPainter(*row).paintOutlineForRowIfNeeded(paintInfo, paintOffset);
            for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                const LayoutTableSection::CellStruct& current = m_layoutTableSection.cellAt(r, c);
                if (!current.hasCells())
                    continue;
                for (unsigned i = 0; i < current.cells.size(); ++i) {
                    if (overflowingCells.contains(current.cells[i]))
                        continue;

                    if (current.cells[i]->rowSpan() > 1 || current.cells[i]->colSpan() > 1) {
                        if (!spanningCells.add(current.cells[i]).isNewEntry)
                            continue;
                    }

                    cells.append(current.cells[i]);
                }
            }
        }

        // Sort the dirty cells by paint order.
        if (!overflowingCells.size())
            std::stable_sort(cells.begin(), cells.end(), compareCellPositions);
        else
            std::sort(cells.begin(), cells.end(), compareCellPositionsWithOverflowingCells);

        for (unsigned i = 0; i < cells.size(); ++i)
            paintCell(*cells[i], paintInfoForCells, paintOffset);
    }
}

void TableSectionPainter::paintCell(const LayoutTableCell& cell, const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint cellPoint = m_layoutTableSection.flipForWritingModeForChild(&cell, paintOffset);
    PaintPhase paintPhase = paintInfo.phase;
    const LayoutTableRow* row = toLayoutTableRow(cell.parent());

    if (shouldPaintSelfBlockBackground(paintPhase)
        && BlockPainter(cell).intersectsPaintRect(paintInfo, paintOffset)) {
        // We need to handle painting a stack of backgrounds. This stack (from bottom to top) consists of
        // the column group, column, row group, row, and then the cell.

        LayoutTable::ColAndColGroup colAndColGroup = m_layoutTableSection.table()->colElement(cell.col());
        LayoutTableCol* column = colAndColGroup.col;
        LayoutTableCol* columnGroup = colAndColGroup.colgroup;
        TableCellPainter tableCellPainter(cell);

        // Column groups and columns first.
        // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
        // the stack, since we have already opened a transparency layer (potentially) for the table row group.
        // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
        // cell.
        if (columnGroup && columnGroup->hasBackground())
            tableCellPainter.paintBackgroundsBehindCell(paintInfo, cellPoint, columnGroup, DisplayItem::TableCellBackgroundFromColumnGroup);
        if (column && column->hasBackground())
            tableCellPainter.paintBackgroundsBehindCell(paintInfo, cellPoint, column, DisplayItem::TableCellBackgroundFromColumn);

        // Paint the row group next.
        if (m_layoutTableSection.hasBackground())
            tableCellPainter.paintBackgroundsBehindCell(paintInfo, cellPoint, &m_layoutTableSection, DisplayItem::TableCellBackgroundFromSection);

        // Paint the row next, but only if it doesn't have a layer. If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (row->hasBackground() && !row->hasSelfPaintingLayer())
            tableCellPainter.paintBackgroundsBehindCell(paintInfo, cellPoint, row, DisplayItem::TableCellBackgroundFromRow);
    }
    if ((!cell.hasSelfPaintingLayer() && !row->hasSelfPaintingLayer()))
        cell.paint(paintInfo, cellPoint);
}

} // namespace blink
