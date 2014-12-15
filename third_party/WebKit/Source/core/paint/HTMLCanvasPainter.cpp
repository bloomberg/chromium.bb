// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/HTMLCanvasPainter.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderHTMLCanvas.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipRecorder.h"

namespace blink {

void HTMLCanvasPainter::paintReplaced(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext* context = paintInfo.context;

    LayoutRect contentRect = m_renderHTMLCanvas.contentBoxRect();
    contentRect.moveBy(paintOffset);
    LayoutRect paintRect = m_renderHTMLCanvas.replacedContentRect();
    paintRect.moveBy(paintOffset);

    bool clip = !contentRect.contains(paintRect);
    if (clip) {
        context->save();
        context->clip(contentRect);
    }

    // FIXME: InterpolationNone should be used if ImageRenderingOptimizeContrast is set.
    // See bug for more details: crbug.com/353716.
    InterpolationQuality interpolationQuality = m_renderHTMLCanvas.style()->imageRendering() == ImageRenderingOptimizeContrast ? InterpolationLow : CanvasDefaultInterpolationQuality;
    if (m_renderHTMLCanvas.style()->imageRendering() == ImageRenderingPixelated)
        interpolationQuality = InterpolationNone;

    InterpolationQuality previousInterpolationQuality = context->imageInterpolationQuality();
    context->setImageInterpolationQuality(interpolationQuality);
    toHTMLCanvasElement(m_renderHTMLCanvas.node())->paint(context, paintRect);
    context->setImageInterpolationQuality(previousInterpolationQuality);

    if (clip)
        context->restore();
}

} // namespace blink
