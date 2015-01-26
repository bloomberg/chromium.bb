/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/GraphicsContext.h"

#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/GraphicsContextClient.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkBlurDrawLooper.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"

#include <gtest/gtest.h>

using namespace blink;

namespace {

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

#define EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = opaqueRect.y(); y < opaqueRect.maxY(); ++y) \
        for (int x = opaqueRect.x(); x < opaqueRect.maxX(); ++x) { \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            EXPECT_EQ(255, alpha); \
        } \
}

#define EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = 0; y < bitmap.height(); ++y) \
        for (int x = 0; x < bitmap.width(); ++x) {     \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            bool opaque = opaqueRect.contains(x, y); \
            EXPECT_EQ(opaque, alpha == 255); \
        } \
}

TEST(GraphicsContextTest, trackDisplayListRecording)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 100);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    GraphicsContext context(&canvas, nullptr);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);

    context.fillRect(FloatRect(0, 0, 50, 50), opaque, SkXfermode::kSrcOver_Mode);
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 50, 50))

    FloatRect bounds(0, 0, 100, 100);
    context.beginRecording(bounds);
    context.fillRect(FloatRect(0, 0, 100, 100), opaque, SkXfermode::kSrcOver_Mode);
    RefPtr<const SkPicture> picture = context.endRecording();

    // Make sure the opaque region was unaffected by the rect drawn during Picture recording.
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 50, 50))
}

TEST(GraphicsContextTest, trackImageMask)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(400, 400);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    GraphicsContext context(&canvas, nullptr);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    // Image masks are done by drawing a bitmap into a transparency layer that uses DstIn to mask
    // out a transparency layer below that is filled with the mask color. In the end this should
    // not be marked opaque.

    context.setCompositeOperation(SkXfermode::kSrcOver_Mode);
    context.beginTransparencyLayer(1);
    context.fillRect(FloatRect(10, 10, 10, 10), opaque, SkXfermode::kSrcOver_Mode);

    context.setCompositeOperation(SkXfermode::kDstIn_Mode);
    context.beginTransparencyLayer(1);

    OwnPtr<ImageBuffer> alphaImage = ImageBuffer::create(IntSize(100, 100));
    EXPECT_FALSE(!alphaImage);
    alphaImage->context()->fillRect(IntRect(0, 0, 100, 100), alpha);

    context.setCompositeOperation(SkXfermode::kSrcOver_Mode);
    context.drawImageBuffer(alphaImage.get(), FloatRect(10, 10, 10, 10));

    context.endLayer();
    context.endLayer();

    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect());
}

TEST(GraphicsContextTest, trackImageMaskWithOpaqueRect)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(400, 400);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    GraphicsContext context(&canvas, nullptr);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    // Image masks are done by drawing a bitmap into a transparency layer that uses DstIn to mask
    // out a transparency layer below that is filled with the mask color. In the end this should
    // not be marked opaque.

    context.setCompositeOperation(SkXfermode::kSrcOver_Mode);
    context.beginTransparencyLayer(1);
    context.fillRect(FloatRect(10, 10, 10, 10), opaque, SkXfermode::kSrcOver_Mode);

    context.setCompositeOperation(SkXfermode::kDstIn_Mode);
    context.beginTransparencyLayer(1);

    OwnPtr<ImageBuffer> alphaImage = ImageBuffer::create(IntSize(100, 100));
    EXPECT_FALSE(!alphaImage);
    alphaImage->context()->fillRect(IntRect(0, 0, 100, 100), alpha);

    context.setCompositeOperation(SkXfermode::kSrcOver_Mode);
    context.drawImageBuffer(alphaImage.get(), FloatRect(10, 10, 10, 10));

    // We can't have an opaque mask actually, but we can pretend here like it would look if we did.
    context.fillRect(FloatRect(12, 12, 3, 3), opaque, SkXfermode::kSrcOver_Mode);

    context.endLayer();
    context.endLayer();

    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(12, 12, 3, 3));
}

#define TEST_CLEAR_SETUP \
        SkBitmap bitmap; \
        bitmap.allocN32Pixels(10, 10); \
        bitmap.eraseColor(0); \
        SkCanvas canvas(bitmap); \
        TestGraphicsContextClient gcClient; \
        GraphicsContext context(&canvas, nullptr); \
        context.setClient(&gcClient);

#define TEST_CLEAR_1(SHOULD_CLEAR, CALL1) \
    do { \
        TEST_CLEAR_SETUP \
        context.CALL1; \
        EXPECT_TRUE((SHOULD_CLEAR == gcClient.didClear())); \
    } while (0)

#define TEST_CLEAR_2(SHOULD_CLEAR, CALL1, CALL2) \
    do { \
        TEST_CLEAR_SETUP \
        context.CALL1; \
        context.CALL2; \
        EXPECT_TRUE((SHOULD_CLEAR == gcClient.didClear())); \
    } while (0)

#define TEST_CLEAR_3(SHOULD_CLEAR, CALL1, CALL2, CALL3) \
    do { \
        TEST_CLEAR_SETUP \
        context.CALL1; \
        context.CALL2; \
        context.CALL3; \
        EXPECT_TRUE((SHOULD_CLEAR == gcClient.didClear())); \
    } while (0)

class TestGraphicsContextClient : public blink::GraphicsContextClient {
public:
    TestGraphicsContextClient() : m_didClear(false) { }
    bool didClear() { return m_didClear; }

protected:
    virtual void willOverwriteCanvas() { m_didClear = true; }

private:
    bool m_didClear;
};

enum BitmapOpacity {
    OpaqueBitmap,
    TransparentBitmap
};

static SkBitmap createTestBitmap(BitmapOpacity opacity)
{
    SkBitmap bitmap;
    SkColor color = opacity == OpaqueBitmap ? SK_ColorWHITE : SK_ColorTRANSPARENT;
    bitmap.allocN32Pixels(10, 10, opacity == OpaqueBitmap);
    for (int y = 0; y < bitmap.height(); ++y)
        for (int x = 0; x < bitmap.width(); ++x)
            *bitmap.getAddr32(x, y) = color;
    return bitmap;
}

TEST(GraphicsContextTest, detectClear)
{
    SkRect fullRect = SkRect::MakeWH(10, 10);
    SkPaint opaquePaint;
    SkPaint alphaPaint;
    alphaPaint.setAlpha(0x7F);
    SkRect partialRect = SkRect::MakeWH(1, 1);
    SkImageInfo opaqueImageInfo = SkImageInfo::MakeN32(10, 10, kOpaque_SkAlphaType);
    SkImageInfo alphaImageInfo = SkImageInfo::MakeN32(10, 10, kPremul_SkAlphaType);
    SkImageInfo smallImageInfo = SkImageInfo::MakeN32(1, 1, kOpaque_SkAlphaType);

    SkBitmap opaqueBitmap = createTestBitmap(OpaqueBitmap);
    SkBitmap alphaBitmap = createTestBitmap(TransparentBitmap);

    SkAutoLockPixels alp(opaqueBitmap);
    const void* imageData = opaqueBitmap.getAddr32(0, 0);
    size_t imageRowBytes = opaqueBitmap.rowBytes();

    // Test drawRect
    TEST_CLEAR_1(true, drawRect(fullRect, opaquePaint));
    TEST_CLEAR_1(false, drawRect(fullRect, alphaPaint));
    TEST_CLEAR_1(false, drawRect(partialRect, opaquePaint));
    TEST_CLEAR_2(false, translate(1, 1), drawRect(fullRect, opaquePaint));

    // Test drawBitmap
    TEST_CLEAR_1(true, drawBitmap(opaqueBitmap, 0, 0, 0));
    TEST_CLEAR_1(true, drawBitmap(opaqueBitmap, 0, 0, &opaquePaint));
    TEST_CLEAR_1(true, drawBitmap(opaqueBitmap, 0, 0, &alphaPaint));
    TEST_CLEAR_1(false, drawBitmap(alphaBitmap, 0, 0, &opaquePaint));
    TEST_CLEAR_1(false, drawBitmap(alphaBitmap, 0, 0, &alphaPaint));
    TEST_CLEAR_1(false, drawBitmap(opaqueBitmap, 1, 0, 0));
    TEST_CLEAR_2(true, translate(-1, 0), drawBitmap(opaqueBitmap, 1, 0, 0));
    TEST_CLEAR_1(true, drawBitmapRect(opaqueBitmap, 0, fullRect, 0));
    TEST_CLEAR_1(true, drawBitmapRect(opaqueBitmap, &partialRect, fullRect, 0));
    TEST_CLEAR_1(false, drawBitmapRect(opaqueBitmap, 0, partialRect, 0));
    TEST_CLEAR_2(true, scale(10, 10), drawBitmapRect(opaqueBitmap, 0, partialRect, 0));

    // Test writePixels
    TEST_CLEAR_1(true, writePixels(opaqueImageInfo, imageData, imageRowBytes, 0, 0));
    TEST_CLEAR_1(true, writePixels(alphaImageInfo, imageData, imageRowBytes, 0, 0)); // alpha has no effect
    TEST_CLEAR_1(false, writePixels(smallImageInfo, imageData, imageRowBytes, 0, 0));
    TEST_CLEAR_2(true, translate(1, 1), writePixels(opaqueImageInfo, imageData, imageRowBytes, 0, 0)); // ignores tranforms
    TEST_CLEAR_1(false, writePixels(opaqueImageInfo, imageData, imageRowBytes, 1, 0));

    // Test clipping
    TEST_CLEAR_2(false, clipRect(partialRect), drawRect(fullRect, opaquePaint));
}

TEST(GraphicsContextTest, detectClearWithOpaquePaint)
{
    // This test ensures that a draw that covers the canvas is detected as a clear
    // if its paint is opaque.

    SkRect fullRect = SkRect::MakeWH(10, 10);

    SkBitmap opaqueBitmap = createTestBitmap(OpaqueBitmap);
    SkBitmap alphaBitmap = createTestBitmap(TransparentBitmap);

    SkAutoTUnref<SkShader> opaqueShader(SkShader::CreateBitmapShader(opaqueBitmap, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode));

    {
        SkPaint paint;
        paint.setStyle(SkPaint::kFill_Style);
        TEST_CLEAR_1(true, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setStyle(SkPaint::kStrokeAndFill_Style);
        TEST_CLEAR_1(true, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setShader(opaqueShader);
        TEST_CLEAR_1(true, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setAlpha(0x7F); // shader overrides color
        paint.setShader(opaqueShader);
        TEST_CLEAR_1(true, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setShader(opaqueShader); // shader overrides the bitmap
        TEST_CLEAR_1(true, drawBitmap(alphaBitmap, 0, 0, &paint));
    }
    {
        SkPaint paint;
        paint.setAlpha(0); // bitmap overrides color
        TEST_CLEAR_1(true, drawBitmap(opaqueBitmap, 0, 0, &paint));
    }
    {
        SkPaint paint;
        paint.setAlpha(0x7F);
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        TEST_CLEAR_1(true, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setAlpha(0x7F);
        paint.setXfermodeMode(SkXfermode::kClear_Mode);
        TEST_CLEAR_1(true, drawRect(fullRect, paint));
    }
}

TEST(GraphicsContextTest, detectNoClearWithTransparentPaint)
{
    // This test ensures that a draw that covers the canvas is never detected as a clear
    // if its paint has any properties that make it non-opaque
    SkRect fullRect = SkRect::MakeWH(10, 10);

    SkBitmap opaqueBitmap = createTestBitmap(OpaqueBitmap);
    SkBitmap alphaBitmap = createTestBitmap(TransparentBitmap);

    SkAutoTUnref<SkShader> alphaShader(SkShader::CreateBitmapShader(alphaBitmap, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode));

    {
        SkPaint paint;
        paint.setStyle(SkPaint::kStroke_Style);
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setColor(SK_ColorTRANSPARENT);
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setShader(alphaShader);
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        paint.setShader(alphaShader); // shader overrides the bitmap.
        TEST_CLEAR_1(false, drawBitmap(opaqueBitmap, 0, 0, &paint));
    }
    for (int mode = SkXfermode::kDstOver_Mode; mode <= SkXfermode::kLastMode; mode++) {
        SkPaint paint;
        paint.setXfermodeMode(static_cast<SkXfermode::Mode>(mode));
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        SkAutoTUnref<SkMaskFilter> filter(SkBlurMaskFilter::Create(kNormal_SkBlurStyle, 1));
        paint.setMaskFilter(filter.get());
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        SkAutoTUnref<SkImageFilter> filter(SkBlurImageFilter::Create(1, 1));
        paint.setImageFilter(filter.get());
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        SkAutoTUnref<SkDrawLooper> looper(SkBlurDrawLooper::Create(SK_ColorWHITE, 1, 1, 1));
        paint.setLooper(looper.get());
        TEST_CLEAR_1(false, drawRect(fullRect, paint));
    }
    {
        SkPaint paint;
        TEST_CLEAR_3(false, beginLayer(1.0f, SkXfermode::kSrcOver_Mode), drawRect(fullRect, paint), endLayer());
    }
}

TEST(GraphicsContextTest, UnboundedDrawsAreClipped)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(400, 400);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    GraphicsContext context(&canvas, nullptr);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    Path path;
    context.setShouldAntialias(false);
    context.setMiterLimit(1);
    context.setStrokeThickness(5);
    context.setLineCap(SquareCap);
    context.setStrokeStyle(SolidStroke);

    // Make skia unable to compute fast bounds for our paths.
    DashArray dashArray;
    dashArray.append(1);
    dashArray.append(0);
    context.setLineDash(dashArray, 0);

    // Make the device opaque in 10,10 40x40.
    context.fillRect(FloatRect(10, 10, 40, 40), opaque, SkXfermode::kSrcOver_Mode);
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(10, 10, 40, 40));

    // Clip to the left edge of the opaque area.
    context.clip(IntRect(10, 10, 10, 40));

    // Draw a path that gets clipped. This should destroy the opaque area but only inside the clip.
    context.setCompositeOperation(SkXfermode::kSrcOut_Mode);
    context.setFillColor(alpha);
    path.moveTo(FloatPoint(10, 10));
    path.addLineTo(FloatPoint(40, 40));
    context.strokePath(path);

    EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, IntRect(20, 10, 30, 40));
}

#define DISPATCH1(c1, c2, op, param1) do { c1.op(param1); c2.op(param1); } while (0);
#define DISPATCH2(c1, c2, op, param1, param2) do { c1.op(param1, param2); c2.op(param1, param2); } while (0);

TEST(GraphicsContextTest, RecordingTotalMatrix)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(400, 400);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);
    GraphicsContext context(&canvas, nullptr);

    SkCanvas controlCanvas(400, 400);
    GraphicsContext controlContext(&controlCanvas, nullptr);

    EXPECT_EQ(context.getCTM(), controlContext.getCTM());
    DISPATCH2(context, controlContext, scale, 2, 2);
    EXPECT_EQ(context.getCTM(), controlContext.getCTM());

    controlContext.save();
    context.beginRecording(FloatRect(0, 0, 200, 200));
    DISPATCH2(context, controlContext, translate, 10, 10);
    EXPECT_EQ(context.getCTM(), controlContext.getCTM());

    controlContext.save();
    context.beginRecording(FloatRect(10, 10, 100, 100));
    DISPATCH1(context, controlContext, rotate, 45);
    EXPECT_EQ(context.getCTM(), controlContext.getCTM());

    controlContext.restore();
    context.endRecording();
    EXPECT_EQ(context.getCTM(), controlContext.getCTM());

    controlContext.restore();
    context.endRecording();
    EXPECT_EQ(context.getCTM(), controlContext.getCTM());
}

TEST(GraphicsContextTest, RecordingCanvas)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);
    GraphicsContext context(&canvas, nullptr);

    FloatRect rect(0, 0, 1, 1);

    // Two beginRecordings in a row generate two canvases.
    // Unfortunately the new one could be allocated in the same
    // spot as the old one so ref the first one to prolong its life.
    context.beginRecording(rect);
    SkCanvas* canvas1 = context.canvas();
    EXPECT_TRUE(canvas1);
    context.beginRecording(rect);
    SkCanvas* canvas2 = context.canvas();
    EXPECT_TRUE(canvas2);

    EXPECT_NE(canvas1, canvas2);
    EXPECT_TRUE(canvas1->unique());

    // endRecording finally makes the picture accessible
    RefPtr<const SkPicture> picture = context.endRecording();
    EXPECT_TRUE(picture);
    EXPECT_TRUE(picture->unique());

    context.endRecording();
}

} // namespace
