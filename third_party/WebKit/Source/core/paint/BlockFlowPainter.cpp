// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BlockFlowPainter.h"

#include "core/layout/FloatingObjects.h"
#include "core/layout/Layer.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "platform/graphics/paint/ClipRecorderStack.h"

namespace blink {

void BlockFlowPainter::paintFloats(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, bool preservePhase)
{
    if (!m_layoutBlockFlow.floatingObjects())
        return;

    const FloatingObjectSet& floatingObjectSet = m_layoutBlockFlow.floatingObjects()->set();
    FloatingObjectSetIterator end = floatingObjectSet.end();
    for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        // Only paint the object if our m_shouldPaint flag is set.
        if (floatingObject->shouldPaint() && !floatingObject->layoutObject()->hasSelfPaintingLayer()) {
            PaintInfo currentPaintInfo(paintInfo);
            currentPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
            // FIXME: LayoutPoint version of xPositionForFloatIncludingMargin would make this much cleaner.
            LayoutPoint childPoint = m_layoutBlockFlow.flipFloatForWritingModeForChild(
                floatingObject, LayoutPoint(paintOffset.x()
                + m_layoutBlockFlow.xPositionForFloatIncludingMargin(floatingObject) - floatingObject->layoutObject()->location().x(), paintOffset.y()
                + m_layoutBlockFlow.yPositionForFloatIncludingMargin(floatingObject) - floatingObject->layoutObject()->location().y()));
            floatingObject->layoutObject()->paint(currentPaintInfo, childPoint);
            if (!preservePhase) {
                currentPaintInfo.phase = PaintPhaseChildBlockBackgrounds;
                floatingObject->layoutObject()->paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseFloat;
                floatingObject->layoutObject()->paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseForeground;
                floatingObject->layoutObject()->paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseOutline;
                floatingObject->layoutObject()->paint(currentPaintInfo, childPoint);
            }
        }
    }
}

void BlockFlowPainter::paintSelection(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_layoutBlockFlow.shouldPaintSelectionGaps() && paintInfo.phase == PaintPhaseForeground) {
        LayoutUnit lastTop = 0;
        LayoutUnit lastLeft = m_layoutBlockFlow.logicalLeftSelectionOffset(&m_layoutBlockFlow, lastTop);
        LayoutUnit lastRight = m_layoutBlockFlow.logicalRightSelectionOffset(&m_layoutBlockFlow, lastTop);
        ClipRecorderStack clipRecorderStack(paintInfo.context);

        LayoutRect bounds;
        if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
            bounds = m_layoutBlockFlow.visualOverflowRect();
            bounds.moveBy(paintOffset);
        }
        LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutBlockFlow, DisplayItem::SelectionGap, bounds);

        LayoutRect gapRectsBounds = m_layoutBlockFlow.selectionGaps(&m_layoutBlockFlow, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight,
            recorder.canUseCachedDrawing() ? nullptr : &paintInfo);
        if (!gapRectsBounds.isEmpty()) {
            Layer* layer = m_layoutBlockFlow.enclosingLayer();
            gapRectsBounds.moveBy(-paintOffset);
            if (!m_layoutBlockFlow.hasLayer()) {
                LayoutRect localBounds(gapRectsBounds);
                m_layoutBlockFlow.flipForWritingMode(localBounds);
                gapRectsBounds = LayoutRect(m_layoutBlockFlow.localToContainerQuad(FloatRect(localBounds), layer->layoutObject()).enclosingBoundingBox());
                if (layer->layoutObject()->hasOverflowClip())
                    gapRectsBounds.move(layer->layoutBox()->scrolledContentOffset());
            }
            layer->addBlockSelectionGapsBounds(gapRectsBounds);
        }
    }
}

} // namespace blink
