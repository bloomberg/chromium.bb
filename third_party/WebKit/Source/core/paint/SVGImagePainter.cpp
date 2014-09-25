// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGImagePainter.h"

#include "core/paint/ObjectPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/ImageQualityController.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderImageResource.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGImageElement.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGImagePainter::paint(PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGImage);

    if (paintInfo.phase != PaintPhaseForeground
        || m_renderSVGImage.style()->visibility() == HIDDEN
        || !m_renderSVGImage.imageResource()->hasImage())
        return;

    FloatRect boundingBox = m_renderSVGImage.paintInvalidationRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsPaintInvalidationRect(boundingBox, m_renderSVGImage.localToParentTransform(), paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*childPaintInfo.context, false);

    childPaintInfo.applyTransform(m_renderSVGImage.localToParentTransform(), &stateSaver);

    if (!m_renderSVGImage.objectBoundingBox().isEmpty()) {
        // SVGRenderingContext may taint the state - make sure we're always saving.
        stateSaver.saveIfNeeded();

        SVGRenderingContext renderingContext(&m_renderSVGImage, childPaintInfo);
        if (renderingContext.isRenderingPrepared()) {
            if (m_renderSVGImage.style()->svgStyle().bufferedRendering() == BR_STATIC && renderingContext.bufferForeground(m_renderSVGImage.bufferedForeground()))
                return;

            paintForeground(m_renderSVGImage, childPaintInfo);
        }
    }

    if (m_renderSVGImage.style()->outlineWidth())
        ObjectPainter(m_renderSVGImage).paintOutline(childPaintInfo, IntRect(boundingBox));
}

void SVGImagePainter::paintForeground(RenderSVGImage& renderer, PaintInfo& paintInfo)
{
    RefPtr<Image> image = renderer.imageResource()->image();
    FloatRect destRect = renderer.objectBoundingBox();
    FloatRect srcRect(0, 0, image->width(), image->height());

    SVGImageElement* imageElement = toSVGImageElement(renderer.element());
    imageElement->preserveAspectRatio()->currentValue()->transformRect(destRect, srcRect);

    InterpolationQuality interpolationQuality = InterpolationDefault;
    if (renderer.style()->svgStyle().bufferedRendering() != BR_STATIC)
        interpolationQuality = ImageQualityController::imageQualityController()->chooseInterpolationQuality(paintInfo.context, &renderer, image.get(), image.get(), LayoutSize(destRect.size()));

    InterpolationQuality previousInterpolationQuality = paintInfo.context->imageInterpolationQuality();
    paintInfo.context->setImageInterpolationQuality(interpolationQuality);
    paintInfo.context->drawImage(image.get(), destRect, srcRect, CompositeSourceOver);
    paintInfo.context->setImageInterpolationQuality(previousInterpolationQuality);
}

} // namespace blink
