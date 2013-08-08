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
#include "core/platform/graphics/GeneratorGeneratedImage.h"

#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"

#define SKIA_MAX_PATTERN_SIZE 32767

namespace WebCore {

void GeneratorGeneratedImage::draw(GraphicsContext* destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOp, BlendMode blendMode)
{
    GraphicsContextStateSaver stateSaver(*destContext);
    destContext->setCompositeOperation(compositeOp, blendMode);
    destContext->clip(destRect);
    destContext->translate(destRect.x(), destRect.y());
    if (destRect.size() != srcRect.size())
        destContext->scale(FloatSize(destRect.width() / srcRect.width(), destRect.height() / srcRect.height()));
    destContext->translate(-srcRect.x(), -srcRect.y());
    destContext->setFillGradient(m_gradient);
    destContext->fillRect(FloatRect(FloatPoint(), m_size));
}

void GeneratorGeneratedImage::drawPatternWithoutCache(GraphicsContext* destContext, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, CompositeOperator compositeOp, const FloatRect& destRect, BlendMode blendMode)
{
    int firstColumn = static_cast<int>(floorf((((destRect.x() - phase.x()) / scale.width()) - srcRect.x()) / srcRect.width()));
    int firstRow = static_cast<int>(floorf((((destRect.y() - phase.y()) / scale.height())  - srcRect.y()) / srcRect.height()));
    for (int i = firstColumn; ; ++i) {
        float dstX = (srcRect.x() + i * srcRect.width()) * scale.width() + phase.x();
        // assert that first column encroaches left edge of dstRect.
        ASSERT(i > firstColumn || dstX <= destRect.x());
        ASSERT(i == firstColumn || dstX > destRect.x());

        if (dstX >= destRect.maxX())
            break;
        float dstMaxX = dstX + srcRect.width() * scale.width();
        if (dstX < destRect.x())
            dstX = destRect.x();
        if (dstMaxX > destRect.maxX())
            dstMaxX = destRect.maxX();
        if (dstX >= dstMaxX)
            continue;

        FloatRect visibleSrcRect;
        FloatRect tileDstRect;
        tileDstRect.setX(dstX);
        tileDstRect.setWidth(dstMaxX - dstX);
        visibleSrcRect.setX((tileDstRect.x() - phase.x()) / scale.width() - i * srcRect.width());
        visibleSrcRect.setWidth(tileDstRect.width() / scale.width());

        for (int j = firstRow; ; j++) {
            float dstY = (srcRect.y() + j * srcRect.height()) * scale.height() + phase.y();
            // assert that first row encroaches top edge of dstRect.
            ASSERT(j > firstRow || dstY <= destRect.y());
            ASSERT(j == firstRow || dstY > destRect.y());

            if (dstY >= destRect.maxY())
                break;
            float dstMaxY = dstY + srcRect.height() * scale.height();
            if (dstY < destRect.y())
                dstY = destRect.y();
            if (dstMaxY > destRect.maxY())
                dstMaxY = destRect.maxY();
            if (dstY >= dstMaxY)
                continue;

            tileDstRect.setY(dstY);
            tileDstRect.setHeight(dstMaxY - dstY);
            visibleSrcRect.setY((tileDstRect.y() - phase.y()) / scale.height() - j * srcRect.height());
            visibleSrcRect.setHeight(tileDstRect.height() / scale.height());
            draw(destContext, tileDstRect, visibleSrcRect, compositeOp, blendMode);
        }
    }
}

void GeneratorGeneratedImage::drawPattern(GraphicsContext* destContext, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, CompositeOperator compositeOp, const FloatRect& destRect, BlendMode blendMode)
{
    // Allow the generator to provide visually-equivalent tiling parameters for better performance.
    IntSize adjustedSize = m_size;
    FloatRect adjustedSrcRect = srcRect;

    m_gradient->adjustParametersForTiledDrawing(adjustedSize, adjustedSrcRect);

    if (adjustedSize.width() > SKIA_MAX_PATTERN_SIZE || adjustedSize.height() > SKIA_MAX_PATTERN_SIZE) {
        // Workaround to fix crbug.com/241486
        // SkBitmapProcShader is unable to handle cached tiles >= 32k pixels high or wide.
        drawPatternWithoutCache(destContext, srcRect, scale, phase, compositeOp, destRect, blendMode);
        return;
    }

    // Factor in the destination context's scale to generate at the best resolution.
    // FIXME: No need to get the full CTM here, we just need the scale.
    AffineTransform destContextCTM = destContext->getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    float xScale = fabs(destContextCTM.xScale());
    float yScale = fabs(destContextCTM.yScale());
    FloatSize scaleWithoutCTM(scale.width() / xScale, scale.height() / yScale);
    adjustedSrcRect.scale(xScale, yScale);

    unsigned generatorHash = m_gradient->hash();

    if (!m_cachedImageBuffer || m_cachedGeneratorHash != generatorHash || m_cachedAdjustedSize != adjustedSize || !destContext->isCompatibleWithBuffer(m_cachedImageBuffer.get())) {
        m_cachedImageBuffer = destContext->createCompatibleBuffer(adjustedSize, m_gradient->hasAlpha());
        if (!m_cachedImageBuffer)
            return;

        // Fill with the generated image.
        m_cachedImageBuffer->context()->setFillGradient(m_gradient);
        m_cachedImageBuffer->context()->fillRect(FloatRect(FloatPoint(), adjustedSize));

        m_cachedGeneratorHash = generatorHash;
        m_cachedAdjustedSize = adjustedSize;
    }

    // Tile the image buffer into the context.
    m_cachedImageBuffer->drawPattern(destContext, adjustedSrcRect, scaleWithoutCTM, phase, compositeOp, destRect, blendMode);
    m_cacheTimer.restart();
}

void GeneratorGeneratedImage::invalidateCacheTimerFired(DeferrableOneShotTimer<GeneratorGeneratedImage>*)
{
    m_cachedImageBuffer.clear();
    m_cachedAdjustedSize = IntSize();
}

}
