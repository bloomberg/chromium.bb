// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGForeignObjectPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGForeignObject.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGRenderingContext.h"

namespace blink {

void SVGForeignObjectPainter::paint(const PaintInfo& paintInfo)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_renderSVGForeignObject.displayItemClient(), m_renderSVGForeignObject.localTransform());

    // When transitioning from SVG to block painters we need to keep the PaintInfo rect up-to-date
    // because it can be used for clipping.
    m_renderSVGForeignObject.updatePaintInfoRect(paintInfoBeforeFiltering.rect);

    OwnPtr<FloatClipRecorder> clipRecorder;
    if (SVGRenderSupport::isOverflowHidden(&m_renderSVGForeignObject))
        clipRecorder = adoptPtr(new FloatClipRecorder(*paintInfoBeforeFiltering.context, m_renderSVGForeignObject.displayItemClient(), paintInfoBeforeFiltering.phase, m_renderSVGForeignObject.viewportRect()));

    SVGRenderingContext renderingContext(m_renderSVGForeignObject, paintInfoBeforeFiltering);
    bool continueRendering = true;
    if (renderingContext.paintInfo().phase == PaintPhaseForeground)
        continueRendering = renderingContext.applyClipMaskAndFilterIfNecessary();

    if (continueRendering) {
        // Paint all phases of FO elements atomically as though the FO element established its own stacking context.
        bool preservePhase = renderingContext.paintInfo().phase == PaintPhaseSelection || renderingContext.paintInfo().phase == PaintPhaseTextClip;
        const LayoutPoint childPoint = IntPoint();
        renderingContext.paintInfo().phase = preservePhase ? renderingContext.paintInfo().phase : PaintPhaseBlockBackground;
        BlockPainter(m_renderSVGForeignObject).paint(renderingContext.paintInfo(), childPoint);
        if (!preservePhase) {
            renderingContext.paintInfo().phase = PaintPhaseChildBlockBackgrounds;
            BlockPainter(m_renderSVGForeignObject).paint(renderingContext.paintInfo(), childPoint);
            renderingContext.paintInfo().phase = PaintPhaseFloat;
            BlockPainter(m_renderSVGForeignObject).paint(renderingContext.paintInfo(), childPoint);
            renderingContext.paintInfo().phase = PaintPhaseForeground;
            BlockPainter(m_renderSVGForeignObject).paint(renderingContext.paintInfo(), childPoint);
            renderingContext.paintInfo().phase = PaintPhaseOutline;
            BlockPainter(m_renderSVGForeignObject).paint(renderingContext.paintInfo(), childPoint);
        }
    }
}

} // namespace blink
