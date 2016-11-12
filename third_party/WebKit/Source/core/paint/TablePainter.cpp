// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TablePainter.h"

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableSection.h"
#include "core/style/CollapsedBorderValue.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableSectionPainter.h"

namespace blink {

void TablePainter::paintObject(const PaintInfo& paintInfo,
                               const LayoutPoint& paintOffset) {
  PaintPhase paintPhase = paintInfo.phase;

  if (shouldPaintSelfBlockBackground(paintPhase)) {
    paintBoxDecorationBackground(paintInfo, paintOffset);
    if (paintPhase == PaintPhaseSelfBlockBackgroundOnly)
      return;
  }

  if (paintPhase == PaintPhaseMask) {
    paintMask(paintInfo, paintOffset);
    return;
  }

  if (paintPhase != PaintPhaseSelfOutlineOnly) {
    PaintInfo paintInfoForDescendants = paintInfo.forDescendants();

    for (LayoutObject* child = m_layoutTable.firstChild(); child;
         child = child->nextSibling()) {
      if (child->isBox() && !toLayoutBox(child)->hasSelfPaintingLayer() &&
          (child->isTableSection() || child->isTableCaption())) {
        LayoutPoint childPoint = m_layoutTable.flipForWritingModeForChild(
            toLayoutBox(child), paintOffset);
        child->paint(paintInfoForDescendants, childPoint);
      }
    }

    if (shouldPaintDescendantBlockBackgrounds(paintPhase))
      paintCollapsedBorders(paintInfoForDescendants, paintOffset);
  }

  if (shouldPaintSelfOutline(paintPhase))
    ObjectPainter(m_layoutTable).paintOutline(paintInfo, paintOffset);
}

void TablePainter::paintCollapsedBorders(const PaintInfo& paintInfo,
                                         const LayoutPoint& paintOffset) {
  if (!m_layoutTable.hasCollapsedBorders() ||
      m_layoutTable.style()->visibility() != EVisibility::Visible)
    return;

  LayoutTable::CollapsedBordersInfo& collapsedBorders =
      m_layoutTable.getCollapsedBordersInfo();

  // Normally we don't clip individual display items by paint dirty rect
  // (aka interest rect), to keep display items independent with paint dirty
  // rect so we can just depend on paint invalidation status to repaint them.
  // However, the collapsed border display item may be too big to contain all
  // collapsed borders in a huge table, so we clip it to paint dirty rect.
  // We need to invalidate the display item if the previous paint is clipped
  // and the paint dirty rect changed.
  if (collapsedBorders.lastPaintResult != FullyPainted &&
      collapsedBorders.lastPaintRect != paintInfo.cullRect())
    m_layoutTable.setDisplayItemsUncached();

  if (DrawingRecorder::useCachedDrawingIfPossible(
          paintInfo.context, m_layoutTable,
          DisplayItem::kTableCollapsedBorders))
    return;

  DrawingRecorder recorder(
      paintInfo.context, m_layoutTable, DisplayItem::kTableCollapsedBorders,
      FloatRect(LayoutRect(paintOffset, m_layoutTable.size())));

  // Using our cached sorted styles, we then do individual passes, painting
  // each style of border from lowest precedence to highest precedence.
  PaintResult paintResult = FullyPainted;
  for (const auto& borderValue : collapsedBorders.values) {
    for (LayoutTableSection* section = m_layoutTable.bottomSection(); section;
         section = m_layoutTable.sectionAbove(section)) {
      LayoutPoint childPoint =
          m_layoutTable.flipForWritingModeForChild(section, paintOffset);
      if (TableSectionPainter(*section).paintCollapsedBorders(
              paintInfo, childPoint, borderValue) ==
          MayBeClippedByPaintDirtyRect)
        paintResult = MayBeClippedByPaintDirtyRect;
    }
  }
  collapsedBorders.lastPaintResult = paintResult;
  collapsedBorders.lastPaintRect = paintInfo.cullRect();
}

void TablePainter::paintBoxDecorationBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset) {
  if (!m_layoutTable.hasBoxDecorationBackground() ||
      m_layoutTable.style()->visibility() != EVisibility::Visible)
    return;

  LayoutRect rect(paintOffset, m_layoutTable.size());
  m_layoutTable.subtractCaptionRect(rect);
  BoxPainter(m_layoutTable)
      .paintBoxDecorationBackgroundWithRect(paintInfo, paintOffset, rect);
}

void TablePainter::paintMask(const PaintInfo& paintInfo,
                             const LayoutPoint& paintOffset) {
  if (m_layoutTable.style()->visibility() != EVisibility::Visible ||
      paintInfo.phase != PaintPhaseMask)
    return;

  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          paintInfo.context, m_layoutTable, paintInfo.phase))
    return;

  LayoutRect rect(paintOffset, m_layoutTable.size());
  m_layoutTable.subtractCaptionRect(rect);

  LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTable,
                                       paintInfo.phase, rect);
  BoxPainter(m_layoutTable).paintMaskImages(paintInfo, rect);
}

}  // namespace blink
