/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "core/frame/ImageBitmap.h"

#include "SkPixelRef.h"  // FIXME: qualify this skia header file.
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/heap/Handle.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"

namespace blink {

class ExceptionState;

class ImageBitmapTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 10);
    surface->getCanvas()->clear(0xFFFFFFFF);
    m_image = surface->makeImageSnapshot();

    sk_sp<SkSurface> surface2 = SkSurface::MakeRasterN32Premul(5, 5);
    surface2->getCanvas()->clear(0xAAAAAAAA);
    m_image2 = surface2->makeImageSnapshot();

    // Save the global memory cache to restore it upon teardown.
    m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());

    // Save the state of experimental canvas features and color correct
    // rendering flags to restore them on teardown.
    experimentalCanvasFeatures =
        RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
    colorCorrectRendering =
        RuntimeEnabledFeatures::colorCorrectRenderingEnabled();
    colorCorrectRenderingDefaultMode =
        RuntimeEnabledFeatures::colorCorrectRenderingDefaultModeEnabled();
  }
  virtual void TearDown() {
    // Garbage collection is required prior to switching out the
    // test's memory cache; image resources are released, evicting
    // them from the cache.
    ThreadState::current()->collectGarbage(BlinkGC::NoHeapPointersOnStack,
                                           BlinkGC::GCWithSweep,
                                           BlinkGC::ForcedGC);

    replaceMemoryCacheForTesting(m_globalMemoryCache.release());
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(
        experimentalCanvasFeatures);
    RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(
        colorCorrectRendering);
    RuntimeEnabledFeatures::setColorCorrectRenderingDefaultModeEnabled(
        colorCorrectRenderingDefaultMode);
  }

  sk_sp<SkImage> m_image, m_image2;
  Persistent<MemoryCache> m_globalMemoryCache;
  bool experimentalCanvasFeatures;
  bool colorCorrectRendering;
  bool colorCorrectRenderingDefaultMode;
};

TEST_F(ImageBitmapTest, ImageResourceConsistency) {
  const ImageBitmapOptions defaultOptions;
  HTMLImageElement* imageElement =
      HTMLImageElement::create(*Document::create());
  ImageResourceContent* image =
      ImageResourceContent::create(StaticBitmapImage::create(m_image).get());
  imageElement->setImageResource(image);

  Optional<IntRect> cropRect =
      IntRect(0, 0, m_image->width(), m_image->height());
  ImageBitmap* imageBitmapNoCrop = ImageBitmap::create(
      imageElement, cropRect, &(imageElement->document()), defaultOptions);
  cropRect = IntRect(m_image->width() / 2, m_image->height() / 2,
                     m_image->width() / 2, m_image->height() / 2);
  ImageBitmap* imageBitmapInteriorCrop = ImageBitmap::create(
      imageElement, cropRect, &(imageElement->document()), defaultOptions);
  cropRect = IntRect(-m_image->width() / 2, -m_image->height() / 2,
                     m_image->width(), m_image->height());
  ImageBitmap* imageBitmapExteriorCrop = ImageBitmap::create(
      imageElement, cropRect, &(imageElement->document()), defaultOptions);
  cropRect = IntRect(-m_image->width(), -m_image->height(), m_image->width(),
                     m_image->height());
  ImageBitmap* imageBitmapOutsideCrop = ImageBitmap::create(
      imageElement, cropRect, &(imageElement->document()), defaultOptions);

  ASSERT_NE(imageBitmapNoCrop->bitmapImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()),
            imageElement->cachedImage()->getImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()));
  ASSERT_NE(imageBitmapInteriorCrop->bitmapImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()),
            imageElement->cachedImage()->getImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()));
  ASSERT_NE(imageBitmapExteriorCrop->bitmapImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()),
            imageElement->cachedImage()->getImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()));

  StaticBitmapImage* emptyImage = imageBitmapOutsideCrop->bitmapImage();
  ASSERT_NE(emptyImage->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()),
            imageElement->cachedImage()->getImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()));
}

// Verifies that ImageBitmaps constructed from HTMLImageElements hold a
// reference to the original Image if the HTMLImageElement src is changed.
TEST_F(ImageBitmapTest, ImageBitmapSourceChanged) {
  HTMLImageElement* image = HTMLImageElement::create(*Document::create());
  ImageResourceContent* originalImageResource =
      ImageResourceContent::create(StaticBitmapImage::create(m_image).get());
  image->setImageResource(originalImageResource);

  const ImageBitmapOptions defaultOptions;
  Optional<IntRect> cropRect =
      IntRect(0, 0, m_image->width(), m_image->height());
  ImageBitmap* imageBitmap = ImageBitmap::create(
      image, cropRect, &(image->document()), defaultOptions);
  // As we are applying color space conversion for the "default" mode,
  // this verifies that the color corrected image is not the same as the
  // source.
  ASSERT_NE(imageBitmap->bitmapImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()),
            originalImageResource->getImage()->imageForCurrentFrame(
                ColorBehavior::transformToTargetForTesting()));

  ImageResourceContent* newImageResource =
      ImageResourceContent::create(StaticBitmapImage::create(m_image2).get());
  image->setImageResource(newImageResource);

  {
    ASSERT_NE(imageBitmap->bitmapImage()->imageForCurrentFrame(
                  ColorBehavior::transformToTargetForTesting()),
              originalImageResource->getImage()->imageForCurrentFrame(
                  ColorBehavior::transformToTargetForTesting()));
    SkImage* image1 =
        imageBitmap->bitmapImage()
            ->imageForCurrentFrame(ColorBehavior::transformToTargetForTesting())
            .get();
    ASSERT_NE(image1, nullptr);
    SkImage* image2 =
        originalImageResource->getImage()
            ->imageForCurrentFrame(ColorBehavior::transformToTargetForTesting())
            .get();
    ASSERT_NE(image2, nullptr);
    ASSERT_NE(image1, image2);
  }

  {
    ASSERT_NE(imageBitmap->bitmapImage()->imageForCurrentFrame(
                  ColorBehavior::transformToTargetForTesting()),
              newImageResource->getImage()->imageForCurrentFrame(
                  ColorBehavior::transformToTargetForTesting()));
    SkImage* image1 =
        imageBitmap->bitmapImage()
            ->imageForCurrentFrame(ColorBehavior::transformToTargetForTesting())
            .get();
    ASSERT_NE(image1, nullptr);
    SkImage* image2 =
        newImageResource->getImage()
            ->imageForCurrentFrame(ColorBehavior::transformToTargetForTesting())
            .get();
    ASSERT_NE(image2, nullptr);
    ASSERT_NE(image1, image2);
  }
}

enum class ColorSpaceConversion : uint8_t {
  NONE = 0,
  DEFAULT_NOT_COLOR_CORRECTED = 1,
  DEFAULT_COLOR_CORRECTED = 2,
  SRGB = 3,
  LINEAR_RGB = 4,

  LAST = LINEAR_RGB
};

static ImageBitmapOptions prepareBitmapOptionsAndSetRuntimeFlags(
    const ColorSpaceConversion& colorSpaceConversion) {
  // Set the color space conversion in ImageBitmapOptions
  ImageBitmapOptions options;
  static const Vector<String> conversions = {"none", "default", "default",
                                             "srgb", "linear-rgb"};
  options.setColorSpaceConversion(
      conversions[static_cast<uint8_t>(colorSpaceConversion)]);

  // Set the runtime flags
  bool flag = (colorSpaceConversion !=
               ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED);
  RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(true);
  RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(flag);
  RuntimeEnabledFeatures::setColorCorrectRenderingDefaultModeEnabled(!flag);

  return options;
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionHTMLImageElement) {
  HTMLImageElement* imageElement =
      HTMLImageElement::create(*Document::create());

  SkPaint p;
  p.setColor(SK_ColorRED);
  sk_sp<SkColorSpace> srcRGBColorSpace = SkColorSpace::MakeSRGB();

  SkImageInfo rasterImageInfo =
      SkImageInfo::MakeN32Premul(10, 10, srcRGBColorSpace);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(rasterImageInfo));
  surface->getCanvas()->drawCircle(5, 5, 5, p);
  sk_sp<SkImage> image = surface->makeImageSnapshot();

  std::unique_ptr<uint8_t[]> srcPixel(
      new uint8_t[rasterImageInfo.bytesPerPixel()]());
  image->readPixels(rasterImageInfo.makeWH(1, 1), srcPixel.get(),
                    image->width() * rasterImageInfo.bytesPerPixel(), 5, 5);

  ImageResourceContent* originalImageResource =
      ImageResourceContent::create(StaticBitmapImage::create(image).get());
  imageElement->setImageResource(originalImageResource);

  Optional<IntRect> cropRect = IntRect(0, 0, image->width(), image->height());

  // Create and test the ImageBitmap objects.
  // We don't check "none" color space conversion as it requires the encoded
  // data in a format readable by ImageDecoder. Furthermore, the code path for
  // "none" color space conversion is not affected by this CL.

  sk_sp<SkColorSpace> colorSpace = nullptr;
  SkColorType colorType = SkColorType::kN32_SkColorType;
  SkColorSpaceXform::ColorFormat colorFormat32 =
      (colorType == kBGRA_8888_SkColorType)
          ? SkColorSpaceXform::ColorFormat::kBGRA_8888_ColorFormat
          : SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorSpaceXform::ColorFormat colorFormat = colorFormat32;

  for (uint8_t i = static_cast<uint8_t>(
           ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED);
       i <= static_cast<uint8_t>(ColorSpaceConversion::LAST); i++) {
    ColorSpaceConversion colorSpaceConversion =
        static_cast<ColorSpaceConversion>(i);
    ImageBitmapOptions options =
        prepareBitmapOptionsAndSetRuntimeFlags(colorSpaceConversion);
    ImageBitmap* imageBitmap = ImageBitmap::create(
        imageElement, cropRect, &(imageElement->document()), options);

    // ColorBehavior::ignore() is used instead of
    // ColorBehavior::transformToTargetForTesting() to avoid color conversion to
    // display color profile, as we want to solely rely on the color correction
    // that happens in ImageBitmap create method.
    SkImage* convertedImage =
        imageBitmap->bitmapImage()
            ->imageForCurrentFrame(ColorBehavior::ignore())
            .get();

    switch (colorSpaceConversion) {
      case ColorSpaceConversion::NONE:
        NOTREACHED();
        break;
      case ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED:
        colorSpace = ColorBehavior::globalTargetColorSpace().ToSkColorSpace();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::DEFAULT_COLOR_CORRECTED:
      case ColorSpaceConversion::SRGB:
        colorSpace = SkColorSpace::MakeSRGB();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::LINEAR_RGB:
        colorSpace = SkColorSpace::MakeSRGBLinear();
        colorType = SkColorType::kRGBA_F16_SkColorType;
        colorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      default:
        NOTREACHED();
    }

    SkImageInfo imageInfo = SkImageInfo::Make(
        1, 1, colorType, SkAlphaType::kPremul_SkAlphaType, colorSpace);
    std::unique_ptr<uint8_t[]> convertedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    convertedImage->readPixels(
        imageInfo, convertedPixel.get(),
        convertedImage->width() * imageInfo.bytesPerPixel(), 5, 5);

    // Transform the source pixel and check if the image bitmap color conversion
    // is done correctly.
    std::unique_ptr<SkColorSpaceXform> colorSpaceXform =
        SkColorSpaceXform::New(srcRGBColorSpace.get(), colorSpace.get());
    std::unique_ptr<uint8_t[]> transformedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    colorSpaceXform->apply(colorFormat, transformedPixel.get(), colorFormat32,
                           srcPixel.get(), 1, SkAlphaType::kPremul_SkAlphaType);

    int compare = std::memcmp(convertedPixel.get(), transformedPixel.get(),
                              imageInfo.bytesPerPixel());
    ASSERT_EQ(compare, 0);
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionImageBitmap) {
  HTMLImageElement* imageElement =
      HTMLImageElement::create(*Document::create());

  SkPaint p;
  p.setColor(SK_ColorRED);
  sk_sp<SkColorSpace> srcRGBColorSpace = SkColorSpace::MakeSRGB();

  SkImageInfo rasterImageInfo =
      SkImageInfo::MakeN32Premul(10, 10, srcRGBColorSpace);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(rasterImageInfo));
  surface->getCanvas()->drawCircle(5, 5, 5, p);
  sk_sp<SkImage> image = surface->makeImageSnapshot();

  std::unique_ptr<uint8_t[]> srcPixel(
      new uint8_t[rasterImageInfo.bytesPerPixel()]());
  image->readPixels(rasterImageInfo.makeWH(1, 1), srcPixel.get(),
                    image->width() * rasterImageInfo.bytesPerPixel(), 5, 5);

  ImageResourceContent* sourceImageResource =
      ImageResourceContent::create(StaticBitmapImage::create(image).get());
  imageElement->setImageResource(sourceImageResource);

  Optional<IntRect> cropRect = IntRect(0, 0, image->width(), image->height());
  ImageBitmapOptions options =
      prepareBitmapOptionsAndSetRuntimeFlags(ColorSpaceConversion::SRGB);
  ImageBitmap* sourceImageBitmap = ImageBitmap::create(
      imageElement, cropRect, &(imageElement->document()), options);

  sk_sp<SkColorSpace> colorSpace = nullptr;
  SkColorType colorType = SkColorType::kN32_SkColorType;
  SkColorSpaceXform::ColorFormat colorFormat32 =
      (colorType == kBGRA_8888_SkColorType)
          ? SkColorSpaceXform::ColorFormat::kBGRA_8888_ColorFormat
          : SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorSpaceXform::ColorFormat colorFormat = colorFormat32;

  for (uint8_t i = static_cast<uint8_t>(
           ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED);
       i <= static_cast<uint8_t>(ColorSpaceConversion::LAST); i++) {
    ColorSpaceConversion colorSpaceConversion =
        static_cast<ColorSpaceConversion>(i);
    options = prepareBitmapOptionsAndSetRuntimeFlags(colorSpaceConversion);
    ImageBitmap* imageBitmap =
        ImageBitmap::create(sourceImageBitmap, cropRect, options);
    // ColorBehavior::ignore() is used instead of
    // ColorBehavior::transformToTargetForTesting() to avoid color conversion to
    // display color profile, as we want to solely rely on the color correction
    // that happens in ImageBitmap create method.
    SkImage* convertedImage =
        imageBitmap->bitmapImage()
            ->imageForCurrentFrame(ColorBehavior::ignore())
            .get();

    switch (colorSpaceConversion) {
      case ColorSpaceConversion::NONE:
        NOTREACHED();
        break;
      case ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED:
        colorSpace = ColorBehavior::globalTargetColorSpace().ToSkColorSpace();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::DEFAULT_COLOR_CORRECTED:
      case ColorSpaceConversion::SRGB:
        colorSpace = SkColorSpace::MakeSRGB();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::LINEAR_RGB:
        colorSpace = SkColorSpace::MakeSRGBLinear();
        colorType = SkColorType::kRGBA_F16_SkColorType;
        colorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      default:
        NOTREACHED();
    }

    SkImageInfo imageInfo = SkImageInfo::Make(
        1, 1, colorType, SkAlphaType::kPremul_SkAlphaType, colorSpace);
    std::unique_ptr<uint8_t[]> convertedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    convertedImage->readPixels(
        imageInfo, convertedPixel.get(),
        convertedImage->width() * imageInfo.bytesPerPixel(), 5, 5);

    // Transform the source pixel and check if the image bitmap color conversion
    // is done correctly.
    std::unique_ptr<SkColorSpaceXform> colorSpaceXform =
        SkColorSpaceXform::New(srcRGBColorSpace.get(), colorSpace.get());
    std::unique_ptr<uint8_t[]> transformedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    colorSpaceXform->apply(colorFormat, transformedPixel.get(), colorFormat32,
                           srcPixel.get(), 1, SkAlphaType::kPremul_SkAlphaType);

    int compare = std::memcmp(convertedPixel.get(), transformedPixel.get(),
                              imageInfo.bytesPerPixel());
    ASSERT_EQ(compare, 0);
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionStaticBitmapImage) {
  SkPaint p;
  p.setColor(SK_ColorRED);
  sk_sp<SkColorSpace> srcRGBColorSpace = SkColorSpace::MakeSRGB();

  SkImageInfo rasterImageInfo =
      SkImageInfo::MakeN32Premul(10, 10, srcRGBColorSpace);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(rasterImageInfo));
  surface->getCanvas()->drawCircle(5, 5, 5, p);
  sk_sp<SkImage> image = surface->makeImageSnapshot();

  std::unique_ptr<uint8_t[]> srcPixel(
      new uint8_t[rasterImageInfo.bytesPerPixel()]());
  image->readPixels(rasterImageInfo.makeWH(1, 1), srcPixel.get(),
                    image->width() * rasterImageInfo.bytesPerPixel(), 5, 5);

  Optional<IntRect> cropRect = IntRect(0, 0, image->width(), image->height());

  sk_sp<SkColorSpace> colorSpace = nullptr;
  SkColorType colorType = SkColorType::kN32_SkColorType;
  SkColorSpaceXform::ColorFormat colorFormat32 =
      (colorType == kBGRA_8888_SkColorType)
          ? SkColorSpaceXform::ColorFormat::kBGRA_8888_ColorFormat
          : SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorSpaceXform::ColorFormat colorFormat = colorFormat32;

  for (uint8_t i = static_cast<uint8_t>(
           ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED);
       i <= static_cast<uint8_t>(ColorSpaceConversion::LAST); i++) {
    ColorSpaceConversion colorSpaceConversion =
        static_cast<ColorSpaceConversion>(i);
    ImageBitmapOptions options =
        prepareBitmapOptionsAndSetRuntimeFlags(colorSpaceConversion);
    ImageBitmap* imageBitmap = ImageBitmap::create(
        StaticBitmapImage::create(image), cropRect, options);

    // ColorBehavior::ignore() is used instead of
    // ColorBehavior::transformToTargetForTesting() to avoid color conversion to
    // display color profile, as we want to solely rely on the color correction
    // that happens in ImageBitmap create method.
    SkImage* convertedImage =
        imageBitmap->bitmapImage()
            ->imageForCurrentFrame(ColorBehavior::ignore())
            .get();

    switch (colorSpaceConversion) {
      case ColorSpaceConversion::NONE:
        NOTREACHED();
        break;
      case ColorSpaceConversion::DEFAULT_NOT_COLOR_CORRECTED:
        colorSpace = ColorBehavior::globalTargetColorSpace().ToSkColorSpace();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::DEFAULT_COLOR_CORRECTED:
      case ColorSpaceConversion::SRGB:
        colorSpace = SkColorSpace::MakeSRGB();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::LINEAR_RGB:
        colorSpace = SkColorSpace::MakeSRGBLinear();
        colorType = SkColorType::kRGBA_F16_SkColorType;
        colorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      default:
        NOTREACHED();
    }

    SkImageInfo imageInfo = SkImageInfo::Make(
        1, 1, colorType, SkAlphaType::kPremul_SkAlphaType, colorSpace);
    std::unique_ptr<uint8_t[]> convertedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    convertedImage->readPixels(
        imageInfo, convertedPixel.get(),
        convertedImage->width() * imageInfo.bytesPerPixel(), 5, 5);

    // Transform the source pixel and check if the image bitmap color conversion
    // is done correctly.
    std::unique_ptr<SkColorSpaceXform> colorSpaceXform =
        SkColorSpaceXform::New(srcRGBColorSpace.get(), colorSpace.get());
    std::unique_ptr<uint8_t[]> transformedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    colorSpaceXform->apply(colorFormat, transformedPixel.get(), colorFormat32,
                           srcPixel.get(), 1, SkAlphaType::kPremul_SkAlphaType);

    int compare = std::memcmp(convertedPixel.get(), transformedPixel.get(),
                              imageInfo.bytesPerPixel());
    ASSERT_EQ(compare, 0);
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionImageData) {
  unsigned char dataBuffer[4] = {255, 0, 0, 255};
  DOMUint8ClampedArray* data = DOMUint8ClampedArray::create(dataBuffer, 4);
  ImageData* imageData =
      ImageData::create(IntSize(1, 1), data, kLegacyImageDataColorSpaceName);
  std::unique_ptr<uint8_t[]> srcPixel(new uint8_t[4]());
  memcpy(srcPixel.get(), imageData->data()->data(), 4);

  Optional<IntRect> cropRect = IntRect(0, 0, 1, 1);
  sk_sp<SkColorSpace> colorSpace = nullptr;
  SkColorSpaceXform::ColorFormat colorFormat32 =
      (SkColorType::kN32_SkColorType == kBGRA_8888_SkColorType)
          ? SkColorSpaceXform::ColorFormat::kBGRA_8888_ColorFormat
          : SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorType colorType = SkColorType::kN32_SkColorType;
  SkColorSpaceXform::ColorFormat colorFormat = colorFormat32;

  for (uint8_t i =
           static_cast<uint8_t>(ColorSpaceConversion::DEFAULT_COLOR_CORRECTED);
       i <= static_cast<uint8_t>(ColorSpaceConversion::LAST); i++) {
    ColorSpaceConversion colorSpaceConversion =
        static_cast<ColorSpaceConversion>(i);
    ImageBitmapOptions options =
        prepareBitmapOptionsAndSetRuntimeFlags(colorSpaceConversion);
    ImageBitmap* imageBitmap =
        ImageBitmap::create(imageData, cropRect, options);

    // ColorBehavior::ignore() is used instead of
    // ColorBehavior::transformToTargetForTesting() to avoid color conversion to
    // display color profile, as we want to solely rely on the color correction
    // that happens in ImageBitmap create method.
    SkImage* convertedImage =
        imageBitmap->bitmapImage()
            ->imageForCurrentFrame(ColorBehavior::ignore())
            .get();

    switch (colorSpaceConversion) {
      case ColorSpaceConversion::NONE:
        NOTREACHED();
        break;
      case ColorSpaceConversion::DEFAULT_COLOR_CORRECTED:
      case ColorSpaceConversion::SRGB:
        colorSpace = SkColorSpace::MakeSRGB();
        colorFormat = colorFormat32;
        break;
      case ColorSpaceConversion::LINEAR_RGB:
        colorSpace = SkColorSpace::MakeSRGBLinear();
        colorType = SkColorType::kRGBA_F16_SkColorType;
        colorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      default:
        NOTREACHED();
    }

    SkImageInfo imageInfo = SkImageInfo::Make(
        1, 1, colorType, SkAlphaType::kUnpremul_SkAlphaType, colorSpace);
    std::unique_ptr<uint8_t[]> convertedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    convertedImage->readPixels(
        imageInfo, convertedPixel.get(),
        convertedImage->width() * imageInfo.bytesPerPixel(), 0, 0);

    // Transform the source pixel and check if the pixel from image bitmap has
    // the same color information.
    std::unique_ptr<SkColorSpaceXform> colorSpaceXform = SkColorSpaceXform::New(
        imageData->getSkColorSpace().get(), colorSpace.get());
    std::unique_ptr<uint8_t[]> transformedPixel(
        new uint8_t[imageInfo.bytesPerPixel()]());
    colorSpaceXform->apply(colorFormat, transformedPixel.get(), colorFormat32,
                           srcPixel.get(), 1,
                           SkAlphaType::kUnpremul_SkAlphaType);
    int compare = std::memcmp(convertedPixel.get(), transformedPixel.get(),
                              imageInfo.bytesPerPixel());
    ASSERT_EQ(compare, 0);
  }
}

}  // namespace blink
