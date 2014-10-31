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
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGImagePainter::paint(PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGImage);

    if (paintInfo.phase != PaintPhaseForeground
        || m_renderSVGImage.style()->visibility() == HIDDEN
        || !m_renderSVGImage.imageResource()->hasImage())
        return;

    FloatRect invalBox = m_renderSVGImage.paintInvalidationRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsPaintInvalidationRect(invalBox, m_renderSVGImage.localToParentTransform(), paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*childPaintInfo.context, false);

    childPaintInfo.applyTransform(m_renderSVGImage.localToParentTransform(), &stateSaver);

    FloatRect boundingBox = m_renderSVGImage.objectBoundingBox();
    if (!boundingBox.isEmpty()) {
        // SVGRenderingContext may taint the state - make sure we're always saving.
        stateSaver.saveIfNeeded();

        SVGRenderingContext renderingContext(&m_renderSVGImage, childPaintInfo);
        if (renderingContext.isRenderingPrepared()) {
            if (m_renderSVGImage.style()->svgStyle().bufferedRendering() != BR_STATIC) {
                paintForeground(childPaintInfo);
            } else {
                RefPtr<DisplayList>& bufferedForeground = m_renderSVGImage.bufferedForeground();
                if (!bufferedForeground) {
                    childPaintInfo.context->beginRecording(boundingBox);
                    paintForeground(childPaintInfo);
                    bufferedForeground = childPaintInfo.context->endRecording();
                }

                childPaintInfo.context->drawDisplayList(bufferedForeground.get());
            }
        }
    }

    if (m_renderSVGImage.style()->outlineWidth())
        ObjectPainter(m_renderSVGImage).paintOutline(childPaintInfo, IntRect(invalBox));
}

void SVGImagePainter::paintForeground(PaintInfo& paintInfo)
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
    paintInfo.context->drawImage(image.get(), destRect, srcRect, CompositeSourceOver);
    paintInfo.context->setImageInterpolationQuality(previousInterpolationQuality);
}

} // namespace blink
