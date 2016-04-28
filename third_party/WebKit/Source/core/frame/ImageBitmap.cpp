// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ImageBitmap.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/RefPtr.h"

namespace blink {

static const char* imageOrientationFlipY = "flipY";
static const char* imageBitmapOptionNone = "none";

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

static void swizzleImageData(unsigned char* srcAddr, int height, int bytesPerRow, bool flipY)
{
    if (flipY) {
        for (int i = 0; i < height / 2; i++) {
            int topRowStartPosition = i * bytesPerRow;
            int bottomRowStartPosition = (height - 1 - i) * bytesPerRow;
            for (int j = 0; j < bytesPerRow; j += 4) {
                std::swap(srcAddr[topRowStartPosition + j], srcAddr[bottomRowStartPosition + j + 2]);
                std::swap(srcAddr[topRowStartPosition + j + 1], srcAddr[bottomRowStartPosition + j + 1]);
                std::swap(srcAddr[topRowStartPosition + j + 2], srcAddr[bottomRowStartPosition + j]);
                std::swap(srcAddr[topRowStartPosition + j + 3], srcAddr[bottomRowStartPosition + j + 3]);
            }
        }
    } else {
        for (int i = 0; i < height * bytesPerRow; i += 4)
            std::swap(srcAddr[i], srcAddr[i + 2]);
    }
}

static PassRefPtr<SkImage> flipSkImageVertically(SkImage* input, AlphaDisposition alphaOp)
{
    int width = input->width();
    int height = input->height();
    SkImageInfo info = SkImageInfo::MakeN32(width, height, (alphaOp == PremultiplyAlpha) ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
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

PassRefPtr<SkImage> ImageBitmap::getSkImageFromDecoder(PassOwnPtr<ImageDecoder> decoder)
{
    if (!decoder->frameCount())
        return nullptr;
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    if (!frame || frame->getStatus() != ImageFrame::FrameComplete)
        return nullptr;
    SkBitmap bitmap = frame->bitmap();
    if (!frameIsValid(bitmap))
        return nullptr;
    return adoptRef(SkImage::NewFromBitmap(bitmap));
}

// The parameter imageFormat indicates whether the first parameter "image" is unpremultiplied or not.
// For example, if the image is already in unpremultiplied format and we want the created ImageBitmap
// in the same format, then we don't need to use the ImageDecoder to decode the image.
static PassRefPtr<StaticBitmapImage> cropImage(Image* image, const IntRect& cropRect, bool flipY, bool premultiplyAlpha, AlphaDisposition imageFormat = DontPremultiplyAlpha, ImageDecoder::GammaAndColorProfileOption colorspaceOp = ImageDecoder::GammaAndColorProfileApplied)
{
    ASSERT(image);

    IntRect imgRect(IntPoint(), IntSize(image->width(), image->height()));
    const IntRect srcRect = intersection(imgRect, cropRect);

    // In the case when cropRect doesn't intersect the source image and it requires a umpremul image
    // We immediately return a transparent black image with cropRect.size()
    if (srcRect.isEmpty() && !premultiplyAlpha) {
        SkImageInfo info = SkImageInfo::Make(cropRect.width(), cropRect.height(), kN32_SkColorType, kUnpremul_SkAlphaType);
        OwnPtr<uint8_t[]> dstPixels = adoptArrayPtr(new uint8_t[cropRect.width() * cropRect.height() * info.bytesPerPixel()]());
        return StaticBitmapImage::create(newSkImageFromRaster(info, dstPixels.release(), cropRect.width() * info.bytesPerPixel()));
    }

    RefPtr<SkImage> skiaImage = image->imageForCurrentFrame();
    // Attempt to get raw unpremultiplied image data, executed only when skiaImage is premultiplied.
    if ((((!premultiplyAlpha && !skiaImage->isOpaque()) || !skiaImage) && image->data() && imageFormat == DontPremultiplyAlpha) || colorspaceOp == ImageDecoder::GammaAndColorProfileIgnored) {
        OwnPtr<ImageDecoder> decoder(ImageDecoder::create(*(image->data()),
            premultiplyAlpha ? ImageDecoder::AlphaPremultiplied : ImageDecoder::AlphaNotPremultiplied,
            colorspaceOp));
        if (!decoder)
            return nullptr;
        decoder->setData(image->data(), true);
        skiaImage = ImageBitmap::getSkImageFromDecoder(decoder.release());
        if (!skiaImage)
            return nullptr;
    }

    if (cropRect == srcRect) {
        if (flipY)
            return StaticBitmapImage::create(flipSkImageVertically(skiaImage->newSubset(srcRect), premultiplyAlpha ? PremultiplyAlpha : DontPremultiplyAlpha));
        SkImage* croppedSkImage = skiaImage->newSubset(srcRect);
        // Call preroll to trigger image decoding.
        croppedSkImage->preroll();
        return StaticBitmapImage::create(adoptRef(croppedSkImage));
    }

    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(cropRect.width(), cropRect.height());
    if (!surface)
        return nullptr;
    if (srcRect.isEmpty())
        return StaticBitmapImage::create(adoptRef(surface->newImageSnapshot()));

    SkScalar dstLeft = std::min(0, -cropRect.x());
    SkScalar dstTop = std::min(0, -cropRect.y());
    if (cropRect.x() < 0)
        dstLeft = -cropRect.x();
    if (cropRect.y() < 0)
        dstTop = -cropRect.y();
    surface->getCanvas()->drawImage(skiaImage.get(), dstLeft, dstTop);
    if (flipY)
        skiaImage = flipSkImageVertically(surface->newImageSnapshot(), PremultiplyAlpha);
    else
        skiaImage = adoptRef(surface->newImageSnapshot());
    if (premultiplyAlpha)
        return StaticBitmapImage::create(skiaImage.release());
    return StaticBitmapImage::create(premulSkImageToUnPremul(skiaImage.get()));
}

ImageBitmap::ImageBitmap(HTMLImageElement* image, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
{
    bool flipY;
    bool premultiplyAlpha;
    parseOptions(options, flipY, premultiplyAlpha);

    if (options.colorspaceConversion() == "none")
        m_image = cropImage(image->cachedImage()->getImage(), cropRect, flipY, premultiplyAlpha, DontPremultiplyAlpha, ImageDecoder::GammaAndColorProfileIgnored);
    else
        m_image = cropImage(image->cachedImage()->getImage(), cropRect, flipY, premultiplyAlpha, DontPremultiplyAlpha, ImageDecoder::GammaAndColorProfileApplied);
    if (!m_image)
        return;
    m_image->setOriginClean(!image->wouldTaintOrigin(document->getSecurityOrigin()));
    m_image->setPremultiplied(premultiplyAlpha);
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

    bool flipY;
    bool premultiplyAlpha;
    parseOptions(options, flipY, premultiplyAlpha);

    if (flipY || !premultiplyAlpha) {
        RefPtr<SkImage> skiaImage = buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
        if (flipY)
            skiaImage = flipSkImageVertically(skiaImage.get(), PremultiplyAlpha);
        if (!premultiplyAlpha)
            skiaImage = premulSkImageToUnPremul(skiaImage.get());
        m_image = StaticBitmapImage::create(skiaImage.release());
    } else {
        m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown));
    }
    m_image->setOriginClean(!video->wouldTaintOrigin(document->getSecurityOrigin()));
    m_image->setPremultiplied(premultiplyAlpha);
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    ASSERT(canvas->isPaintable());
    bool flipY;
    bool premultiplyAlpha;
    parseOptions(options, flipY, premultiplyAlpha);

    // canvas is always premultiplied, so set the last parameter to true and convert to un-premul later
    m_image = cropImage(canvas->copiedImage(BackBuffer, PreferAcceleration).get(), cropRect, flipY, true);
    if (!m_image)
        return;
    if (!premultiplyAlpha)
        m_image = StaticBitmapImage::create(premulSkImageToUnPremul(m_image->imageForCurrentFrame().get()));
    m_image->setOriginClean(canvas->originClean());
    m_image->setPremultiplied(premultiplyAlpha);
}

ImageBitmap::ImageBitmap(ImageData* data, const IntRect& cropRect, const ImageBitmapOptions& options, const bool& isImageDataPremultiplied)
{
    bool flipY;
    bool premultiplyAlpha;
    parseOptions(options, flipY, premultiplyAlpha);
    IntRect srcRect = intersection(cropRect, IntRect(IntPoint(), data->size()));

    // treat non-premultiplyAlpha as a special case
    if (!premultiplyAlpha) {
        unsigned char* srcAddr = data->data()->data();
        int srcHeight = data->size().height();
        int dstHeight = cropRect.height();
        // TODO (xidachen): skia doesn't support SkImage::NewRasterCopy from a kRGBA color type.
        // For now, we swap R and B channel and uses kBGRA color type.
        SkImageInfo info;
        if (!isImageDataPremultiplied)
            info = SkImageInfo::Make(cropRect.width(), dstHeight, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
        else
            info = SkImageInfo::Make(cropRect.width(), dstHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        int srcPixelBytesPerRow = info.bytesPerPixel() * data->size().width();
        int dstPixelBytesPerRow = info.bytesPerPixel() * cropRect.width();
        if (cropRect == IntRect(IntPoint(), data->size())) {
            swizzleImageData(srcAddr, srcHeight, srcPixelBytesPerRow, flipY);
            m_image = StaticBitmapImage::create(adoptRef(SkImage::NewRasterCopy(info, srcAddr, dstPixelBytesPerRow)));
            // restore the original ImageData
            swizzleImageData(srcAddr, srcHeight, srcPixelBytesPerRow, flipY);
        } else {
            OwnPtr<uint8_t[]> copiedDataBuffer = adoptArrayPtr(new uint8_t[dstHeight * dstPixelBytesPerRow]());
            if (!srcRect.isEmpty()) {
                IntPoint srcPoint = IntPoint((cropRect.x() > 0) ? cropRect.x() : 0, (cropRect.y() > 0) ? cropRect.y() : 0);
                IntPoint dstPoint = IntPoint((cropRect.x() >= 0) ? 0 : -cropRect.x(), (cropRect.y() >= 0) ? 0 : -cropRect.y());
                int copyHeight = srcHeight - srcPoint.y();
                if (cropRect.height() < copyHeight)
                    copyHeight = cropRect.height();
                int copyWidth = data->size().width() - srcPoint.x();
                if (cropRect.width() < copyWidth)
                    copyWidth = cropRect.width();
                for (int i = 0; i < copyHeight; i++) {
                    int srcStartCopyPosition = (i + srcPoint.y()) * srcPixelBytesPerRow + srcPoint.x() * info.bytesPerPixel();
                    int srcEndCopyPosition = srcStartCopyPosition + copyWidth * info.bytesPerPixel();
                    int dstStartCopyPosition;
                    if (flipY)
                        dstStartCopyPosition = (dstHeight -1 - dstPoint.y() - i) * dstPixelBytesPerRow + dstPoint.x() * info.bytesPerPixel();
                    else
                        dstStartCopyPosition = (dstPoint.y() + i) * dstPixelBytesPerRow + dstPoint.x() * info.bytesPerPixel();
                    for (int j = 0; j < srcEndCopyPosition - srcStartCopyPosition; j++) {
                        if (j % 4 == 0)
                            copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j + 2];
                        else if (j % 4 == 2)
                            copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j - 2];
                        else
                            copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j];
                    }
                }
            }
            m_image = StaticBitmapImage::create(newSkImageFromRaster(info, copiedDataBuffer.release(), dstPixelBytesPerRow));
        }
        m_image->setPremultiplied(premultiplyAlpha);
        return;
    }

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
    if (flipY)
        m_image = StaticBitmapImage::create(flipSkImageVertically(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown).get(), PremultiplyAlpha));
    else
        m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown));
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    bool flipY;
    bool premultiplyAlpha;
    parseOptions(options, flipY, premultiplyAlpha);
    m_image = cropImage(bitmap->bitmapImage(), cropRect, flipY, premultiplyAlpha, bitmap->isPremultiplied() ? DontPremultiplyAlpha : PremultiplyAlpha);
    if (!m_image)
        return;
    m_image->setOriginClean(bitmap->originClean());
    m_image->setPremultiplied(premultiplyAlpha);
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    bool flipY;
    bool premultiplyAlpha;
    parseOptions(options, flipY, premultiplyAlpha);
    m_image = cropImage(image.get(), cropRect, flipY, premultiplyAlpha, PremultiplyAlpha);
    if (!m_image)
        return;
    m_image->setOriginClean(image->originClean());
    m_image->setPremultiplied(premultiplyAlpha);
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

ImageBitmap* ImageBitmap::create(HTMLImageElement* image, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return new ImageBitmap(image, normalizedCropRect, document, options);
}

ImageBitmap* ImageBitmap::create(HTMLVideoElement* video, const IntRect& cropRect, Document* document, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return new ImageBitmap(video, normalizedCropRect, document, options);
}

ImageBitmap* ImageBitmap::create(HTMLCanvasElement* canvas, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return new ImageBitmap(canvas, normalizedCropRect, options);
}

ImageBitmap* ImageBitmap::create(ImageData* data, const IntRect& cropRect, const ImageBitmapOptions& options, const bool& isImageDataPremultiplied)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return new ImageBitmap(data, normalizedCropRect, options, isImageDataPremultiplied);
}

ImageBitmap* ImageBitmap::create(ImageBitmap* bitmap, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return new ImageBitmap(bitmap, normalizedCropRect, options);
}

ImageBitmap* ImageBitmap::create(PassRefPtr<StaticBitmapImage> image, const IntRect& cropRect, const ImageBitmapOptions& options)
{
    IntRect normalizedCropRect = normalizeRect(cropRect);
    return new ImageBitmap(image, normalizedCropRect, options);
}

ImageBitmap* ImageBitmap::create(PassRefPtr<StaticBitmapImage> image)
{
    return new ImageBitmap(image);
}

void ImageBitmap::close()
{
    if (!m_image || m_isNeutered)
        return;
    m_image.clear();
    m_isNeutered = true;
}

// static
ImageBitmap* ImageBitmap::take(ScriptPromiseResolver*, sk_sp<SkImage> image)
{
    return ImageBitmap::create(StaticBitmapImage::create(fromSkSp(image)));
}

PassOwnPtr<uint8_t[]> ImageBitmap::copyBitmapData(AlphaDisposition alphaOp)
{
    SkImageInfo info = SkImageInfo::Make(width(), height(), kRGBA_8888_SkColorType, (alphaOp == PremultiplyAlpha) ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
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

void ImageBitmap::parseOptions(const ImageBitmapOptions& options, bool& flipY, bool& premultiplyAlpha)
{
    if (options.imageOrientation() == imageOrientationFlipY) {
        flipY = true;
    } else {
        flipY = false;
        ASSERT(options.imageOrientation() == imageBitmapOptionNone);
    }
    if (options.premultiplyAlpha() == imageBitmapOptionNone) {
        premultiplyAlpha = false;
    } else {
        premultiplyAlpha = true;
        ASSERT(options.premultiplyAlpha() == "default" || options.premultiplyAlpha() == "premultiply");
    }
}

PassRefPtr<Image> ImageBitmap::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint, SnapshotReason, const FloatSize&) const
{
    *status = NormalSourceImageStatus;
    return m_image ? m_image : nullptr;
}

void ImageBitmap::adjustDrawRects(FloatRect* srcRect, FloatRect* dstRect) const
{
}

FloatSize ImageBitmap::elementSize(const FloatSize&) const
{
    return FloatSize(width(), height());
}

DEFINE_TRACE(ImageBitmap)
{
}

} // namespace blink
