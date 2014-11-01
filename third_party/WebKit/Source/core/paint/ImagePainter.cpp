// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ImagePainter.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Page.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/DrawingRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderReplaced.h"
#include "core/rendering/TextRunConstructor.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/Path.h"

namespace blink {

void ImagePainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    m_renderImage.RenderReplaced::paint(paintInfo, paintOffset);

    if (paintInfo.phase == PaintPhaseOutline)
        paintAreaElementFocusRing(paintInfo);
}

void ImagePainter::paintAreaElementFocusRing(PaintInfo& paintInfo)
{
    Document& document = m_renderImage.document();

    if (document.printing() || !document.frame()->selection().isFocusedAndActive())
        return;

    Element* focusedElement = document.focusedElement();
    if (!isHTMLAreaElement(focusedElement))
        return;

    HTMLAreaElement& areaElement = toHTMLAreaElement(*focusedElement);
    if (areaElement.imageElement() != m_renderImage.node())
        return;

    // Even if the theme handles focus ring drawing for entire elements, it won't do it for
    // an area within an image, so we don't call RenderTheme::supportsFocusRing here.

    Path path = areaElement.computePath(&m_renderImage);
    if (path.isEmpty())
        return;

    RenderStyle* areaElementStyle = areaElement.computedStyle();
    unsigned short outlineWidth = areaElementStyle->outlineWidth();
    if (!outlineWidth)
        return;

    // FIXME: Clip path instead of context when Skia pathops is ready.
    // https://crbug.com/251206
    GraphicsContextStateSaver savedContext(*paintInfo.context);
    IntRect focusRect = m_renderImage.absoluteContentBox();
    DrawingRecorder recorder(paintInfo.context, &m_renderImage, paintInfo.phase, focusRect);
    paintInfo.context->clip(focusRect);
    paintInfo.context->drawFocusRing(path, outlineWidth,
        areaElementStyle->outlineOffset(),
        m_renderImage.resolveColor(areaElementStyle, CSSPropertyOutlineColor));
}

void ImagePainter::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutUnit cWidth = m_renderImage.contentWidth();
    LayoutUnit cHeight = m_renderImage.contentHeight();

    GraphicsContext* context = paintInfo.context;

    if (!m_renderImage.imageResource()->hasImage()) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (cWidth > 2 && cHeight > 2) {
            // Draw an outline rect where the image should be.
            IntRect paintRect = pixelSnappedIntRect(LayoutRect(paintOffset.x() + m_renderImage.borderLeft() + m_renderImage.paddingLeft(), paintOffset.y() + m_renderImage.borderTop() + m_renderImage.paddingTop(), cWidth, cHeight));
            DrawingRecorder recorder(context, &m_renderImage, paintInfo.phase, paintRect);
            context->setStrokeStyle(SolidStroke);
            context->setStrokeColor(Color::lightGray);
            context->setFillColor(Color::transparent);
            context->drawRect(paintRect);
        }
    } else if (cWidth > 0 && cHeight > 0) {
        LayoutRect contentRect = m_renderImage.contentBoxRect();
        contentRect.moveBy(paintOffset);
        LayoutRect paintRect = m_renderImage.replacedContentRect();
        paintRect.moveBy(paintOffset);
        DrawingRecorder recorder(context, &m_renderImage, paintInfo.phase, contentRect);
        bool clip = !contentRect.contains(paintRect);
        if (clip) {
            context->save();
            context->clip(contentRect);
        }

        paintIntoRect(context, paintRect);

        if (clip)
            context->restore();
    }
}

void ImagePainter::paintIntoRect(GraphicsContext* context, const LayoutRect& rect)
{
    IntRect alignedRect = pixelSnappedIntRect(rect);
    if (!m_renderImage.imageResource()->hasImage() || m_renderImage.imageResource()->errorOccurred() || alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return;

    RefPtr<Image> img = m_renderImage.imageResource()->image(alignedRect.width(), alignedRect.height());
    if (!img || img->isNull())
        return;

    Image* image = img.get();
    InterpolationQuality interpolationQuality = BoxPainter::chooseInterpolationQuality(m_renderImage, context, image, image, alignedRect.size());

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(m_renderImage));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::willPaintImage(&m_renderImage);
    InterpolationQuality previousInterpolationQuality = context->imageInterpolationQuality();
    context->setImageInterpolationQuality(interpolationQuality);
    context->drawImage(image, alignedRect, CompositeSourceOver, m_renderImage.shouldRespectImageOrientation());
    context->setImageInterpolationQuality(previousInterpolationQuality);
    InspectorInstrumentation::didPaintImage(&m_renderImage);
}

} // namespace blink
