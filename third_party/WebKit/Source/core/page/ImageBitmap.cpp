// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/ImageBitmap.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "wtf/RefPtr.h"

using namespace std;

namespace WebCore {

static inline IntRect normalizeRect(const IntRect rect)
{
    return IntRect(min(rect.x(), rect.maxX()),
        min(rect.y(), rect.maxY()),
        max(rect.width(), -rect.width()),
        max(rect.height(), -rect.height()));
}

static inline PassRefPtr<BitmapImage> cropImage(Image* image, IntRect cropRect)
{
    SkBitmap cropped;
    image->nativeImageForCurrentFrame()->bitmap().extractSubset(&cropped, cropRect);
    return BitmapImage::create(NativeImageSkia::create(cropped));
}

ImageBitmap::ImageBitmap(HTMLImageElement* image, IntRect cropRect)
    : m_bitmapOffset(max(0, -cropRect.x()), max(0, -cropRect.y()))
    , m_size(cropRect.size())
{
    Image* bitmapImage = image->cachedImage()->image();
    m_bitmap = cropImage(bitmapImage, cropRect).get();

    ScriptWrappable::init(this);
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video, IntRect cropRect)
    : m_bitmapOffset(max(0, -cropRect.x()), max(0, -cropRect.y()))
    , m_size(cropRect.size())
{
    IntRect videoRect = IntRect(IntPoint(), video->player()->naturalSize());
    IntRect srcRect = intersection(cropRect, videoRect);
    IntRect dstRect(IntPoint(), srcRect.size());

    m_buffer = ImageBuffer::create(videoRect.size());
    GraphicsContext* c = m_buffer->context();
    c->clip(dstRect);
    c->translate(-srcRect.x(), -srcRect.y());
    video->paintCurrentFrameInContext(c, videoRect);
    m_bitmap = static_cast<BitmapImage*>(m_buffer->copyImage(DontCopyBackingStore).get());

    ScriptWrappable::init(this);
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas, IntRect cropRect)
    : m_bitmapOffset(max(0, -cropRect.x()), max(0, -cropRect.y()))
    , m_size(cropRect.size())
{
    IntSize canvasSize = canvas->size();
    IntRect srcRect = intersection(cropRect, IntRect(IntPoint(), canvasSize));
    IntRect dstRect(IntPoint(), srcRect.size());

    m_buffer = ImageBuffer::create(canvasSize);
    m_buffer->context()->drawImageBuffer(canvas->buffer(), dstRect, srcRect);
    m_bitmap = static_cast<BitmapImage*>(m_buffer->copyImage(DontCopyBackingStore).get());

    ScriptWrappable::init(this);
}

ImageBitmap::ImageBitmap(ImageData* data, IntRect cropRect)
    : m_bitmapOffset(max(0, -cropRect.x()), max(0, -cropRect.y()))
    , m_size(cropRect.size())
{
    IntRect srcRect = intersection(cropRect, IntRect(IntPoint(), data->size()));

    m_buffer = ImageBuffer::create(data->size());
    if (srcRect.width() > 0 && srcRect.height() > 0)
        m_buffer->putByteArray(Unmultiplied, data->data(), data->size(), srcRect, IntPoint(min(0, -cropRect.x()), min(0, -cropRect.y())));

    m_bitmap = static_cast<BitmapImage*>(m_buffer->copyImage(DontCopyBackingStore).get());

    ScriptWrappable::init(this);
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap, IntRect cropRect)
    : m_bitmapOffset(max(0, bitmap->bitmapOffset().x() - cropRect.x()), max(0, bitmap->bitmapOffset().y() - cropRect.y()))
    , m_size(cropRect.size())
{
    Image* bitmapImage = bitmap->bitmapImage();
    cropRect.moveBy(IntPoint(-bitmap->bitmapOffset().x(), -bitmap->bitmapOffset().y()));
    m_bitmap = cropImage(bitmapImage, cropRect).get();

    ScriptWrappable::init(this);
}

PassRefPtr<ImageBitmap> ImageBitmap::create(HTMLImageElement* image, IntRect cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    RefPtr<ImageBitmap> imageBitmap(adoptRef(new ImageBitmap(image, normalizedCropRect)));
    return imageBitmap.release();
}

PassRefPtr<ImageBitmap> ImageBitmap::create(HTMLVideoElement* video, IntRect cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    RefPtr<ImageBitmap> imageBitmap(adoptRef(new ImageBitmap(video, normalizedCropRect)));
    return imageBitmap.release();
}

PassRefPtr<ImageBitmap> ImageBitmap::create(HTMLCanvasElement* canvas, IntRect cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    RefPtr<ImageBitmap> imageBitmap(adoptRef(new ImageBitmap(canvas, normalizedCropRect)));
    return imageBitmap.release();
}

PassRefPtr<ImageBitmap> ImageBitmap::create(ImageData* data, IntRect cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    RefPtr<ImageBitmap> imageBitmap(adoptRef(new ImageBitmap(data, normalizedCropRect)));
    return imageBitmap.release();
}

PassRefPtr<ImageBitmap> ImageBitmap::create(ImageBitmap* bitmap, IntRect cropRect)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    RefPtr<ImageBitmap> imageBitmap(adoptRef(new ImageBitmap(bitmap, normalizedCropRect)));
    return imageBitmap.release();
}

}
