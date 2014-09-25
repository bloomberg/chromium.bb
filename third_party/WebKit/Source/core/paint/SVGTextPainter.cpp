// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGTextPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGText.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGTextPainter::paint(PaintInfo& paintInfo)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    PaintInfo blockInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*blockInfo.context, false);

    blockInfo.applyTransform(m_renderSVGText.localToParentTransform(), &stateSaver);

    BlockPainter(m_renderSVGText).paint(blockInfo, LayoutPoint());

    // Paint the outlines, if any
    if (paintInfo.phase == PaintPhaseForeground) {
        blockInfo.phase = PaintPhaseSelfOutline;
        BlockPainter(m_renderSVGText).paint(blockInfo, LayoutPoint());
    }
}

} // namespace blink
