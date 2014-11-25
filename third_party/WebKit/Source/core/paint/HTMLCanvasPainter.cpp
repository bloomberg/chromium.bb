// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/HTMLCanvasPainter.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/paint/ClipRecorder.h"
#include "core/paint/DrawingRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderHTMLCanvas.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void HTMLCanvasPainter::paintReplaced(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext* context = paintInfo.context;

    LayoutRect contentRect = m_renderHTMLCanvas.contentBoxRect();
    contentRect.moveBy(paintOffset);
    LayoutRect paintRect = m_renderHTMLCanvas.replacedContentRect();
    paintRect.moveBy(paintOffset);

    PaintInfo localPaintInfo(paintInfo);
    OwnPtr<ClipRecorder> clipRecorder;
    bool clip = !contentRect.contains(paintRect);
    if (clip)
        clipRecorder = adoptPtr(new ClipRecorder(m_renderHTMLCanvas, paintInfo, contentRect));

    // FIXME: InterpolationNone should be used if ImageRenderingOptimizeContrast is set.
    // See bug for more details: crbug.com/353716.
    InterpolationQuality interpolationQuality = m_renderHTMLCanvas.style()->imageRendering() == ImageRenderingOptimizeContrast ? InterpolationLow : CanvasDefaultInterpolationQuality;

    HTMLCanvasElement* canvas = toHTMLCanvasElement(m_renderHTMLCanvas.node());
    LayoutSize layoutSize = contentRect.size();
    if (m_renderHTMLCanvas.style()->imageRendering() == ImageRenderingPixelated
        && (layoutSize.width() > canvas->width() || layoutSize.height() > canvas->height() || layoutSize == canvas->size())) {
        interpolationQuality = InterpolationNone;
    }

    DrawingRecorder recorder(context, &m_renderHTMLCanvas, localPaintInfo.phase, pixelSnappedIntRect(paintRect));
    InterpolationQuality previousInterpolationQuality = context->imageInterpolationQuality();
    context->setImageInterpolationQuality(interpolationQuality);
    canvas->paint(context, paintRect);
    context->setImageInterpolationQuality(previousInterpolationQuality);
}

} // namespace blink
