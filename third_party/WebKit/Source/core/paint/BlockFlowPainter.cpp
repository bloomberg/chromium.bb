// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BlockFlowPainter.h"

#include "core/layout/FloatingObjects.h"
#include "core/layout/Layer.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/rendering/RenderBlockFlow.h"
#include "platform/graphics/paint/ClipRecorderStack.h"

namespace blink {

void BlockFlowPainter::paintFloats(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, bool preservePhase)
{
    if (!m_renderBlockFlow.floatingObjects())
        return;

    const FloatingObjectSet& floatingObjectSet = m_renderBlockFlow.floatingObjects()->set();
    FloatingObjectSetIterator end = floatingObjectSet.end();
    for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        // Only paint the object if our m_shouldPaint flag is set.
        if (floatingObject->shouldPaint() && !floatingObject->renderer()->hasSelfPaintingLayer()) {
            PaintInfo currentPaintInfo(paintInfo);
            currentPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
            // FIXME: LayoutPoint version of xPositionForFloatIncludingMargin would make this much cleaner.
            LayoutPoint childPoint = m_renderBlockFlow.flipFloatForWritingModeForChild(
                floatingObject, LayoutPoint(paintOffset.x()
                + m_renderBlockFlow.xPositionForFloatIncludingMargin(floatingObject) - floatingObject->renderer()->location().x(), paintOffset.y()
                + m_renderBlockFlow.yPositionForFloatIncludingMargin(floatingObject) - floatingObject->renderer()->location().y()));
            floatingObject->renderer()->paint(currentPaintInfo, childPoint);
            if (!preservePhase) {
                currentPaintInfo.phase = PaintPhaseChildBlockBackgrounds;
                floatingObject->renderer()->paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseFloat;
                floatingObject->renderer()->paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseForeground;
                floatingObject->renderer()->paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseOutline;
                floatingObject->renderer()->paint(currentPaintInfo, childPoint);
            }
        }
    }
}

void BlockFlowPainter::paintSelection(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderBlockFlow.shouldPaintSelectionGaps() && paintInfo.phase == PaintPhaseForeground) {
        LayoutUnit lastTop = 0;
        LayoutUnit lastLeft = m_renderBlockFlow.logicalLeftSelectionOffset(&m_renderBlockFlow, lastTop);
        LayoutUnit lastRight = m_renderBlockFlow.logicalRightSelectionOffset(&m_renderBlockFlow, lastTop);
        ClipRecorderStack clipRecorderStack(paintInfo.context);

        LayoutRect bounds;
        if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
            bounds = m_renderBlockFlow.visualOverflowRect();
            bounds.moveBy(paintOffset);
        }
        RenderDrawingRecorder recorder(paintInfo.context, m_renderBlockFlow, DisplayItem::SelectionGap, bounds);

        LayoutRect gapRectsBounds = m_renderBlockFlow.selectionGaps(&m_renderBlockFlow, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight,
            recorder.canUseCachedDrawing() ? nullptr : &paintInfo);
        if (!gapRectsBounds.isEmpty()) {
            Layer* layer = m_renderBlockFlow.enclosingLayer();
            gapRectsBounds.moveBy(-paintOffset);
            if (!m_renderBlockFlow.hasLayer()) {
                LayoutRect localBounds(gapRectsBounds);
                m_renderBlockFlow.flipForWritingMode(localBounds);
                gapRectsBounds = m_renderBlockFlow.localToContainerQuad(FloatRect(localBounds), layer->renderer()).enclosingBoundingBox();
                if (layer->renderer()->hasOverflowClip())
                    gapRectsBounds.move(layer->renderBox()->scrolledContentOffset());
            }
            layer->addBlockSelectionGapsBounds(gapRectsBounds);
        }
    }
}

} // namespace blink
