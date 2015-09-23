// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TableRowPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"

namespace blink {

void TableRowPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(m_layoutTableRow.hasSelfPaintingLayer());

    paintOutlineForRowIfNeeded(paintInfo, paintOffset);
    for (LayoutTableCell* cell = m_layoutTableRow.firstCell(); cell; cell = cell->nextCell()) {
        // Paint the row background behind the cell.
        if (paintInfo.phase == PaintPhaseBlockBackground || paintInfo.phase == PaintPhaseChildBlockBackground) {
            if (m_layoutTableRow.hasBackground()) {
                TableCellPainter tableCellPainter(*cell);
                if (!LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(*paintInfo.context, *cell, DisplayItem::TableCellBackgroundFromSelfPaintingRow, paintOffset)) {
                    LayoutObjectDrawingRecorder recorder(*paintInfo.context, *cell, DisplayItem::TableCellBackgroundFromSelfPaintingRow, tableCellPainter.paintBounds(paintOffset, TableCellPainter::AddOffsetFromParent), paintOffset);
                    tableCellPainter.paintBackgroundsBehindCell(paintInfo, paintOffset, &m_layoutTableRow);
                }
            }
        }

        if (!cell->hasSelfPaintingLayer())
            cell->paint(paintInfo, paintOffset);
    }
}

void TableRowPainter::paintOutlineForRowIfNeeded(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && m_layoutTableRow.style()->visibility() == VISIBLE) {
        LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
        ObjectPainter(m_layoutTableRow).paintOutline(paintInfo, adjustedPaintOffset);
    }
}

} // namespace blink
