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

    if (!m_renderImage.imageResource()->hasImage() || m_renderImage.imageResource()->errorOccurred()) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (cWidth > 2 && cHeight > 2) {
            const int borderWidth = 1;

            LayoutUnit leftBorder = m_renderImage.borderLeft();
            LayoutUnit topBorder = m_renderImage.borderTop();
            LayoutUnit leftPad = m_renderImage.paddingLeft();
            LayoutUnit topPad = m_renderImage.paddingTop();

            // Draw an outline rect where the image should be.
            IntRect paintRect = pixelSnappedIntRect(LayoutRect(paintOffset.x() + leftBorder + leftPad, paintOffset.y() + topBorder + topPad, cWidth, cHeight));
            DrawingRecorder recorder(context, &m_renderImage, paintInfo.phase, paintRect);
            context->setStrokeStyle(SolidStroke);
            context->setStrokeColor(Color::lightGray);
            context->setFillColor(Color::transparent);
            context->drawRect(paintRect);

            bool errorPictureDrawn = false;
            LayoutSize imageOffset;
            // When calculating the usable dimensions, exclude the pixels of
            // the ouline rect so the error image/alt text doesn't draw on it.
            LayoutUnit usableWidth = cWidth - 2 * borderWidth;
            LayoutUnit usableHeight = cHeight - 2 * borderWidth;

            RefPtr<Image> image = m_renderImage.imageResource()->image();

            if (m_renderImage.imageResource()->errorOccurred() && !image->isNull() && usableWidth >= image->width() && usableHeight >= image->height()) {
                float deviceScaleFactor = blink::deviceScaleFactor(m_renderImage.frame());
                // Call brokenImage() explicitly to ensure we get the broken image icon at the appropriate resolution.
                pair<Image*, float> brokenImageAndImageScaleFactor = ImageResource::brokenImage(deviceScaleFactor);
                image = brokenImageAndImageScaleFactor.first;
                IntSize imageSize = image->size();
                imageSize.scale(1 / brokenImageAndImageScaleFactor.second);
                // Center the error image, accounting for border and padding.
                LayoutUnit centerX = (usableWidth - imageSize.width()) / 2;
                if (centerX < 0)
                    centerX = 0;
                LayoutUnit centerY = (usableHeight - imageSize.height()) / 2;
                if (centerY < 0)
                    centerY = 0;
                imageOffset = LayoutSize(leftBorder + leftPad + centerX + borderWidth, topBorder + topPad + centerY + borderWidth);
                context->drawImage(image.get(), pixelSnappedIntRect(LayoutRect(paintOffset + imageOffset, imageSize)), CompositeSourceOver, m_renderImage.shouldRespectImageOrientation());
                errorPictureDrawn = true;
            }

            if (!m_renderImage.altText().isEmpty()) {
                const Font& font = m_renderImage.style()->font();
                const FontMetrics& fontMetrics = font.fontMetrics();
                LayoutUnit ascent = fontMetrics.ascent();
                LayoutPoint textRectOrigin = paintOffset;
                textRectOrigin.move(leftBorder + leftPad + (RenderImage::paddingWidth / 2) - borderWidth, topBorder + topPad + (RenderImage::paddingHeight / 2) - borderWidth);
                LayoutPoint textOrigin(textRectOrigin.x(), textRectOrigin.y() + ascent);

                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                TextRun textRun = constructTextRun(&m_renderImage, font, m_renderImage.altText(), m_renderImage.style(), TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion, DefaultTextRunFlags | RespectDirection);
                float textWidth = font.width(textRun);
                TextRunPaintInfo textRunPaintInfo(textRun);
                textRunPaintInfo.bounds = FloatRect(textRectOrigin, FloatSize(textWidth, fontMetrics.height()));
                context->setFillColor(m_renderImage.resolveColor(CSSPropertyColor));
                if (textRun.direction() == RTL) {
                    int availableWidth = cWidth - static_cast<int>(RenderImage::paddingWidth);
                    textOrigin.move(availableWidth - ceilf(textWidth), 0);
                }
                if (errorPictureDrawn) {
                    if (usableWidth >= textWidth && fontMetrics.height() <= imageOffset.height())
                        context->drawBidiText(font, textRunPaintInfo, textOrigin);
                } else if (usableWidth >= textWidth && usableHeight >= fontMetrics.height()) {
                    context->drawBidiText(font, textRunPaintInfo, textOrigin);
                }
            }
        }
    } else if (m_renderImage.imageResource()->hasImage() && cWidth > 0 && cHeight > 0) {
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
    if (!m_renderImage.imageResource()->hasImage() || m_renderImage.imageResource()->errorOccurred())
        return; // FIXME: should we just ASSERT these conditions? (audit all callers).

    IntRect alignedRect = pixelSnappedIntRect(rect);
    if (alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return;

    RefPtr<Image> image = m_renderImage.imageResource()->image(alignedRect.width(), alignedRect.height());
    if (!image || image->isNull())
        return;

    // FIXME: why is interpolation quality selection not included in the Instrumentation reported cost of drawing an image?
    InterpolationQuality interpolationQuality = BoxPainter::chooseInterpolationQuality(m_renderImage, context, image.get(), image.get(), alignedRect.size());

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(m_renderImage));
    // FIXME: crbug.com/361045 remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::willPaintImage(&m_renderImage);

    InterpolationQuality previousInterpolationQuality = context->imageInterpolationQuality();
    context->setImageInterpolationQuality(interpolationQuality);
    context->drawImage(image.get(), alignedRect, CompositeSourceOver, m_renderImage.shouldRespectImageOrientation());
    context->setImageInterpolationQuality(previousInterpolationQuality);

    InspectorInstrumentation::didPaintImage(&m_renderImage);
}

void ImagePainter::paintBoxDecorationBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Don't anti-alias the background of an image. See crbug.com/423834 for the reason why.
    // However, don't turn off anti-aliasing for subclasses such as video.
    // An additional mega-hack here is to turn off anti-aliasing only for situations with a large device scale factor
    // The referenced bug gives more details about a device with a scale factor greater than 3.4).
    bool shouldAntialias = !m_renderImage.isRenderImage() || blink::deviceScaleFactor(m_renderImage.frame()) < 3.4;

    bool oldShouldAntialias = paintInfo.context->shouldAntialias();
    paintInfo.context->setShouldAntialias(shouldAntialias);
    BoxPainter(m_renderImage).paintBoxDecorationBackground(paintInfo, paintOffset);

    paintInfo.context->setShouldAntialias(oldShouldAntialias);
}

} // namespace blink
