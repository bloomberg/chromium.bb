// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TableRowPainter.h"

#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderTableRow.h"

namespace blink {

void TableRowPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(m_renderTableRow.hasSelfPaintingLayer());
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderTableRow);

    paintOutlineForRowIfNeeded(paintInfo, paintOffset);
    for (RenderTableCell* cell = m_renderTableRow.firstCell(); cell; cell = cell->nextCell()) {
        // Paint the row background behind the cell.
        if (paintInfo.phase == PaintPhaseBlockBackground || paintInfo.phase == PaintPhaseChildBlockBackground)
            cell->paintBackgroundsBehindCell(paintInfo, paintOffset, &m_renderTableRow);
        if (!cell->hasSelfPaintingLayer())
            cell->paint(paintInfo, paintOffset);
    }
}

void TableRowPainter::paintOutlineForRowIfNeeded(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_renderTableRow.location();
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && m_renderTableRow.style()->visibility() == VISIBLE)
        m_renderTableRow.paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, m_renderTableRow.size()));
}

} // namespace blink
