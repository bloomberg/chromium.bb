// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(sk_sp<SkImage> image)
{
    if (!image)
        return nullptr;
    if (image->isTextureBacked())
        return AcceleratedStaticBitmapImage::create(std::move(image));
    return adoptRef(new StaticBitmapImage(std::move(image)));
}

StaticBitmapImage::StaticBitmapImage(sk_sp<SkImage> image) : m_image(std::move(image))
{
    ASSERT(m_image);
}

IntSize StaticBitmapImage::size() const
{
    return IntSize(m_image->width(), m_image->height());
}

bool StaticBitmapImage::isTextureBacked()
{
    return m_image && m_image->isTextureBacked();
}

bool StaticBitmapImage::currentFrameKnownToBeOpaque(MetadataMode)
{
    return m_image->isOpaque();
}

void StaticBitmapImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect,
    const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode clampMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.intersect(SkRect::Make(m_image->bounds()));

    if (dstRect.isEmpty() || adjustedSrcRect.isEmpty())
        return; // Nothing to draw.

    canvas->drawImageRect(m_image.get(), adjustedSrcRect, dstRect, &paint,
        WebCoreClampingModeToSkiaRectConstraint(clampMode));

    if (ImageObserver* observer = getImageObserver())
        observer->didDraw(this);
}

sk_sp<SkImage> StaticBitmapImage::imageForCurrentFrame()
{
    return m_image;
}

} // namespace blink
