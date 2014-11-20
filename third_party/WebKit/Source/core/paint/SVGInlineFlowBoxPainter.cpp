// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGInlineFlowBoxPainter.h"

#include "core/paint/SVGInlineTextBoxPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/SVGInlineFlowBox.h"
#include "core/rendering/svg/SVGInlineTextBox.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGInlineFlowBoxPainter::paintSelectionBackground(const PaintInfo& paintInfo)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);

    PaintInfo childPaintInfo(paintInfo);
    for (InlineBox* child = m_svgInlineFlowBox.firstChild(); child; child = child->nextOnLine()) {
        if (child->isSVGInlineTextBox())
            SVGInlineTextBoxPainter(*toSVGInlineTextBox(child)).paintSelectionBackground(childPaintInfo);
        else if (child->isSVGInlineFlowBox())
            SVGInlineFlowBoxPainter(*toSVGInlineFlowBox(child)).paintSelectionBackground(childPaintInfo);
    }
}

void SVGInlineFlowBoxPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);

    // FIXME: SVGRenderingContext wants a non-const PaintInfo to muck with.
    PaintInfo localPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*localPaintInfo.context);
    SVGRenderingContext renderingContext(&m_svgInlineFlowBox.renderer(), localPaintInfo);
    if (renderingContext.isRenderingPrepared()) {
        for (InlineBox* child = m_svgInlineFlowBox.firstChild(); child; child = child->nextOnLine())
            child->paint(localPaintInfo, paintOffset, 0, 0);
    }
}

} // namespace blink
