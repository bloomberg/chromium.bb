/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/CrossfadeGeneratedImage.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/ImageBuffer.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

CrossfadeGeneratedImage::CrossfadeGeneratedImage(Image* fromImage, Image* toImage, float percentage, IntSize crossfadeSize, const IntSize& size)
    : m_fromImage(fromImage)
    , m_toImage(toImage)
    , m_percentage(percentage)
    , m_crossfadeSize(crossfadeSize)
{
    m_size = size;
}

void CrossfadeGeneratedImage::drawCrossfade(GraphicsContext* context)
{
    float inversePercentage = 1 - m_percentage;

    IntSize fromImageSize = m_fromImage->size();
    IntSize toImageSize = m_toImage->size();

    context->beginTransparencyLayer(1);

    // Draw the image we're fading away from.
    context->save();
    if (m_crossfadeSize != fromImageSize) {
        context->scale(
            static_cast<float>(m_crossfadeSize.width()) / fromImageSize.width(),
            static_cast<float>(m_crossfadeSize.height()) / fromImageSize.height());
    }
    context->setAlphaAsFloat(inversePercentage);
    context->drawImage(m_fromImage, IntPoint());
    context->restore();

    // Draw the image we're fading towards.
    context->save();
    if (m_crossfadeSize != toImageSize) {
        context->scale(
            static_cast<float>(m_crossfadeSize.width()) / toImageSize.width(),
            static_cast<float>(m_crossfadeSize.height()) / toImageSize.height());
    }
    context->setAlphaAsFloat(m_percentage);
    context->drawImage(m_toImage, IntPoint(), CompositePlusLighter);
    context->restore();

    context->endLayer();
}

void CrossfadeGeneratedImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator compositeOp, WebBlendMode blendMode)
{
    // Draw nothing if either of the images hasn't loaded yet.
    if (m_fromImage == Image::nullImage() || m_toImage == Image::nullImage())
        return;

    GraphicsContextStateSaver stateSaver(*context);
    context->setCompositeOperation(compositeOp, blendMode);
    context->clip(dstRect);
    context->translate(dstRect.x(), dstRect.y());
    if (dstRect.size() != srcRect.size())
        context->scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
    context->translate(-srcRect.x(), -srcRect.y());

    drawCrossfade(context);
}

void CrossfadeGeneratedImage::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const FloatSize& scale, const FloatPoint& phase, CompositeOperator compositeOp, const FloatRect& dstRect, WebBlendMode blendMode, const IntSize& repeatSpacing)
{
    // Draw nothing if either of the images hasn't loaded yet.
    if (m_fromImage == Image::nullImage() || m_toImage == Image::nullImage())
        return;

    FloatRect tileRect = srcRect;
    tileRect.expand(repeatSpacing);

    GraphicsContext recordingContext(nullptr, nullptr);
    recordingContext.beginRecording(tileRect);
    drawCrossfade(&recordingContext);
    RefPtr<const SkPicture> tilePicture = recordingContext.endRecording();

    // FIXME: Create GeneratedIMage::drawPicturePattern, move the following code to it,
    // and use it for GradientGeneratedImage too.
    AffineTransform patternTransform;
    patternTransform.translate(phase.x(), phase.y());
    patternTransform.scale(scale.width(), scale.height());
    patternTransform.translate(tileRect.x(), tileRect.y());

    RefPtr<Pattern> picturePattern = Pattern::createPicturePattern(tilePicture);
    picturePattern->setPatternSpaceTransform(patternTransform);

    GraphicsContextStateSaver saver(*context);
    context->setCompositeOperation(compositeOp, blendMode);
    context->setFillPattern(picturePattern);
    context->fillRect(dstRect);
}

} // namespace blink
