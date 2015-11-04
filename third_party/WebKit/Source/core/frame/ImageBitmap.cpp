// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/ImageBitmap.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/RefPtr.h"

namespace blink {

static inline IntRect normalizeRect(const IntRect& rect)
{
    return IntRect(std::min(rect.x(), rect.maxX()),
        std::min(rect.y(), rect.maxY()),
        std::max(rect.width(), -rect.width()),
        std::max(rect.height(), -rect.height()));
}

ImageBitmap::ImageBitmap(HTMLImageElement* image, const IntRect& cropRect)
{
    m_image = cropImage(image->cachedImage()->image()->imageForCurrentFrame(), cropRect);
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video, const IntRect& cropRect)
{
    IntSize playerSize;
    if (video->webMediaPlayer())
        playerSize = video->webMediaPlayer()->naturalSize();

    IntRect videoRect = IntRect(IntPoint(), playerSize);
    IntRect srcRect = intersection(cropRect, videoRect);
    OwnPtr<ImageBuffer> buffer = ImageBuffer::create(cropRect.size(), NonOpaque, DoNotInitializeImagePixels);
    if (!buffer)
        return;

    IntPoint dstPoint = IntPoint(std::max(0, -cropRect.x()), std::max(0, -cropRect.y()));
    video->paintCurrentFrame(buffer->canvas(), IntRect(dstPoint, srcRect.size()), nullptr);
    m_image = buffer->newSkImageSnapshot(PreferNoAcceleration);
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas, const IntRect& cropRect)
{
    ASSERT(canvas->isPaintable());
    m_image = cropImage(canvas->copiedImage(BackBuffer, PreferAcceleration)->imageForCurrentFrame(), cropRect);
}

ImageBitmap::ImageBitmap(ImageData* data, const IntRect& cropRect)
{
    IntRect srcRect = intersection(cropRect, IntRect(IntPoint(), data->size()));

    OwnPtr<ImageBuffer> buffer = ImageBuffer::create(cropRect.size(), NonOpaque, DoNotInitializeImagePixels);
    if (!buffer)
        return;

    if (srcRect.isEmpty()) {
        m_image = buffer->newSkImageSnapshot(PreferNoAcceleration);
        return;
    }

    IntPoint dstPoint = IntPoint(std::min(0, -cropRect.x()), std::min(0, -cropRect.y()));
    if (cropRect.x() < 0)
        dstPoint.setX(-cropRect.x());
    if (cropRect.y() < 0)
        dstPoint.setY(-cropRect.y());
    buffer->putByteArray(Unmultiplied, data->data()->data(), data->size(), srcRect, dstPoint);
    m_image = buffer->newSkImageSnapshot(PreferNoAcceleration);
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap, const IntRect& cropRect)
{
    m_image = cropImage(bitmap->skImage(), cropRect);
}

ImageBitmap::ImageBitmap(Image* image, const IntRect& cropRect)
{
    m_image = cropImage(image->imageForCurrentFrame(), cropRect);
}

ImageBitmap::~ImageBitmap()
{
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(HTMLImageElement* image, const IntRect& cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(image, normalizedCropRect));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(HTMLVideoElement* video, const IntRect& cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(video, normalizedCropRect));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(HTMLCanvasElement* canvas, const IntRect& cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(canvas, normalizedCropRect));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(ImageData* data, const IntRect& cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(data, normalizedCropRect));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(ImageBitmap* bitmap, const IntRect& cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(bitmap, normalizedCropRect));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(Image* image, const IntRect& cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(image, normalizedCropRect));
}

PassRefPtr<SkImage> ImageBitmap::cropImage(PassRefPtr<SkImage> image, const IntRect& cropRect)
{
    ASSERT(image);

    IntRect imgRect = IntRect(IntPoint(), IntSize(image->width(), image->height()));
    const IntRect srcRect = intersection(imgRect, cropRect);

    if (cropRect == srcRect)
        return adoptRef(image->newSubset(srcRect));

    RefPtr<SkSurface> surface = adoptRef(SkSurface::NewRasterN32Premul(cropRect.width(), cropRect.height()));

    if (srcRect.isEmpty())
        return adoptRef(surface->newImageSnapshot());

    SkScalar dstLeft = std::min(0, -cropRect.x());
    SkScalar dstTop = std::min(0, -cropRect.y());
    if (cropRect.x() < 0)
        dstLeft = -cropRect.x();
    if (cropRect.y() < 0)
        dstTop = -cropRect.y();
    surface->getCanvas()->drawImage(image.get(), dstLeft, dstTop);
    return adoptRef(surface->newImageSnapshot());
}

void ImageBitmap::notifyImageSourceChanged()
{
}

PassRefPtr<Image> ImageBitmap::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint) const
{
    *status = NormalSourceImageStatus;
    return m_image ? StaticBitmapImage::create(m_image) : nullptr;
}

void ImageBitmap::adjustDrawRects(FloatRect* srcRect, FloatRect* dstRect) const
{

}

FloatSize ImageBitmap::elementSize() const
{
    return FloatSize(width(), height());
}

DEFINE_TRACE(ImageBitmap)
{
    ImageLoaderClient::trace(visitor);
}

} // namespace blink
