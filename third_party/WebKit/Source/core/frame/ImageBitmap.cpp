// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ImageBitmap.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/RefPtr.h"

namespace blink {

static const char* imageOrientationFlipY = "flipY";
static const char* imageBitmapOptionNone = "none";

static void parseOptions(const ImageBitmapOptions& options, bool& imageOrientationFlipYFlag, bool& premultiplyAlphaEnabledFlag)
{
    if (options.imageOrientation() == imageOrientationFlipY) {
        imageOrientationFlipYFlag = true;
    } else {
        imageOrientationFlipYFlag = false;
        ASSERT(options.imageOrientation() == imageBitmapOptionNone);
    }
    if (options.premultiplyAlpha() == imageBitmapOptionNone) {
        premultiplyAlphaEnabledFlag = false;
    } else {
        premultiplyAlphaEnabledFlag = true;
        ASSERT(options.premultiplyAlpha() == "default");
    }
}

// The following two functions are helpers used in cropImage
static inline IntRect normalizeRect(const IntRect& rect)
{
    return IntRect(std::min(rect.x(), rect.maxX()),
        std::min(rect.y(), rect.maxY()),
        std::max(rect.width(), -rect.width()),
        std::max(rect.height(), -rect.height()));
}

static bool frameIsValid(const SkBitmap& frameBitmap)
{
    ASSERT(!frameBitmap.isNull() && !frameBitmap.empty() && frameBitmap.isImmutable());
    return frameBitmap.colorType() == kN32_SkColorType;
}

static PassOwnPtr<uint8_t[]> copySkImageData(SkImage* input, SkImageInfo info)
{
    OwnPtr<uint8_t[]> dstPixels = adoptArrayPtr(new uint8_t[input->width() * input->height() * info.bytesPerPixel()]);
    input->readPixels(info, dstPixels.get(), input->width() * info.bytesPerPixel(), 0, 0);
    return dstPixels.release();
}

static PassRefPtr<SkImage> newSkImageFromRaster(SkImageInfo info, PassOwnPtr<uint8_t[]> imagePixels, int imageRowBytes)
{
    return adoptRef(SkImage::NewFromRaster(info, imagePixels.leakPtr(), imageRowBytes,
        [](const void* pixels, void*)
        {
            delete[] static_cast<const uint8_t*>(pixels);
        }, nullptr));
}

static PassRefPtr<SkImage> flipSkImageVertically(SkImage* input)
{
    int width = input->width();
    int height = input->height();
    SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    int imageRowBytes = width * info.bytesPerPixel();
    OwnPtr<uint8_t[]> imagePixels = copySkImageData(input, info);
    for (int i = 0; i < height / 2; i++) {
        int topFirstElement = i * imageRowBytes;
        int topLastElement = (i + 1) * imageRowBytes;
        int bottomFirstElement = (height - 1 - i) * imageRowBytes;
        std::swap_ranges(imagePixels.get() + topFirstElement, imagePixels.get() + topLastElement, imagePixels.get() + bottomFirstElement);
    }
    return newSkImageFromRaster(info, imagePixels.release(), imageRowBytes);
}

static PassRefPtr<SkImage> premulSkImageToUnPremul(SkImage* input)
{
    SkImageInfo info = SkImageInfo::Make(input->width(), input->height(), kN32_SkColorType, kUnpremul_SkAlphaType);
    OwnPtr<uint8_t[]> dstPixels = copySkImageData(input, info);
    return newSkImageFromRaster(info, dstPixels.release(), input->width() * info.bytesPerPixel());
}

static PassRefPtr<StaticBitmapImage> cropImage(Image* image, const IntRect& cropRect, bool flipYEnabled, bool premultiplyAlphaEnabled)
{
    ASSERT(image);

    IntRect imgRect(IntPoint(), IntSize(image->width(), image->height()));
    const IntRect srcRect = intersection(imgRect, cropRect);

    RefPtr<SkImage> skiaImage = image->imageForCurrentFrame();
    // Attempt to get raw unpremultiplied image data.
    if (((!premultiplyAlphaEnabled && !skiaImage->isOpaque()) || !skiaImage) && image->data()) {
        // TODO(xidachen): GammaAndColorProfileApplied needs to be changed when working on color-space conversion
        OwnPtr<ImageDecoder> decoder(ImageDecoder::create(
            *(image->data()), ImageDecoder::AlphaNotPremultiplied,
            ImageDecoder::GammaAndColorProfileApplied));
        if (!decoder)
            return nullptr;
        decoder->setData(image->data(), true);
        if (!decoder->frameCount())
            return nullptr;
        ImageFrame* frame = decoder->frameBufferAtIndex(0);
        if (!frame || frame->status() != ImageFrame::FrameComplete)
            return nullptr;
        SkBitmap bitmap = frame->bitmap();
        if (!frameIsValid(bitmap))
            return nullptr;
        skiaImage = adoptRef(SkImage::NewFromBitmap(bitmap));
    }

    if (cropRect == srcRect) {
        if (flipYEnabled)
            return StaticBitmapImage::create(flipSkImageVertically(skiaImage->newSubset(srcRect)));
        return StaticBitmapImage::create(adoptRef(skiaImage->newSubset(srcRect)));
    }

    RefPtr<SkSurface> surface = adoptRef(SkSurface::NewRasterN32Premul(cropRect.width(), cropRect.height()));
    // In the case where cropRect doesn't intesect the source image, we return a premultiplied transparent black SkImage.
    // If we decide we want to grab meta data from m_image, we have to change this.
    if (srcRect.isEmpty())
        return StaticBitmapImage::create(adoptRef(surface->newImageSnapshot()));

    SkScalar dstLeft = std::min(0, -cropRect.x());
    SkScalar dstTop = std::min(0, -cropRect.y());
    if (cropRect.x() < 0)
        dstLeft = -cropRect.x();
    if (cropRect.y() < 0)
        dstTop = -cropRect.y();
    surface->getCanvas()->drawImage(skiaImage.get(), dstLeft, dstTop);
    if (flipYEnabled)
        skiaImage = flipSkImageVertically(surface->newImageSnapshot());
    else
        skiaImage = adoptRef(surface->newImageSnapshot());
    if (premultiplyAlphaEnabled)
        return StaticBitmapImage::create(skiaImage.release());
    return StaticBitmapImage::create(premulSkImageToUnPremul(skiaImage.get()));
}

ImageBitmap::ImageBitmap(HTMLImageElement* image, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
{
    bool imageOrientationFlipYFlag;
    bool premultiplyAlphaEnabledFlag;
    parseOptions(options, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);

    m_image = cropImage(image->cachedImage()->image(), cropRect, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);
    m_image->setOriginClean(!image->wouldTaintOrigin(document->securityOrigin()));
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
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

    bool imageOrientationFlipYFlag;
    bool premultiplyAlphaEnabledFlag;
    parseOptions(options, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);

    if (imageOrientationFlipYFlag || !premultiplyAlphaEnabledFlag) {
        RefPtr<SkImage> skiaImage = buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
        if (imageOrientationFlipYFlag)
            skiaImage = flipSkImageVertically(skiaImage.get());
        if (!premultiplyAlphaEnabledFlag)
            skiaImage = premulSkImageToUnPremul(skiaImage.get());
        m_image = StaticBitmapImage::create(skiaImage.release());
    } else {
        m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown));
    }
    m_image->setOriginClean(!video->wouldTaintOrigin(document->securityOrigin()));
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    ASSERT(canvas->isPaintable());
    bool imageOrientationFlipYFlag;
    bool premultiplyAlphaEnabledFlag;
    parseOptions(options, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);
    // canvas is always premultiplied, so set the last parameter to true and convert to un-premul later
    m_image = cropImage(canvas->copiedImage(BackBuffer, PreferAcceleration).get(), cropRect, imageOrientationFlipYFlag, true);
    if (!premultiplyAlphaEnabledFlag)
        m_image = StaticBitmapImage::create(premulSkImageToUnPremul(m_image->imageForCurrentFrame().get()));
    m_image->setOriginClean(canvas->originClean());
}

ImageBitmap::ImageBitmap(ImageData* data, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect srcRect = intersection(cropRect, IntRect(IntPoint(), data->size()));

    OwnPtr<ImageBuffer> buffer = ImageBuffer::create(cropRect.size(), NonOpaque, DoNotInitializeImagePixels);
    if (!buffer)
        return;

    if (srcRect.isEmpty()) {
        m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown));
        return;
    }

    IntPoint dstPoint = IntPoint(std::min(0, -cropRect.x()), std::min(0, -cropRect.y()));
    if (cropRect.x() < 0)
        dstPoint.setX(-cropRect.x());
    if (cropRect.y() < 0)
        dstPoint.setY(-cropRect.y());
    buffer->putByteArray(Unmultiplied, data->data()->data(), data->size(), srcRect, dstPoint);
    if (options.imageOrientation() == imageOrientationFlipY)
        m_image = StaticBitmapImage::create(flipSkImageVertically(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown).get()));
    else
        m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown));
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    bool imageOrientationFlipYFlag;
    bool premultiplyAlphaEnabledFlag;
    parseOptions(options, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);
    m_image = cropImage(bitmap->bitmapImage(), cropRect, imageOrientationFlipYFlag, true);
    m_image->setOriginClean(bitmap->originClean());
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    bool imageOrientationFlipYFlag;
    bool premultiplyAlphaEnabledFlag;
    parseOptions(options, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);
    m_image = cropImage(image.get(), cropRect, imageOrientationFlipYFlag, premultiplyAlphaEnabledFlag);
    m_image->setOriginClean(image->originClean());
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image)
{
    m_image = image;
}

PassRefPtr<StaticBitmapImage> ImageBitmap::transfer()
{
    ASSERT(!isNeutered());
    m_isNeutered = true;
    return m_image.release();
}

ImageBitmap::~ImageBitmap()
{
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(HTMLImageElement* image, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(image, normalizedCropRect, document, options));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(HTMLVideoElement* video, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(video, normalizedCropRect, document, options));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(HTMLCanvasElement* canvas, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(canvas, normalizedCropRect, options));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(ImageData* data, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(data, normalizedCropRect, options));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(ImageBitmap* bitmap, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(bitmap, normalizedCropRect, options));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(PassRefPtr<StaticBitmapImage> image, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return adoptRefWillBeNoop(new ImageBitmap(image, normalizedCropRect, options));
}

PassRefPtrWillBeRawPtr<ImageBitmap> ImageBitmap::create(PassRefPtr<StaticBitmapImage> image)
{
    return adoptRefWillBeNoop(new ImageBitmap(image));
}

void ImageBitmap::close()
{
    if (!m_image || m_isNeutered)
        return;
    m_image.clear();
    m_isNeutered = true;
}

PassOwnPtr<uint8_t[]> ImageBitmap::copyBitmapData()
{
    SkImageInfo info = SkImageInfo::Make(width(), height(), kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    OwnPtr<uint8_t[]> dstPixels = copySkImageData(m_image->imageForCurrentFrame().get(), info);
    return dstPixels.release();
}

unsigned long ImageBitmap::width() const
{
    if (!m_image)
        return 0;
    ASSERT(m_image->width() > 0);
    return m_image->width();
}

unsigned long ImageBitmap::height() const
{
    if (!m_image)
        return 0;
    ASSERT(m_image->height() > 0);
    return m_image->height();
}

IntSize ImageBitmap::size() const
{
    if (!m_image)
        return IntSize();
    ASSERT(m_image->width() > 0 && m_image->height() > 0);
    return IntSize(m_image->width(), m_image->height());
}

ScriptPromise ImageBitmap::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, int sx, int sy, int sw, int sh, const ImageBitmapOptions& options, ExceptionState& exceptionState)
{
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    return ImageBitmapSource::fulfillImageBitmap(scriptState, create(this, IntRect(sx, sy, sw, sh), options));
}

void ImageBitmap::notifyImageSourceChanged()
{
}

PassRefPtr<Image> ImageBitmap::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint, SnapshotReason) const
{
    *status = NormalSourceImageStatus;
    return m_image ? m_image : nullptr;
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
