// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGImagePainter.h"

#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGImage.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
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
        SVGPaintContext paintContext(m_renderSVGImage, paintInfoBeforeFiltering);
        if (paintContext.applyClipMaskAndFilterIfNecessary()) {
            LayoutObjectDrawingRecorder recorder(paintContext.paintInfo().context, m_renderSVGImage, paintContext.paintInfo().phase, boundingBox);
            if (!recorder.canUseCachedDrawing()) {
                if (m_renderSVGImage.style()->svgStyle().bufferedRendering() != BR_STATIC) {
                    paintForeground(paintContext.paintInfo());
                } else {
                    RefPtr<const SkPicture>& bufferedForeground = m_renderSVGImage.bufferedForeground();
                    if (!bufferedForeground) {
                        paintContext.paintInfo().context->beginRecording(m_renderSVGImage.objectBoundingBox());
                        paintForeground(paintContext.paintInfo());
                        bufferedForeground = paintContext.paintInfo().context->endRecording();
                    }

                    paintContext.paintInfo().context->drawPicture(bufferedForeground.get());
                }
            }
        }
    }

    if (m_renderSVGImage.style()->outlineWidth()) {
        LayoutRect layoutBoundingBox(boundingBox);
        ObjectPainter(m_renderSVGImage).paintOutline(paintInfoBeforeFiltering, layoutBoundingBox, layoutBoundingBox);
    }
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
