// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TableRowPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/TableCellPainter.h"

namespace blink {

void TableRowPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(m_layoutTableRow.hasSelfPaintingLayer());
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutTableRow);

    paintOutlineForRowIfNeeded(paintInfo, paintOffset);
    for (LayoutTableCell* cell = m_layoutTableRow.firstCell(); cell; cell = cell->nextCell()) {
        // Paint the row background behind the cell.
        if (paintInfo.phase == PaintPhaseBlockBackground || paintInfo.phase == PaintPhaseChildBlockBackground)
            TableCellPainter(*cell).paintBackgroundsBehindCell(paintInfo, paintOffset, &m_layoutTableRow);
        if (!cell->hasSelfPaintingLayer())
            cell->paint(paintInfo, paintOffset);
    }
}

void TableRowPainter::paintOutlineForRowIfNeeded(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && m_layoutTableRow.style()->visibility() == VISIBLE) {
        LayoutRect visualOverflowRect(m_layoutTableRow.visualOverflowRect());
        visualOverflowRect.moveBy(adjustedPaintOffset);
        ObjectPainter(m_layoutTableRow).paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, m_layoutTableRow.size()), visualOverflowRect);
    }
}

} // namespace blink
