// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TableSectionPainter.h"

#include "core/paint/TableRowPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTable.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderTableCol.h"
#include "core/rendering/RenderTableRow.h"

namespace blink {

void TableSectionPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderTableSection);

    ASSERT(!m_renderTableSection.needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (m_renderTableSection.needsLayout())
        return;

    unsigned totalRows = m_renderTableSection.numRows();
    unsigned totalCols = m_renderTableSection.table()->columns().size();

    if (!totalRows || !totalCols)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_renderTableSection.location();

    PaintPhase phase = paintInfo.phase;
    bool pushedClip = m_renderTableSection.pushContentsClip(paintInfo, adjustedPaintOffset, ForceContentsClip);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        m_renderTableSection.popContentsClip(paintInfo, phase, adjustedPaintOffset);

    if ((phase == PaintPhaseOutline || phase == PaintPhaseSelfOutline) && m_renderTableSection.style()->visibility() == VISIBLE)
        m_renderTableSection.paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, m_renderTableSection.size()));
}

static inline bool compareCellPositions(RenderTableCell* elem1, RenderTableCell* elem2)
{
    return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly issue paint invalidations.
static inline bool compareCellPositionsWithOverflowingCells(RenderTableCell* elem1, RenderTableCell* elem2)
{
    if (elem1->rowIndex() != elem2->rowIndex())
        return elem1->rowIndex() < elem2->rowIndex();

    return elem1->col() < elem2->col();
}

void TableSectionPainter::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect localPaintInvalidationRect = paintInfo.rect;
    localPaintInvalidationRect.moveBy(-paintOffset);

    LayoutRect tableAlignedRect = m_renderTableSection.logicalRectForWritingModeAndDirection(localPaintInvalidationRect);

    CellSpan dirtiedRows = m_renderTableSection.dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = m_renderTableSection.dirtiedColumns(tableAlignedRect);

    WillBeHeapHashSet<RawPtrWillBeMember<RenderTableCell> > overflowingCells = m_renderTableSection.overflowingCells();
    if (dirtiedColumns.start() < dirtiedColumns.end()) {
        if (!m_renderTableSection.hasMultipleCellLevels() && !overflowingCells.size()) {
            if (paintInfo.phase == PaintPhaseCollapsedTableBorders) {
                // Collapsed borders are painted from the bottom right to the top left so that precedence
                // due to cell position is respected.
                for (unsigned r = dirtiedRows.end(); r > dirtiedRows.start(); r--) {
                    unsigned row = r - 1;
                    for (unsigned c = dirtiedColumns.end(); c > dirtiedColumns.start(); c--) {
                        unsigned col = c - 1;
                        RenderTableSection::CellStruct& current = m_renderTableSection.cellAt(row, col);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (row > dirtiedRows.start() && m_renderTableSection.primaryCellAt(row - 1, col) == cell) || (col > dirtiedColumns.start() && m_renderTableSection.primaryCellAt(row, col - 1) == cell))
                            continue;
                        LayoutPoint cellPoint = m_renderTableSection.flipForWritingModeForChild(cell, paintOffset);
                        cell->paintCollapsedBorders(paintInfo, cellPoint);
                    }
                }
            } else {
                // Draw the dirty cells in the order that they appear.
                for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
                    RenderTableRow* row = m_renderTableSection.rowRendererAt(r);
                    if (row && !row->hasSelfPaintingLayer())
                        TableRowPainter(*row).paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                    for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                        RenderTableSection::CellStruct& current = m_renderTableSection.cellAt(r, c);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (r > dirtiedRows.start() && m_renderTableSection.primaryCellAt(r - 1, c) == cell) || (c > dirtiedColumns.start() && m_renderTableSection.primaryCellAt(r, c - 1) == cell))
                            continue;
                        paintCell(cell, paintInfo, paintOffset);
                    }
                }
            }
        } else {
            // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
#if ENABLE(ASSERT)
            unsigned totalRows = m_renderTableSection.numRows();
            unsigned totalCols = m_renderTableSection.table()->columns().size();
            ASSERT(overflowingCells.size() < totalRows * totalCols * gMaxAllowedOverflowingCellRatioForFastPaintPath);
#endif

            // To make sure we properly paint invalidate the section, we paint invalidated all the overflowing cells that we collected.
            Vector<RenderTableCell*> cells;
            copyToVector(overflowingCells, cells);

            HashSet<RenderTableCell*> spanningCells;

            for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
                RenderTableRow* row = m_renderTableSection.rowRendererAt(r);
                if (row && !row->hasSelfPaintingLayer())
                    TableRowPainter(*row).paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                    RenderTableSection::CellStruct& current = m_renderTableSection.cellAt(r, c);
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

            if (paintInfo.phase == PaintPhaseCollapsedTableBorders) {
                for (unsigned i = cells.size(); i > 0; --i) {
                    LayoutPoint cellPoint = m_renderTableSection.flipForWritingModeForChild(cells[i - 1], paintOffset);
                    cells[i - 1]->paintCollapsedBorders(paintInfo, cellPoint);
                }
            } else {
                for (unsigned i = 0; i < cells.size(); ++i)
                    paintCell(cells[i], paintInfo, paintOffset);
            }
        }
    }
}

void TableSectionPainter::paintCell(RenderTableCell* cell, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint cellPoint = m_renderTableSection.flipForWritingModeForChild(cell, paintOffset);
    PaintPhase paintPhase = paintInfo.phase;
    RenderTableRow* row = toRenderTableRow(cell->parent());

    if (paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) {
        // We need to handle painting a stack of backgrounds. This stack (from bottom to top) consists of
        // the column group, column, row group, row, and then the cell.
        RenderTableCol* column = m_renderTableSection.table()->colElement(cell->col());
        RenderTableCol* columnGroup = column ? column->enclosingColumnGroup() : 0;

        // Column groups and columns first.
        // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
        // the stack, since we have already opened a transparency layer (potentially) for the table row group.
        // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
        // cell.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, columnGroup);
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, column);

        // Paint the row group next.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, &m_renderTableSection);

        // Paint the row next, but only if it doesn't have a layer. If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (!row->hasSelfPaintingLayer())
            cell->paintBackgroundsBehindCell(paintInfo, cellPoint, row);
    }
    if ((!cell->hasSelfPaintingLayer() && !row->hasSelfPaintingLayer()))
        cell->paint(paintInfo, cellPoint);
}

} // namespace blink
