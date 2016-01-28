// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    // TODO(wangxianzhu): This painting order is inconsistent with other outlines. crbug.com/577282.
    paintOutlineForRowIfNeeded(paintInfo, paintOffset);
    if (paintInfo.phase == PaintPhaseSelfOutlineOnly)
        return;

    PaintInfo paintInfoForCells = paintInfo.forDescendants();
    bool shouldPaintRowBackground = shouldPaintSelfBlockBackground(paintInfo.phase) && m_layoutTableRow.hasBackground();
    bool shouldPaintCells = paintInfo.phase != PaintPhaseSelfBlockBackgroundOnly;
    for (LayoutTableCell* cell = m_layoutTableRow.firstCell(); cell; cell = cell->nextCell()) {
        if (shouldPaintRowBackground)
            TableCellPainter(*cell).paintBackgroundsBehindCell(paintInfoForCells, paintOffset, &m_layoutTableRow, DisplayItem::TableCellBackgroundFromRow);
        if (shouldPaintCells && !cell->hasSelfPaintingLayer())
            cell->paint(paintInfoForCells, paintOffset);
    }
}

void TableRowPainter::paintOutlineForRowIfNeeded(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if (shouldPaintSelfOutline(paintPhase)) {
        LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
        ObjectPainter(m_layoutTableRow).paintOutline(paintInfo, adjustedPaintOffset);
    }
}

} // namespace blink
