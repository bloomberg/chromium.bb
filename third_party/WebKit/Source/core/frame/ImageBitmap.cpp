// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ImageBitmap.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

static const char* imageOrientationFlipY = "flipY";
static const char* imageBitmapOptionNone = "none";

namespace {

struct ParsedOptions {
    bool flipY = false;
    bool premultiplyAlpha = true;
    bool shouldScaleInput = false;
    unsigned resizeWidth = 0;
    unsigned resizeHeight = 0;
    IntRect cropRect;
    SkFilterQuality resizeQuality = kLow_SkFilterQuality;
};

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

ParsedOptions parseOptions(const ImageBitmapOptions& options, Optional<IntRect> cropRect, IntSize sourceSize)
{
    ParsedOptions parsedOptions;
    if (options.imageOrientation() == imageOrientationFlipY) {
        parsedOptions.flipY = true;
    } else {
        parsedOptions.flipY = false;
        DCHECK(options.imageOrientation() == imageBitmapOptionNone);
    }
    if (options.premultiplyAlpha() == imageBitmapOptionNone) {
        parsedOptions.premultiplyAlpha = false;
    } else {
        parsedOptions.premultiplyAlpha = true;
        DCHECK(options.premultiplyAlpha() == "default" || options.premultiplyAlpha() == "premultiply");
    }

    int sourceWidth = sourceSize.width();
    int sourceHeight = sourceSize.height();
    if (!cropRect) {
        parsedOptions.cropRect = IntRect(0, 0, sourceWidth, sourceHeight);
    } else  {
        parsedOptions.cropRect = normalizeRect(*cropRect);
    }
    if (!options.hasResizeWidth() && !options.hasResizeHeight()) {
        parsedOptions.resizeWidth = parsedOptions.cropRect.width();
        parsedOptions.resizeHeight = parsedOptions.cropRect.height();
    } else if (options.hasResizeWidth() && options.hasResizeHeight()) {
        parsedOptions.resizeWidth = options.resizeWidth();
        parsedOptions.resizeHeight = options.resizeHeight();
    } else if (options.hasResizeWidth() && !options.hasResizeHeight()) {
        parsedOptions.resizeWidth = options.resizeWidth();
        parsedOptions.resizeHeight = ceil(static_cast<float>(options.resizeWidth()) / parsedOptions.cropRect.width() * parsedOptions.cropRect.height());
    } else {
        parsedOptions.resizeHeight = options.resizeHeight();
        parsedOptions.resizeWidth = ceil(static_cast<float>(options.resizeHeight()) / parsedOptions.cropRect.height() * parsedOptions.cropRect.width());
    }
    if (static_cast<int>(parsedOptions.resizeWidth) == parsedOptions.cropRect.width() && static_cast<int>(parsedOptions.resizeHeight) == parsedOptions.cropRect.height()) {
        parsedOptions.shouldScaleInput = false;
        return parsedOptions;
    }
    parsedOptions.shouldScaleInput = true;

    if (options.resizeQuality() == "high")
        parsedOptions.resizeQuality = kHigh_SkFilterQuality;
    else if (options.resizeQuality() == "medium")
        parsedOptions.resizeQuality = kMedium_SkFilterQuality;
    else if (options.resizeQuality() == "pixelated")
        parsedOptions.resizeQuality = kNone_SkFilterQuality;
    else
        parsedOptions.resizeQuality = kLow_SkFilterQuality;
    return parsedOptions;
}

} // namespace

static std::unique_ptr<uint8_t[]> copySkImageData(SkImage* input, const SkImageInfo& info)
{
    std::unique_ptr<uint8_t[]> dstPixels = wrapArrayUnique(new uint8_t[input->width() * input->height() * info.bytesPerPixel()]);
    input->readPixels(info, dstPixels.get(), input->width() * info.bytesPerPixel(), 0, 0);
    return dstPixels;
}

static PassRefPtr<SkImage> newSkImageFromRaster(const SkImageInfo& info, std::unique_ptr<uint8_t[]> imagePixels, int imageRowBytes)
{
    return fromSkSp(SkImage::MakeFromRaster(SkPixmap(info, imagePixels.release(), imageRowBytes),
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
    std::unique_ptr<uint8_t[]> imagePixels = copySkImageData(input, info);
    for (int i = 0; i < height / 2; i++) {
        int topFirstElement = i * imageRowBytes;
        int topLastElement = (i + 1) * imageRowBytes;
        int bottomFirstElement = (height - 1 - i) * imageRowBytes;
        std::swap_ranges(imagePixels.get() + topFirstElement, imagePixels.get() + topLastElement, imagePixels.get() + bottomFirstElement);
    }
    return newSkImageFromRaster(info, std::move(imagePixels), imageRowBytes);
}

static PassRefPtr<SkImage> premulSkImageToUnPremul(SkImage* input)
{
    SkImageInfo info = SkImageInfo::Make(input->width(), input->height(), kN32_SkColorType, kUnpremul_SkAlphaType);
    std::unique_ptr<uint8_t[]> dstPixels = copySkImageData(input, info);
    return newSkImageFromRaster(info, std::move(dstPixels), input->width() * info.bytesPerPixel());
}

static PassRefPtr<SkImage> unPremulSkImageToPremul(SkImage* input)
{
    SkImageInfo info = SkImageInfo::Make(input->width(), input->height(), kN32_SkColorType, kPremul_SkAlphaType);
    std::unique_ptr<uint8_t[]> dstPixels = copySkImageData(input, info);
    return newSkImageFromRaster(info, std::move(dstPixels), input->width() * info.bytesPerPixel());
}

PassRefPtr<SkImage> ImageBitmap::getSkImageFromDecoder(std::unique_ptr<ImageDecoder> decoder)
{
    if (!decoder->frameCount())
        return nullptr;
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    if (!frame || frame->getStatus() != ImageFrame::FrameComplete)
        return nullptr;
    SkBitmap bitmap = frame->bitmap();
    if (!frameIsValid(bitmap))
        return nullptr;
    return fromSkSp(SkImage::MakeFromBitmap(bitmap));
}

bool ImageBitmap::isResizeOptionValid(const ImageBitmapOptions& options, ExceptionState& exceptionState)
{
    if ((options.hasResizeWidth() && options.resizeWidth() == 0) || (options.hasResizeHeight() && options.resizeHeight() == 0)) {
        exceptionState.throwDOMException(InvalidStateError, "The resizeWidth or/and resizeHeight is equal to 0.");
        return false;
    }
    return true;
}

bool ImageBitmap::isSourceSizeValid(int sourceWidth, int sourceHeight, ExceptionState& exceptionState)
{
    if (!sourceWidth || !sourceHeight) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sourceWidth ? "height" : "width"));
        return false;
    }
    return true;
}

// The parameter imageFormat indicates whether the first parameter "image" is unpremultiplied or not.
// imageFormat = PremultiplyAlpha means the image is in premuliplied format
// For example, if the image is already in unpremultiplied format and we want the created ImageBitmap
// in the same format, then we don't need to use the ImageDecoder to decode the image.
static PassRefPtr<StaticBitmapImage> cropImage(Image* image, const ParsedOptions& parsedOptions, AlphaDisposition imageFormat = PremultiplyAlpha, ImageDecoder::GammaAndColorProfileOption colorSpaceOp = ImageDecoder::GammaAndColorProfileApplied)
{
    ASSERT(image);

    IntRect imgRect(IntPoint(), IntSize(image->width(), image->height()));
    const IntRect srcRect = intersection(imgRect, parsedOptions.cropRect);

    // In the case when cropRect doesn't intersect the source image and it requires a umpremul image
    // We immediately return a transparent black image with cropRect.size()
    if (srcRect.isEmpty() && !parsedOptions.premultiplyAlpha) {
        SkImageInfo info = SkImageInfo::Make(parsedOptions.resizeWidth, parsedOptions.resizeHeight, kN32_SkColorType, kUnpremul_SkAlphaType);
        std::unique_ptr<uint8_t[]> dstPixels = wrapArrayUnique(new uint8_t[info.width() * info.height() * info.bytesPerPixel()]());
        return StaticBitmapImage::create(newSkImageFromRaster(info, std::move(dstPixels), info.width() * info.bytesPerPixel()));
    }

    RefPtr<SkImage> skiaImage = image->imageForCurrentFrame();
    // Attempt to get raw unpremultiplied image data, executed only when skiaImage is premultiplied.
    if ((((!parsedOptions.premultiplyAlpha && !skiaImage->isOpaque()) || !skiaImage) && image->data() && imageFormat == PremultiplyAlpha) || colorSpaceOp == ImageDecoder::GammaAndColorProfileIgnored) {
        std::unique_ptr<ImageDecoder> decoder(ImageDecoder::create(
            ImageDecoder::determineImageType(*(image->data())),
            parsedOptions.premultiplyAlpha ? ImageDecoder::AlphaPremultiplied : ImageDecoder::AlphaNotPremultiplied,
            colorSpaceOp));
        if (!decoder)
            return nullptr;
        decoder->setData(image->data(), true);
        skiaImage = ImageBitmap::getSkImageFromDecoder(std::move(decoder));
        if (!skiaImage)
            return nullptr;
    }

    if (parsedOptions.cropRect == srcRect && !parsedOptions.shouldScaleInput) {
        RefPtr<SkImage> croppedSkImage = fromSkSp(skiaImage->makeSubset(srcRect));
        if (parsedOptions.flipY)
            return StaticBitmapImage::create(flipSkImageVertically(croppedSkImage.get(), parsedOptions.premultiplyAlpha ? PremultiplyAlpha : DontPremultiplyAlpha));
        // Special case: The first parameter image is unpremul but we need to turn it into premul.
        if (parsedOptions.premultiplyAlpha && imageFormat == DontPremultiplyAlpha)
            return StaticBitmapImage::create(unPremulSkImageToPremul(croppedSkImage.get()));
        // Call preroll to trigger image decoding.
        croppedSkImage->preroll();
        return StaticBitmapImage::create(croppedSkImage.release());
    }

    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(parsedOptions.resizeWidth, parsedOptions.resizeHeight);
    if (!surface)
        return nullptr;
    if (srcRect.isEmpty())
        return StaticBitmapImage::create(fromSkSp(surface->makeImageSnapshot()));

    SkScalar dstLeft = std::min(0, -parsedOptions.cropRect.x());
    SkScalar dstTop = std::min(0, -parsedOptions.cropRect.y());
    if (parsedOptions.cropRect.x() < 0)
        dstLeft = -parsedOptions.cropRect.x();
    if (parsedOptions.cropRect.y() < 0)
        dstTop = -parsedOptions.cropRect.y();
    if (parsedOptions.flipY) {
        surface->getCanvas()->translate(0, surface->height());
        surface->getCanvas()->scale(1, -1);
    }
    if (parsedOptions.shouldScaleInput) {
        SkRect drawSrcRect = SkRect::MakeXYWH(parsedOptions.cropRect.x(), parsedOptions.cropRect.y(), parsedOptions.cropRect.width(), parsedOptions.cropRect.height());
        SkRect drawDstRect = SkRect::MakeXYWH(0, 0, parsedOptions.resizeWidth, parsedOptions.resizeHeight);
        SkPaint paint;
        paint.setFilterQuality(parsedOptions.resizeQuality);
        surface->getCanvas()->drawImageRect(skiaImage.get(), drawSrcRect, drawDstRect, &paint);
    } else {
        surface->getCanvas()->drawImage(skiaImage.get(), dstLeft, dstTop);
    }
    skiaImage = fromSkSp(surface->makeImageSnapshot());

    if (parsedOptions.premultiplyAlpha) {
        if (imageFormat == PremultiplyAlpha)
            return StaticBitmapImage::create(unPremulSkImageToPremul(skiaImage.get()));
        return StaticBitmapImage::create(skiaImage.release());
    }
    return StaticBitmapImage::create(premulSkImageToUnPremul(skiaImage.get()));
}

ImageBitmap::ImageBitmap(HTMLImageElement* image, Optional<IntRect> cropRect, Document* document, const ImageBitmapOptions& options)
{
    RefPtr<Image> input = image->cachedImage()->getImage();
    ParsedOptions parsedOptions = parseOptions(options, cropRect, image->bitmapSourceSize());

    if (options.colorSpaceConversion() == "none")
        m_image = cropImage(input.get(), parsedOptions, PremultiplyAlpha, ImageDecoder::GammaAndColorProfileIgnored);
    else
        m_image = cropImage(input.get(), parsedOptions, PremultiplyAlpha, ImageDecoder::GammaAndColorProfileApplied);
    if (!m_image)
        return;
    // In the case where the source image is lazy-decoded, m_image may not be in
    // a decoded state, we trigger it here.
    RefPtr<SkImage> skImage = m_image->imageForCurrentFrame();
    SkPixmap pixmap;
    if (!skImage->isTextureBacked() && !skImage->peekPixels(&pixmap)) {
        sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(skImage->width(), skImage->height());
        surface->getCanvas()->drawImage(skImage.get(), 0, 0);
        m_image = StaticBitmapImage::create(fromSkSp(surface->makeImageSnapshot()));
    }
    m_image->setOriginClean(!image->wouldTaintOrigin(document->getSecurityOrigin()));
    m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video, Optional<IntRect> cropRect, Document* document, const ImageBitmapOptions& options)
{
    IntSize playerSize;
    if (video->webMediaPlayer())
        playerSize = video->webMediaPlayer()->naturalSize();

    // TODO(xidachen); implement the resize option.
    ParsedOptions parsedOptions = parseOptions(options, cropRect, video->bitmapSourceSize());

    IntRect videoRect = IntRect(IntPoint(), playerSize);
    IntRect srcRect = intersection(parsedOptions.cropRect, videoRect);
    std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(parsedOptions.cropRect.size(), NonOpaque, DoNotInitializeImagePixels);
    if (!buffer)
        return;

    if (parsedOptions.flipY) {
        buffer->canvas()->translate(0, buffer->size().height());
        buffer->canvas()->scale(1, -1);
    }
    IntPoint dstPoint = IntPoint(std::max(0, -parsedOptions.cropRect.x()), std::max(0, -parsedOptions.cropRect.y()));
    video->paintCurrentFrame(buffer->canvas(), IntRect(dstPoint, srcRect.size()), nullptr);

    RefPtr<SkImage> skiaImage = buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
    if (!parsedOptions.premultiplyAlpha)
        skiaImage = premulSkImageToUnPremul(skiaImage.get());
    m_image = StaticBitmapImage::create(skiaImage.release());
    m_image->setOriginClean(!video->wouldTaintOrigin(document->getSecurityOrigin()));
    m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    ASSERT(canvas->isPaintable());
    RefPtr<Image> input = canvas->copiedImage(BackBuffer, PreferAcceleration);
    ParsedOptions parsedOptions = parseOptions(options, cropRect, canvas->bitmapSourceSize());

    bool isPremultiplyAlphaReverted = false;
    if (!parsedOptions.premultiplyAlpha) {
        parsedOptions.premultiplyAlpha = true;
        isPremultiplyAlphaReverted = true;
    }
    m_image = cropImage(input.get(), parsedOptions);
    if (!m_image)
        return;
    if (isPremultiplyAlphaReverted) {
        parsedOptions.premultiplyAlpha = false;
        m_image = StaticBitmapImage::create(premulSkImageToUnPremul(m_image->imageForCurrentFrame().get()));
    }
    m_image->setOriginClean(canvas->originClean());
    m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(std::unique_ptr<uint8_t[]> data, uint32_t width, uint32_t height, bool isImageBitmapPremultiplied, bool isImageBitmapOriginClean)
{
    SkImageInfo info = SkImageInfo::MakeN32(width, height, isImageBitmapPremultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
    m_image = StaticBitmapImage::create(fromSkSp(SkImage::MakeRasterCopy(SkPixmap(info, data.get(), info.bytesPerPixel() * width))));
    m_image->setPremultiplied(isImageBitmapPremultiplied);
    m_image->setOriginClean(isImageBitmapOriginClean);
}

static PassRefPtr<SkImage> scaleSkImage(PassRefPtr<SkImage> skImage, unsigned resizeWidth, unsigned resizeHeight, SkFilterQuality resizeQuality)
{
    SkImageInfo resizedInfo = SkImageInfo::Make(resizeWidth, resizeHeight, kN32_SkColorType, kUnpremul_SkAlphaType);
    std::unique_ptr<uint8_t[]> resizedPixels = wrapArrayUnique(new uint8_t[resizeWidth * resizeHeight * resizedInfo.bytesPerPixel()]);
    SkPixmap pixmap(resizedInfo, resizedPixels.release(), resizeWidth * resizedInfo.bytesPerPixel());
    skImage->scalePixels(pixmap, resizeQuality);
    return fromSkSp(SkImage::MakeFromRaster(pixmap, [](const void* pixels, void*)
        {
            delete[] static_cast<const uint8_t*>(pixels);
        }, nullptr));
}

ImageBitmap::ImageBitmap(ImageData* data, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    // TODO(xidachen): implement the resize option
    IntRect dataSrcRect = IntRect(IntPoint(), data->size());
    ParsedOptions parsedOptions = parseOptions(options, cropRect, data->bitmapSourceSize());
    IntRect srcRect = cropRect ? intersection(parsedOptions.cropRect, dataSrcRect) : dataSrcRect;

    // treat non-premultiplyAlpha as a special case
    if (!parsedOptions.premultiplyAlpha) {
        unsigned char* srcAddr = data->data()->data();
        int srcHeight = data->size().height();
        int dstHeight = parsedOptions.cropRect.height();

        // Using kN32 type, swizzle input if necessary.
        SkImageInfo info = SkImageInfo::Make(parsedOptions.cropRect.width(), dstHeight, kN32_SkColorType, kUnpremul_SkAlphaType);
        int srcPixelBytesPerRow = info.bytesPerPixel() * data->size().width();
        int dstPixelBytesPerRow = info.bytesPerPixel() * parsedOptions.cropRect.width();
        RefPtr<SkImage> skImage;
        if (parsedOptions.cropRect == IntRect(IntPoint(), data->size())) {
            if (kN32_SkColorType == kBGRA_8888_SkColorType)
                swizzleImageData(srcAddr, srcHeight, srcPixelBytesPerRow, parsedOptions.flipY);
            skImage = fromSkSp(SkImage::MakeRasterCopy(SkPixmap(info, srcAddr, dstPixelBytesPerRow)));
            // restore the original ImageData
            if (kN32_SkColorType == kBGRA_8888_SkColorType)
                swizzleImageData(srcAddr, srcHeight, srcPixelBytesPerRow, parsedOptions.flipY);
        } else {
            std::unique_ptr<uint8_t[]> copiedDataBuffer = wrapArrayUnique(new uint8_t[dstHeight * dstPixelBytesPerRow]());
            if (!srcRect.isEmpty()) {
                IntPoint srcPoint = IntPoint((parsedOptions.cropRect.x() > 0) ? parsedOptions.cropRect.x() : 0, (parsedOptions.cropRect.y() > 0) ? parsedOptions.cropRect.y() : 0);
                IntPoint dstPoint = IntPoint((parsedOptions.cropRect.x() >= 0) ? 0 : -parsedOptions.cropRect.x(), (parsedOptions.cropRect.y() >= 0) ? 0 : -parsedOptions.cropRect.y());
                int copyHeight = srcHeight - srcPoint.y();
                if (parsedOptions.cropRect.height() < copyHeight)
                    copyHeight = parsedOptions.cropRect.height();
                int copyWidth = data->size().width() - srcPoint.x();
                if (parsedOptions.cropRect.width() < copyWidth)
                    copyWidth = parsedOptions.cropRect.width();
                for (int i = 0; i < copyHeight; i++) {
                    int srcStartCopyPosition = (i + srcPoint.y()) * srcPixelBytesPerRow + srcPoint.x() * info.bytesPerPixel();
                    int srcEndCopyPosition = srcStartCopyPosition + copyWidth * info.bytesPerPixel();
                    int dstStartCopyPosition;
                    if (parsedOptions.flipY)
                        dstStartCopyPosition = (dstHeight -1 - dstPoint.y() - i) * dstPixelBytesPerRow + dstPoint.x() * info.bytesPerPixel();
                    else
                        dstStartCopyPosition = (dstPoint.y() + i) * dstPixelBytesPerRow + dstPoint.x() * info.bytesPerPixel();
                    for (int j = 0; j < srcEndCopyPosition - srcStartCopyPosition; j++) {
                        // swizzle when necessary
                        if (kN32_SkColorType == kBGRA_8888_SkColorType) {
                            if (j % 4 == 0)
                                copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j + 2];
                            else if (j % 4 == 2)
                                copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j - 2];
                            else
                                copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j];
                        } else {
                            copiedDataBuffer[dstStartCopyPosition + j] = srcAddr[srcStartCopyPosition + j];
                        }
                    }
                }
            }
            skImage = newSkImageFromRaster(info, std::move(copiedDataBuffer), dstPixelBytesPerRow);
        }
        if (parsedOptions.shouldScaleInput)
            m_image = StaticBitmapImage::create(scaleSkImage(skImage, parsedOptions.resizeWidth, parsedOptions.resizeHeight, parsedOptions.resizeQuality));
        else
            m_image = StaticBitmapImage::create(skImage);
        m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
        return;
    }

    std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(parsedOptions.cropRect.size(), NonOpaque, DoNotInitializeImagePixels);
    if (!buffer)
        return;

    if (srcRect.isEmpty()) {
        m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown));
        return;
    }

    IntPoint dstPoint = IntPoint(std::min(0, -parsedOptions.cropRect.x()), std::min(0, -parsedOptions.cropRect.y()));
    if (parsedOptions.cropRect.x() < 0)
        dstPoint.setX(-parsedOptions.cropRect.x());
    if (parsedOptions.cropRect.y() < 0)
        dstPoint.setY(-parsedOptions.cropRect.y());
    buffer->putByteArray(Unmultiplied, data->data()->data(), data->size(), srcRect, dstPoint);
    RefPtr<SkImage> skImage = buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
    if (parsedOptions.flipY)
        skImage = flipSkImageVertically(skImage.get(), PremultiplyAlpha);
    if (parsedOptions.shouldScaleInput) {
        sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(parsedOptions.resizeWidth, parsedOptions.resizeHeight);
        if (!surface)
            return;
        SkPaint paint;
        paint.setFilterQuality(parsedOptions.resizeQuality);
        SkRect dstDrawRect = SkRect::MakeWH(parsedOptions.resizeWidth, parsedOptions.resizeHeight);
        surface->getCanvas()->drawImageRect(skImage.get(), dstDrawRect, &paint);
        skImage = fromSkSp(surface->makeImageSnapshot());
    }
    m_image = StaticBitmapImage::create(skImage);
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    RefPtr<Image> input = bitmap->bitmapImage();
    if (!input)
        return;
    ParsedOptions parsedOptions = parseOptions(options, cropRect, input->size());

    m_image = cropImage(input.get(), parsedOptions, bitmap->isPremultiplied() ? PremultiplyAlpha : DontPremultiplyAlpha);
    if (!m_image)
        return;
    m_image->setOriginClean(bitmap->originClean());
    m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    bool originClean = image->originClean();
    RefPtr<Image> input = image;
    ParsedOptions parsedOptions = parseOptions(options, cropRect, input->size());

    m_image = cropImage(input.get(), parsedOptions, DontPremultiplyAlpha);
    if (!m_image)
        return;
    m_image->setOriginClean(originClean);
    m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image)
{
    m_image = image;
}

ImageBitmap::ImageBitmap(WebExternalTextureMailbox& mailbox)
{
    m_image = StaticBitmapImage::create(mailbox);
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

ImageBitmap* ImageBitmap::create(HTMLImageElement* image, Optional<IntRect> cropRect, Document* document, const ImageBitmapOptions& options)
{
    return new ImageBitmap(image, cropRect, document, options);
}

ImageBitmap* ImageBitmap::create(HTMLVideoElement* video, Optional<IntRect> cropRect, Document* document, const ImageBitmapOptions& options)
{
    return new ImageBitmap(video, cropRect, document, options);
}

ImageBitmap* ImageBitmap::create(HTMLCanvasElement* canvas, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    return new ImageBitmap(canvas, cropRect, options);
}

ImageBitmap* ImageBitmap::create(ImageData* data, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    return new ImageBitmap(data, cropRect, options);
}

ImageBitmap* ImageBitmap::create(ImageBitmap* bitmap, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    return new ImageBitmap(bitmap, cropRect, options);
}

ImageBitmap* ImageBitmap::create(PassRefPtr<StaticBitmapImage> image, Optional<IntRect> cropRect, const ImageBitmapOptions& options)
{
    return new ImageBitmap(image, cropRect, options);
}

ImageBitmap* ImageBitmap::create(PassRefPtr<StaticBitmapImage> image)
{
    return new ImageBitmap(image);
}

ImageBitmap* ImageBitmap::create(WebExternalTextureMailbox& mailbox)
{
    return new ImageBitmap(mailbox);
}

ImageBitmap* ImageBitmap::create(std::unique_ptr<uint8_t[]> data, uint32_t width, uint32_t height, bool isImageBitmapPremultiplied, bool isImageBitmapOriginClean)
{
    return new ImageBitmap(std::move(data), width, height, isImageBitmapPremultiplied, isImageBitmapOriginClean);
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

std::unique_ptr<uint8_t[]> ImageBitmap::copyBitmapData(AlphaDisposition alphaOp, DataColorFormat format)
{
    SkImageInfo info = SkImageInfo::Make(width(), height(), (format == RGBAColorType) ? kRGBA_8888_SkColorType : kN32_SkColorType, (alphaOp == PremultiplyAlpha) ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
    std::unique_ptr<uint8_t[]> dstPixels = copySkImageData(m_image->imageForCurrentFrame().get(), info);
    return dstPixels;
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

bool ImageBitmap::isTextureBacked() const
{
    return m_image && (m_image->isTextureBacked() || m_image->hasMailbox());
}

IntSize ImageBitmap::size() const
{
    if (!m_image)
        return IntSize();
    ASSERT(m_image->width() > 0 && m_image->height() > 0);
    return IntSize(m_image->width(), m_image->height());
}

ScriptPromise ImageBitmap::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, Optional<IntRect> cropRect, const ImageBitmapOptions& options, ExceptionState& exceptionState)
{
    if ((cropRect && !isSourceSizeValid(cropRect->width(), cropRect->height(), exceptionState))
        || !isSourceSizeValid(width(), height(), exceptionState))
        return ScriptPromise();
    if (!isResizeOptionValid(options, exceptionState))
        return ScriptPromise();
    return ImageBitmapSource::fulfillImageBitmap(scriptState, create(this, cropRect, options));
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
