// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGImagePainter.h"

#include "core/layout/svg/SVGLayoutContext.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "core/rendering/ImageQualityController.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderImageResource.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "core/svg/SVGImageElement.h"
#include "platform/graphics/GraphicsContext.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

void SVGImagePainter::paint(const PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGImage);

    if (paintInfo.phase != PaintPhaseForeground
        || m_renderSVGImage.style()->visibility() == HIDDEN
        || !m_renderSVGImage.imageResource()->hasImage())
        return;

    FloatRect boundingBox = m_renderSVGImage.paintInvalidationRectInLocalCoordinates();

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_renderSVGImage.displayItemClient(), m_renderSVGImage.localToParentTransform());
    {
        SVGLayoutContext renderingContext(m_renderSVGImage, paintInfoBeforeFiltering);
        if (renderingContext.applyClipMaskAndFilterIfNecessary()) {
            RenderDrawingRecorder recorder(renderingContext.paintInfo().context, m_renderSVGImage, renderingContext.paintInfo().phase, boundingBox);
            if (!recorder.canUseCachedDrawing()) {
                if (m_renderSVGImage.style()->svgStyle().bufferedRendering() != BR_STATIC) {
                    paintForeground(renderingContext.paintInfo());
                } else {
                    RefPtr<const SkPicture>& bufferedForeground = m_renderSVGImage.bufferedForeground();
                    if (!bufferedForeground) {
                        renderingContext.paintInfo().context->beginRecording(m_renderSVGImage.objectBoundingBox());
                        paintForeground(renderingContext.paintInfo());
                        bufferedForeground = renderingContext.paintInfo().context->endRecording();
                    }

                    renderingContext.paintInfo().context->drawPicture(bufferedForeground.get());
                }
            }
        }
    }

    if (m_renderSVGImage.style()->outlineWidth())
        ObjectPainter(m_renderSVGImage).paintOutline(paintInfoBeforeFiltering, LayoutRect(boundingBox));
}

void SVGImagePainter::paintForeground(const PaintInfo& paintInfo)
{
    RefPtr<Image> image = m_renderSVGImage.imageResource()->image();
    FloatRect destRect = m_renderSVGImage.objectBoundingBox();
    FloatRect srcRect(0, 0, image->width(), image->height());

    SVGImageElement* imageElement = toSVGImageElement(m_renderSVGImage.element());
    imageElement->preserveAspectRatio()->currentValue()->transformRect(destRect, srcRect);

    InterpolationQuality interpolationQuality = InterpolationDefault;
    if (m_renderSVGImage.style()->svgStyle().bufferedRendering() != BR_STATIC)
        interpolationQuality = ImageQualityController::imageQualityController()->chooseInterpolationQuality(paintInfo.context, &m_renderSVGImage, image.get(), image.get(), LayoutSize(destRect.size()));

    InterpolationQuality previousInterpolationQuality = paintInfo.context->imageInterpolationQuality();
    paintInfo.context->setImageInterpolationQuality(interpolationQuality);
    paintInfo.context->drawImage(image.get(), destRect, srcRect, SkXfermode::kSrcOver_Mode);
    paintInfo.context->setImageInterpolationQuality(previousInterpolationQuality);
}

} // namespace blink
