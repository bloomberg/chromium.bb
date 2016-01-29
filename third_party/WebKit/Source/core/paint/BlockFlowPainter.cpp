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

} // namespace blink
