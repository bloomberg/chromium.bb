// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockFlowPainter.h"

#include "core/layout/FloatingObjects.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/paint/ClipScope.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"

namespace blink {

void BlockFlowPainter::paintFloats(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_layoutBlockFlow.floatingObjects())
        return;

    ASSERT(paintInfo.phase == PaintPhaseFloat || paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip);
    PaintInfo floatPaintInfo(paintInfo);
    if (paintInfo.phase == PaintPhaseFloat)
        floatPaintInfo.phase = PaintPhaseForeground;

    for (const auto& floatingObject : m_layoutBlockFlow.floatingObjects()->set()) {
        if (!floatingObject->shouldPaint())
            continue;

        const LayoutBox* floatingLayoutObject = floatingObject->layoutObject();
        if (floatingLayoutObject->hasSelfPaintingLayer())
            continue;

        // FIXME: LayoutPoint version of xPositionForFloatIncludingMargin would make this much cleaner.
        LayoutPoint childPoint = m_layoutBlockFlow.flipFloatForWritingModeForChild(
            *floatingObject, LayoutPoint(paintOffset.x()
            + m_layoutBlockFlow.xPositionForFloatIncludingMargin(*floatingObject) - floatingLayoutObject->location().x(), paintOffset.y()
            + m_layoutBlockFlow.yPositionForFloatIncludingMargin(*floatingObject) - floatingLayoutObject->location().y()));
        ObjectPainter(*floatingLayoutObject).paintAsPseudoStackingContext(floatPaintInfo, childPoint);
    }
}

void BlockFlowPainter::paintSelection(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground);
    if (!m_layoutBlockFlow.shouldPaintSelectionGaps())
        return;

    LayoutUnit lastTop = 0;
    LayoutUnit lastLeft = m_layoutBlockFlow.logicalLeftSelectionOffset(&m_layoutBlockFlow, lastTop);
    LayoutUnit lastRight = m_layoutBlockFlow.logicalRightSelectionOffset(&m_layoutBlockFlow, lastTop);

    LayoutRect bounds = m_layoutBlockFlow.visualOverflowRect();
    bounds.moveBy(paintOffset);

    // Only create a DrawingRecorder and ClipScope if skipRecording is false. This logic is needed
    // because selectionGaps(...) needs to be called even when we do not record.
    bool skipRecording = LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutBlockFlow, DisplayItem::SelectionGap, paintOffset);
    Optional<LayoutObjectDrawingRecorder> drawingRecorder;
    Optional<ClipScope> clipScope;
    if (!skipRecording) {
        drawingRecorder.emplace(paintInfo.context, m_layoutBlockFlow, DisplayItem::SelectionGap, FloatRect(bounds), paintOffset);
        clipScope.emplace(paintInfo.context);
    }

    LayoutRect gapRectsBounds = m_layoutBlockFlow.selectionGaps(&m_layoutBlockFlow, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight,
        skipRecording ? nullptr : &paintInfo,
        skipRecording ? nullptr : &(*clipScope));
    // TODO(wkorman): Rework below to process paint invalidation rects during layout rather than paint.
    if (!gapRectsBounds.isEmpty()) {
        PaintLayer* layer = m_layoutBlockFlow.enclosingLayer();
        gapRectsBounds.moveBy(-paintOffset);
        if (!m_layoutBlockFlow.hasLayer()) {
            LayoutRect localBounds(gapRectsBounds);
            m_layoutBlockFlow.flipForWritingMode(localBounds);
            gapRectsBounds = LayoutRect(m_layoutBlockFlow.localToAncestorQuad(FloatRect(localBounds), layer->layoutObject()).enclosingBoundingBox());
            if (layer->layoutObject()->hasOverflowClip())
                gapRectsBounds.move(layer->layoutBox()->scrolledContentOffset());
        }
        layer->addBlockSelectionGapsBounds(gapRectsBounds);
    }
}

} // namespace blink
