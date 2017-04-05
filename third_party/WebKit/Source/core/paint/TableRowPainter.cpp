// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableRowPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"

namespace blink {

void TableRowPainter::paint(const PaintInfo& paintInfo,
                            const LayoutPoint& paintOffset) {
  ObjectPainter(m_layoutTableRow).checkPaintOffset(paintInfo, paintOffset);
  DCHECK(m_layoutTableRow.hasSelfPaintingLayer());

  // TODO(crbug.com/577282): This painting order is inconsistent with other
  // outlines.
  if (shouldPaintSelfOutline(paintInfo.phase))
    paintOutline(paintInfo, paintOffset);
  if (paintInfo.phase == PaintPhaseSelfOutlineOnly)
    return;

  if (shouldPaintSelfBlockBackground(paintInfo.phase)) {
    const auto* section = m_layoutTableRow.section();
    LayoutRect cullRect = LayoutRect(paintInfo.cullRect().m_rect);
    cullRect.moveBy(m_layoutTableRow.physicalLocation(section));
    LayoutRect logicalRectInSection =
        section->logicalRectForWritingModeAndDirection(cullRect);
    CellSpan dirtiedColumns =
        section->dirtiedEffectiveColumns(logicalRectInSection);
    paintBoxDecorationBackground(paintInfo, paintOffset, dirtiedColumns);
  }

  if (paintInfo.phase == PaintPhaseSelfBlockBackgroundOnly)
    return;

  PaintInfo paintInfoForCells = paintInfo.forDescendants();
  for (LayoutTableCell* cell = m_layoutTableRow.firstCell(); cell;
       cell = cell->nextCell()) {
    if (!cell->hasSelfPaintingLayer())
      cell->paint(paintInfoForCells, paintOffset);
  }
}

void TableRowPainter::paintOutline(const PaintInfo& paintInfo,
                                   const LayoutPoint& paintOffset) {
  DCHECK(shouldPaintSelfOutline(paintInfo.phase));
  LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
  ObjectPainter(m_layoutTableRow).paintOutline(paintInfo, adjustedPaintOffset);
}

void TableRowPainter::paintBoxDecorationBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset,
    const CellSpan& dirtiedColumns) {
  bool hasBackground = m_layoutTableRow.styleRef().hasBackground();
  bool hasBoxShadow = m_layoutTableRow.styleRef().boxShadow();
  if (!hasBackground && !hasBoxShadow)
    return;

  const auto* section = m_layoutTableRow.section();
  PaintResult paintResult =
      dirtiedColumns == section->fullTableEffectiveColumnSpan()
          ? FullyPainted
          : MayBeClippedByPaintDirtyRect;
  m_layoutTableRow.getMutableForPainting().updatePaintResult(
      paintResult, paintInfo.cullRect());

  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          paintInfo.context, m_layoutTableRow,
          DisplayItem::kBoxDecorationBackground))
    return;

  LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
  LayoutRect bounds =
      BoxPainter(m_layoutTableRow)
          .boundsForDrawingRecorder(paintInfo, adjustedPaintOffset);
  LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableRow,
                                       DisplayItem::kBoxDecorationBackground,
                                       bounds);
  LayoutRect paintRect(adjustedPaintOffset, m_layoutTableRow.size());

  if (hasBoxShadow) {
    BoxPainter::paintNormalBoxShadow(paintInfo, paintRect,
                                     m_layoutTableRow.styleRef());
  }

  if (hasBackground) {
    PaintInfo paintInfoForCells = paintInfo.forDescendants();
    for (auto c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
      if (const auto* cell =
              section->originatingCellAt(m_layoutTableRow.rowIndex(), c))
        paintBackgroundBehindCell(*cell, paintInfoForCells, paintOffset);
    }
  }

  if (hasBoxShadow) {
    // TODO(wangxianzhu): Calculate the inset shadow bounds by insetting
    // paintRect by half widths of collapsed borders.
    BoxPainter::paintInsetBoxShadow(paintInfo, paintRect,
                                    m_layoutTableRow.styleRef());
  }
}

void TableRowPainter::paintBackgroundBehindCell(
    const LayoutTableCell& cell,
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset) {
  DCHECK(m_layoutTableRow.styleRef().hasBackground());
  LayoutPoint cellPoint = paintOffset;
  // If the row is self painting, paintOffset is in row's coordinates, so
  // doesn't need to flip in section's blocks direction. A row doesn't have
  // flipped blocks direction.
  if (!m_layoutTableRow.hasSelfPaintingLayer()) {
    cellPoint = m_layoutTableRow.section()->flipForWritingModeForChild(
        &cell, cellPoint);
  }
  TableCellPainter(cell).paintContainerBackgroundBehindCell(
      paintInfo, cellPoint, m_layoutTableRow);
}

}  // namespace blink
