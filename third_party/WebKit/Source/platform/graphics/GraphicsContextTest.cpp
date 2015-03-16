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
#include "platform/graphics/ImageBuffer.h"
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
