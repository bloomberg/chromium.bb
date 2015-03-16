// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/StaticBitmapImage.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

PassRefPtr<Image> StaticBitmapImage::create(PassRefPtr<SkImage> image)
{
    return adoptRef(new StaticBitmapImage(image));
}

StaticBitmapImage::StaticBitmapImage(PassRefPtr<SkImage> image) : m_image(image)
{
    ASSERT(m_image);
}

StaticBitmapImage::~StaticBitmapImage() { }

IntSize StaticBitmapImage::size() const
{
    return IntSize(m_image->width(), m_image->height());
}

bool StaticBitmapImage::currentFrameKnownToBeOpaque()
{
    return m_image->isOpaque();
}

void StaticBitmapImage::draw(GraphicsContext* ctx, const FloatRect& dstRect, const FloatRect& srcRect, SkXfermode::Mode compositeOp, RespectImageOrientationEnum)
{
    FloatRect normDstRect = adjustForNegativeSize(dstRect);
    FloatRect normSrcRect = adjustForNegativeSize(srcRect);

    normSrcRect.intersect(FloatRect(0, 0, m_image->width(), m_image->height()));

    if (normSrcRect.isEmpty() || normDstRect.isEmpty())
        return; // Nothing to draw.

    ASSERT(normSrcRect.width() <= m_image->width() && normSrcRect.height() <= m_image->height());

    {
        SkCanvas* canvas = ctx->canvas();

        SkPaint paint;
        int initialSaveCount = ctx->preparePaintForDrawRectToRect(&paint, srcRect, dstRect, compositeOp, !currentFrameKnownToBeOpaque());

        SkRect srcSkRect = WebCoreFloatRectToSKRect(normSrcRect);
        SkRect dstSkRect = WebCoreFloatRectToSKRect(normDstRect);

        canvas->drawImageRect(m_image.get(), &srcSkRect, dstSkRect, &paint);
        canvas->restoreToCount(initialSaveCount);
    }

    if (ImageObserver* observer = imageObserver())
        observer->didDraw(this);
}

} // namespace blink
