/*
 * Copyright (C) 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/GradientGeneratedImage.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

void GradientGeneratedImage::draw(GraphicsContext* destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOp, WebBlendMode blendMode)
{
    GraphicsContextStateSaver stateSaver(*destContext);
    destContext->setCompositeOperation(compositeOp, blendMode);
    destContext->clip(destRect);
    destContext->translate(destRect.x(), destRect.y());
    if (destRect.size() != srcRect.size())
        destContext->scale(destRect.width() / srcRect.width(), destRect.height() / srcRect.height());
    destContext->translate(-srcRect.x(), -srcRect.y());
    destContext->setFillGradient(m_gradient);
    destContext->fillRect(FloatRect(FloatPoint(), m_size));
}

void GradientGeneratedImage::drawPattern(GraphicsContext* destContext, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, CompositeOperator compositeOp, const FloatRect& destRect, WebBlendMode blendMode, const IntSize& repeatSpacing)
{
    FloatRect tileRect = srcRect;
    tileRect.expand(repeatSpacing);

    GraphicsContext recordingContext(nullptr, nullptr);
    recordingContext.beginRecording(tileRect);
    recordingContext.setFillGradient(m_gradient);
    recordingContext.fillRect(srcRect);
    RefPtr<const SkPicture> tilePicture = recordingContext.endRecording();

    AffineTransform patternTransform;
    patternTransform.translate(phase.x(), phase.y());
    patternTransform.scale(scale.width(), scale.height());
    patternTransform.translate(tileRect.x(), tileRect.y());

    RefPtr<Pattern> picturePattern = Pattern::createPicturePattern(tilePicture);
    picturePattern->setPatternSpaceTransform(patternTransform);

    GraphicsContextStateSaver saver(*destContext);
    destContext->setCompositeOperation(compositeOp, blendMode);
    destContext->setFillPattern(picturePattern);
    destContext->fillRect(destRect);
}

} // namespace blink
