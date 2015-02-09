/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/skia/NativeImageSkia.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

void NativeImageSkia::draw(
    GraphicsContext* context,
    const SkRect& srcRect,
    const SkRect& destRect,
    SkXfermode::Mode op) const
{
    TRACE_EVENT0("skia", "NativeImageSkia::draw");

    bool isLazyDecoded = DeferredImageDecoder::isLazyDecoded(bitmap());
    bool isOpaque = bitmap().isOpaque();

    {
        SkPaint paint;
        int initialSaveCount = context->preparePaintForDrawRectToRect(&paint, srcRect, destRect, op, !isOpaque, isLazyDecoded, isDataComplete());
        // We want to filter it if we decided to do interpolation above, or if
        // there is something interesting going on with the matrix (like a rotation).
        // Note: for serialization, we will want to subset the bitmap first so we
        // don't send extra pixels.
        context->drawBitmapRect(bitmap(), &srcRect, destRect, &paint);
        context->canvas()->restoreToCount(initialSaveCount);
    }

    if (isLazyDecoded)
        PlatformInstrumentation::didDrawLazyPixelRef(bitmap().getGenerationID());
}

static SkBitmap createBitmapWithSpace(const SkBitmap& bitmap, int spaceWidth, int spaceHeight)
{
    SkImageInfo info = bitmap.info();
    info = SkImageInfo::Make(info.width() + spaceWidth, info.height() + spaceHeight, info.colorType(), kPremul_SkAlphaType);

    SkBitmap result;
    result.allocPixels(info);
    result.eraseColor(SK_ColorTRANSPARENT);
    bitmap.copyPixelsTo(reinterpret_cast<uint8_t*>(result.getPixels()), result.rowBytes() * result.height(), result.rowBytes());

    return result;
}

void NativeImageSkia::drawPattern(
    GraphicsContext* context,
    const FloatRect& floatSrcRect,
    const FloatSize& scale,
    const FloatPoint& phase,
    SkXfermode::Mode compositeOp,
    const FloatRect& destRect,
    const IntSize& repeatSpacing) const
{
    FloatRect normSrcRect = floatSrcRect;
    normSrcRect.intersect(FloatRect(0, 0, bitmap().width(), bitmap().height()));
    if (destRect.isEmpty() || normSrcRect.isEmpty())
        return; // nothing to draw

    SkMatrix localMatrix;
    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the pattern. If WebKit wants
    // a shifted image, it will shift it from there using the localMatrix.
    const float adjustedX = phase.x() + normSrcRect.x() * scale.width();
    const float adjustedY = phase.y() + normSrcRect.y() * scale.height();
    localMatrix.setTranslate(SkFloatToScalar(adjustedX), SkFloatToScalar(adjustedY));

    // Because no resizing occurred, the shader transform should be
    // set to the pattern's transform, which just includes scale.
    localMatrix.preScale(scale.width(), scale.height());

    SkBitmap bitmapToPaint;
    bitmap().extractSubset(&bitmapToPaint, enclosingIntRect(normSrcRect));
    if (!repeatSpacing.isZero()) {
        SkScalar ctmScaleX = 1.0;
        SkScalar ctmScaleY = 1.0;

        if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
            AffineTransform ctm = context->getCTM();
            ctmScaleX = ctm.xScale();
            ctmScaleY = ctm.yScale();
        }

        bitmapToPaint = createBitmapWithSpace(
            bitmapToPaint,
            repeatSpacing.width() * ctmScaleX / scale.width(),
            repeatSpacing.height() * ctmScaleY / scale.height());
    }
    RefPtr<SkShader> shader = adoptRef(SkShader::CreateBitmapShader(bitmapToPaint, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));

    bool isLazyDecoded = DeferredImageDecoder::isLazyDecoded(bitmap());
    {
        SkPaint paint;
        int initialSaveCount = context->preparePaintForDrawRectToRect(&paint, floatSrcRect,
            destRect, compositeOp, !bitmap().isOpaque(), isLazyDecoded, isDataComplete());
        paint.setShader(shader.get());
        context->drawRect(destRect, paint);
        context->canvas()->restoreToCount(initialSaveCount);
    }

    if (isLazyDecoded)
        PlatformInstrumentation::didDrawLazyPixelRef(bitmap().getGenerationID());
}

} // namespace blink
