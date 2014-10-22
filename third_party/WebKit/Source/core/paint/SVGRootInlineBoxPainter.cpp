// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGRootInlineBoxPainter.h"

#include "core/paint/SVGInlineFlowBoxPainter.h"
#include "core/paint/SVGInlineTextBoxPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/SVGInlineFlowBox.h"
#include "core/rendering/svg/SVGInlineTextBox.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGRootInlineBox.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGRootInlineBoxPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);

    bool isPrinting = m_svgRootInlineBox.renderer().document().printing();
    bool hasSelection = !isPrinting && m_svgRootInlineBox.selectionState() != RenderObject::SelectionNone;

    PaintInfo childPaintInfo(paintInfo);
    if (hasSelection) {
        for (InlineBox* child = m_svgRootInlineBox.firstChild(); child; child = child->nextOnLine()) {
            if (child->isSVGInlineTextBox())
                SVGInlineTextBoxPainter(*toSVGInlineTextBox(child)).paintSelectionBackground(childPaintInfo);
            else if (child->isSVGInlineFlowBox())
                SVGInlineFlowBoxPainter(*toSVGInlineFlowBox(child)).paintSelectionBackground(childPaintInfo);
        }
    }

    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    SVGRenderingContext renderingContext(&m_svgRootInlineBox.renderer(), paintInfo);
    if (renderingContext.isRenderingPrepared()) {
        for (InlineBox* child = m_svgRootInlineBox.firstChild(); child; child = child->nextOnLine())
            child->paint(paintInfo, paintOffset, 0, 0);
    }
}

} // namespace blink
