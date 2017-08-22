// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/imagebitmap/ImageBitmap.h"

#include <memory>
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/graphics/UnacceleratedStaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"

namespace blink {

constexpr const char* kImageOrientationFlipY = "flipY";
constexpr const char* kImageBitmapOptionNone = "none";
constexpr const char* kImageBitmapOptionDefault = "default";
constexpr const char* kImageBitmapOptionPremultiply = "premultiply";
constexpr const char* kImageBitmapOptionResizeQualityHigh = "high";
constexpr const char* kImageBitmapOptionResizeQualityMedium = "medium";
constexpr const char* kImageBitmapOptionResizeQualityPixelated = "pixelated";
constexpr const char* kSRGBImageBitmapColorSpaceConversion = "srgb";
constexpr const char* kLinearRGBImageBitmapColorSpaceConversion = "linear-rgb";
constexpr const char* kP3ImageBitmapColorSpaceConversion = "p3";
constexpr const char* kRec2020ImageBitmapColorSpaceConversion = "rec2020";

namespace {

// The following two functions are helpers used in cropImage
static inline IntRect NormalizeRect(const IntRect& rect) {
  return IntRect(std::min(rect.X(), rect.MaxX()),
                 std::min(rect.Y(), rect.MaxY()),
                 std::max(rect.Width(), -rect.Width()),
                 std::max(rect.Height(), -rect.Height()));
}

ImageBitmap::ParsedOptions ParseOptions(const ImageBitmapOptions& options,
                                        Optional<IntRect> crop_rect,
                                        IntSize source_size) {
  ImageBitmap::ParsedOptions parsed_options;
  if (options.imageOrientation() == kImageOrientationFlipY) {
    parsed_options.flip_y = true;
  } else {
    parsed_options.flip_y = false;
    DCHECK(options.imageOrientation() == kImageBitmapOptionNone);
  }
  if (options.premultiplyAlpha() == kImageBitmapOptionNone) {
    parsed_options.premultiply_alpha = false;
  } else {
    parsed_options.premultiply_alpha = true;
    DCHECK(options.premultiplyAlpha() == kImageBitmapOptionDefault ||
           options.premultiplyAlpha() == kImageBitmapOptionPremultiply);
  }

  if (options.colorSpaceConversion() != kImageBitmapOptionNone) {
    if (CanvasColorParams::ColorCorrectRenderingInAnyColorSpace()) {
      if (options.colorSpaceConversion() == kImageBitmapOptionDefault ||
          options.colorSpaceConversion() ==
              kSRGBImageBitmapColorSpaceConversion) {
        parsed_options.color_params.SetCanvasColorSpace(kSRGBCanvasColorSpace);
      } else if (options.colorSpaceConversion() ==
                 kLinearRGBImageBitmapColorSpaceConversion) {
        parsed_options.color_params.SetCanvasColorSpace(kSRGBCanvasColorSpace);
        parsed_options.color_params.SetCanvasPixelFormat(kF16CanvasPixelFormat);
      } else if (options.colorSpaceConversion() ==
                 kP3ImageBitmapColorSpaceConversion) {
        parsed_options.color_params.SetCanvasColorSpace(kP3CanvasColorSpace);
        parsed_options.color_params.SetCanvasPixelFormat(kF16CanvasPixelFormat);
      } else if (options.colorSpaceConversion() ==
                 kRec2020ImageBitmapColorSpaceConversion) {
        parsed_options.color_params.SetCanvasColorSpace(
            kRec2020CanvasColorSpace);
        parsed_options.color_params.SetCanvasPixelFormat(kF16CanvasPixelFormat);
      } else {
        NOTREACHED()
            << "Invalid ImageBitmap creation attribute colorSpaceConversion: "
            << options.colorSpaceConversion();
      }
    } else if (CanvasColorParams::ColorCorrectRenderingInSRGBOnly()) {
      DCHECK_EQ(options.colorSpaceConversion(), kImageBitmapOptionDefault);
      parsed_options.color_params.SetCanvasColorSpace(kSRGBCanvasColorSpace);
    } else {
      DCHECK_EQ(options.colorSpaceConversion(), kImageBitmapOptionDefault);
      parsed_options.color_params.SetCanvasColorSpace(kLegacyCanvasColorSpace);
    }
  }

  int source_width = source_size.Width();
  int source_height = source_size.Height();
  if (!crop_rect) {
    parsed_options.crop_rect = IntRect(0, 0, source_width, source_height);
  } else {
    parsed_options.crop_rect = NormalizeRect(*crop_rect);
  }
  if (!options.hasResizeWidth() && !options.hasResizeHeight()) {
    parsed_options.resize_width = parsed_options.crop_rect.Width();
    parsed_options.resize_height = parsed_options.crop_rect.Height();
  } else if (options.hasResizeWidth() && options.hasResizeHeight()) {
    parsed_options.resize_width = options.resizeWidth();
    parsed_options.resize_height = options.resizeHeight();
  } else if (options.hasResizeWidth() && !options.hasResizeHeight()) {
    parsed_options.resize_width = options.resizeWidth();
    parsed_options.resize_height = ceil(
        static_cast<float>(options.resizeWidth()) /
        parsed_options.crop_rect.Width() * parsed_options.crop_rect.Height());
  } else {
    parsed_options.resize_height = options.resizeHeight();
    parsed_options.resize_width = ceil(
        static_cast<float>(options.resizeHeight()) /
        parsed_options.crop_rect.Height() * parsed_options.crop_rect.Width());
  }
  if (static_cast<int>(parsed_options.resize_width) ==
          parsed_options.crop_rect.Width() &&
      static_cast<int>(parsed_options.resize_height) ==
          parsed_options.crop_rect.Height()) {
    parsed_options.should_scale_input = false;
    return parsed_options;
  }
  parsed_options.should_scale_input = true;

  if (options.resizeQuality() == kImageBitmapOptionResizeQualityHigh)
    parsed_options.resize_quality = kHigh_SkFilterQuality;
  else if (options.resizeQuality() == kImageBitmapOptionResizeQualityMedium)
    parsed_options.resize_quality = kMedium_SkFilterQuality;
  else if (options.resizeQuality() == kImageBitmapOptionResizeQualityPixelated)
    parsed_options.resize_quality = kNone_SkFilterQuality;
  else
    parsed_options.resize_quality = kLow_SkFilterQuality;
  return parsed_options;
}

// The function dstBufferSizeHasOverflow() is being called at the beginning of
// each ImageBitmap() constructor, which makes sure that doing
// width * height * bytesPerPixel will never overflow unsigned.
bool DstBufferSizeHasOverflow(const ImageBitmap::ParsedOptions& options) {
  CheckedNumeric<unsigned> total_bytes = options.crop_rect.Width();
  total_bytes *= options.crop_rect.Height();
  total_bytes *=
      SkColorTypeBytesPerPixel(options.color_params.GetSkColorType());
  if (!total_bytes.IsValid())
    return true;

  if (!options.should_scale_input)
    return false;
  total_bytes = options.resize_width;
  total_bytes *= options.resize_height;
  total_bytes *=
      SkColorTypeBytesPerPixel(options.color_params.GetSkColorType());
  if (!total_bytes.IsValid())
    return true;

  return false;
}

SkImageInfo GetSkImageInfo(const RefPtr<StaticBitmapImage>& image) {
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  SkColorType color_type = kN32_SkColorType;
  if (skia_image->colorSpace() && skia_image->colorSpace()->gammaIsLinear())
    color_type = kRGBA_F16_SkColorType;
  return SkImageInfo::Make(skia_image->width(), skia_image->height(),
                           color_type, skia_image->alphaType(),
                           skia_image->refColorSpace());
}

RefPtr<Uint8Array> CopyImageData(const RefPtr<StaticBitmapImage>& input,
                                 const SkImageInfo& info) {
  unsigned width = static_cast<unsigned>(input->width());
  RefPtr<ArrayBuffer> dst_buffer =
      ArrayBuffer::CreateOrNull(width * input->height(), info.bytesPerPixel());
  if (!dst_buffer)
    return nullptr;
  unsigned byte_length = dst_buffer->ByteLength();
  RefPtr<Uint8Array> dst_pixels =
      Uint8Array::Create(std::move(dst_buffer), 0, byte_length);
  input->PaintImageForCurrentFrame().GetSkImage()->readPixels(
      info, dst_pixels->Data(), width * info.bytesPerPixel(), 0, 0);
  return dst_pixels;
}

RefPtr<Uint8Array> CopyImageData(const RefPtr<StaticBitmapImage>& input) {
  SkImageInfo info = GetSkImageInfo(input);
  return CopyImageData(std::move(input), info);
}

void freePixels(const void*, void* pixels) {
  static_cast<Uint8Array*>(pixels)->Deref();
}

RefPtr<StaticBitmapImage> NewImageFromRaster(
    const SkImageInfo& info,
    RefPtr<Uint8Array>&& image_pixels) {
  unsigned image_row_bytes = info.width() * info.bytesPerPixel();
  SkPixmap pixmap(info, image_pixels->Data(), image_row_bytes);
  return StaticBitmapImage::Create(
      SkImage::MakeFromRaster(pixmap, freePixels, image_pixels.LeakRef()));
}

RefPtr<StaticBitmapImage> FlipImageVertically(RefPtr<StaticBitmapImage> input) {
  RefPtr<Uint8Array> image_pixels = CopyImageData(input);
  if (!image_pixels)
    return nullptr;
  SkImageInfo info = GetSkImageInfo(input.Get());
  unsigned image_row_bytes = info.width() * info.bytesPerPixel();
  for (int i = 0; i < info.height() / 2; i++) {
    unsigned top_first_element = i * image_row_bytes;
    unsigned top_last_element = (i + 1) * image_row_bytes;
    unsigned bottom_first_element = (info.height() - 1 - i) * image_row_bytes;
    std::swap_ranges(image_pixels->Data() + top_first_element,
                     image_pixels->Data() + top_last_element,
                     image_pixels->Data() + bottom_first_element);
  }
  return NewImageFromRaster(info, std::move(image_pixels));
}

RefPtr<StaticBitmapImage> GetImageWithAlphaDisposition(
    RefPtr<StaticBitmapImage>&& image,
    AlphaDisposition alpha_disposition) {
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  if (alpha_disposition == kDontPremultiplyAlpha)
    alpha_type = kUnpremul_SkAlphaType;
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (skia_image->alphaType() == alpha_type)
    return image;

  SkImageInfo info = GetSkImageInfo(image.Get());
  info = info.makeAlphaType(alpha_type);
  RefPtr<Uint8Array> dst_pixels = CopyImageData(image, info);
  if (!dst_pixels)
    return nullptr;
  return NewImageFromRaster(info, std::move(dst_pixels));
}

RefPtr<StaticBitmapImage> ScaleImage(RefPtr<StaticBitmapImage>&& image,
                                     unsigned resize_width,
                                     unsigned resize_height,
                                     SkFilterQuality resize_quality) {
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  SkAlphaType original_alpha = skia_image->alphaType();
  auto pm_image = skia_image;
  auto pm_info = GetSkImageInfo(image);
  if (original_alpha == kUnpremul_SkAlphaType) {
    pm_info = pm_info.makeAlphaType(kPremul_SkAlphaType);
    SkPixmap pm_pixmap;
    if (!skia_image->peekPixels(&pm_pixmap))
      return nullptr;
    pm_pixmap.reset(pm_info, pm_pixmap.addr(), pm_pixmap.rowBytes());
    pm_image = SkImage::MakeFromRaster(pm_pixmap, nullptr, nullptr);
  }

  RefPtr<ArrayBuffer> dst_buffer = ArrayBuffer::CreateOrNull(
      resize_width * resize_height, pm_info.bytesPerPixel());
  if (!dst_buffer)
    return nullptr;
  RefPtr<Uint8Array> resized_pixels =
      Uint8Array::Create(dst_buffer, 0, dst_buffer->ByteLength());
  SkImageInfo pm_resized_info = pm_info.makeWH(resize_width, resize_height);
  SkPixmap pm_resized_pixmap(pm_resized_info, resized_pixels->Data(),
                             resize_width * pm_info.bytesPerPixel());

  // When the original image is unpremul and is tagged as premul, calling
  // SkImage::scalePixels() with parameter kHigh_SkFilterQuality clamps RGB
  // components to alpha (bugs.chromium.org/p/skia/issues/detail?id=6855).
  // As a workaround, we downgrade kHigh_SkFilterQuality to kMedium_ for
  // unpremul images. This does not affect the quality of down-scaling,
  // but should be fixed to get the highest upscaling quality.
  // Bug: 744636.
  if (original_alpha == kUnpremul_SkAlphaType &&
      resize_quality == kHigh_SkFilterQuality)
    resize_quality = kMedium_SkFilterQuality;

  // Only scale in premul
  pm_image->scalePixels(pm_resized_pixmap, resize_quality);
  sk_sp<SkImage> resized_image;
  if (original_alpha == kPremul_SkAlphaType) {
    resized_image = SkImage::MakeFromRaster(pm_resized_pixmap, freePixels,
                                            resized_pixels.LeakRef());
  } else {
    SkPixmap upm_resized_pixmap(
        pm_resized_info.makeAlphaType(kUnpremul_SkAlphaType),
        resized_pixels->Data(), resize_width * pm_info.bytesPerPixel());
    resized_image = SkImage::MakeFromRaster(upm_resized_pixmap, freePixels,
                                            resized_pixels.LeakRef());
  }
  return StaticBitmapImage::Create(resized_image,
                                   image->ContextProviderWrapper());
}

RefPtr<StaticBitmapImage> ApplyColorSpaceConversion(
    RefPtr<StaticBitmapImage>&& image,
    ImageBitmap::ParsedOptions& options) {
  if (!CanvasColorParams::ColorCorrectRenderingEnabled())
    return image;

  // TODO(zakerinasab): crbug.com/754713
  // If image does not have any color space info and we must color convert to
  // SRGB, we should tag the image as SRGB. Since we cannot use SkImage::
  // makeColorSpace() on SkImages with no color space to get the image in SRGB,
  // we have to do this using a slow code path (reading the pixels and creating
  // a new SRGB SkImage). This is inefficient and also converts GPU-backed
  // images to CPU-backed. For now, we ignore this and let the images be in
  // null color space. This must be okay if color management is only supported
  // for SRGB. This might be okay for converting null color space to other
  // color spaces. Please see crbug.com/754713 for more details.

  if (!CanvasColorParams::ColorCorrectRenderingInAnyColorSpace())
    return image;

  // Color correct the image. This code path uses SkImage::makeColorSpace(). If
  // the color space of the source image is nullptr, it will be assumed in SRGB.
  return image->ConvertToColorSpace(options.color_params.GetSkColorSpace(),
                                    SkTransferFunctionBehavior::kRespect);
}

RefPtr<StaticBitmapImage> MakeBlankImage(
    const ImageBitmap::ParsedOptions& parsed_options) {
  SkImageInfo info = SkImageInfo::Make(
      parsed_options.crop_rect.Width(), parsed_options.crop_rect.Height(),
      parsed_options.color_params.GetSkColorType(), kPremul_SkAlphaType,
      parsed_options.color_params.GetSkColorSpaceForSkSurfaces());
  if (parsed_options.should_scale_input) {
    info =
        info.makeWH(parsed_options.resize_width, parsed_options.resize_height);
  }
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

}  // namespace

sk_sp<SkImage> ImageBitmap::GetSkImageFromDecoder(
    std::unique_ptr<ImageDecoder> decoder) {
  if (!decoder->FrameCount())
    return nullptr;
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  if (!frame || frame->GetStatus() != ImageFrame::kFrameComplete)
    return nullptr;
  DCHECK(!frame->Bitmap().isNull() && !frame->Bitmap().empty());
  return frame->FinalizePixelsAndGetImage();
}

bool ImageBitmap::IsResizeOptionValid(const ImageBitmapOptions& options,
                                      ExceptionState& exception_state) {
  if ((options.hasResizeWidth() && options.resizeWidth() == 0) ||
      (options.hasResizeHeight() && options.resizeHeight() == 0)) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "The resizeWidth or/and resizeHeight is equal to 0.");
    return false;
  }
  return true;
}

bool ImageBitmap::IsSourceSizeValid(int source_width,
                                    int source_height,
                                    ExceptionState& exception_state) {
  if (!source_width || !source_height) {
    exception_state.ThrowDOMException(
        kIndexSizeError, String::Format("The source %s provided is 0.",
                                        source_width ? "height" : "width"));
    return false;
  }
  return true;
}

static RefPtr<StaticBitmapImage> CropImageAndApplyColorSpaceConversion(
    RefPtr<Image>&& image,
    ImageBitmap::ParsedOptions& parsed_options,
    ColorBehavior color_behavior = ColorBehavior::TransformToGlobalTarget()) {
  DCHECK(image);
  IntRect img_rect(IntPoint(), IntSize(image->width(), image->height()));
  const IntRect src_rect = Intersection(img_rect, parsed_options.crop_rect);

  // If cropRect doesn't intersect the source image, return a transparent black
  // image.
  if (src_rect.IsEmpty())
    return MakeBlankImage(parsed_options);

  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  // Attempt to get raw unpremultiplied image data, executed only when
  // skia_image is premultiplied.
  if ((((!image->IsSVGImage() && !skia_image->isOpaque()) || !skia_image) &&
       image->Data() && skia_image->alphaType() == kPremul_SkAlphaType) ||
      color_behavior.IsIgnore()) {
    std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
        image->Data(), true,
        parsed_options.premultiply_alpha ? ImageDecoder::kAlphaPremultiplied
                                         : ImageDecoder::kAlphaNotPremultiplied,
        color_behavior));
    if (!decoder)
      return nullptr;
    skia_image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
    if (!skia_image)
      return nullptr;
  }

  if (src_rect != img_rect)
    skia_image = skia_image->makeSubset(src_rect);

  RefPtr<StaticBitmapImage> result =
      StaticBitmapImage::Create(skia_image, image->ContextProviderWrapper());

  // flip if needed
  if (parsed_options.flip_y)
    result = FlipImageVertically(std::move(result));

  // premultiply / unpremultiply if needed
  result = GetImageWithAlphaDisposition(std::move(result),
                                        parsed_options.premultiply_alpha
                                            ? kPremultiplyAlpha
                                            : kDontPremultiplyAlpha);

  // resize if needed
  if (parsed_options.should_scale_input) {
    result =
        ScaleImage(std::move(result), parsed_options.resize_width,
                   parsed_options.resize_height, parsed_options.resize_quality);
  }

  // color correct the image
  result = ApplyColorSpaceConversion(std::move(result), parsed_options);
  return result;
}

ImageBitmap::ImageBitmap(ImageElementBase* image,
                         Optional<IntRect> crop_rect,
                         Document* document,
                         const ImageBitmapOptions& options) {
  RefPtr<Image> input = image->CachedImage()->GetImage();
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ = CropImageAndApplyColorSpaceConversion(
      std::move(input), parsed_options,
      options.colorSpaceConversion() == kImageBitmapOptionNone
          ? ColorBehavior::Ignore()
          : ColorBehavior::TransformToGlobalTarget());
  if (!image_)
    return;

  // In the case where the source image is lazy-decoded, m_image may not be in
  // a decoded state, we trigger it here.
  sk_sp<SkImage> sk_image = image_->PaintImageForCurrentFrame().GetSkImage();
  SkPixmap pixmap;
  if (!sk_image->isTextureBacked() && !sk_image->peekPixels(&pixmap)) {
    sk_sp<SkColorSpace> dst_color_space = nullptr;
    SkColorType dst_color_type = kN32_SkColorType;
    if (CanvasColorParams::ColorCorrectRenderingInAnyColorSpace() ||
        parsed_options.color_params.ColorCorrectNoColorSpaceToSRGB()) {
      dst_color_space = parsed_options.color_params.GetSkColorSpace();
      dst_color_type = parsed_options.color_params.GetSkColorType();
    }
    SkImageInfo image_info =
        SkImageInfo::Make(sk_image->width(), sk_image->height(), dst_color_type,
                          kPremul_SkAlphaType, dst_color_space);
    sk_sp<SkSurface> surface = SkSurface::MakeRaster(image_info);
    surface->getCanvas()->drawImage(sk_image, 0, 0);
    image_ = ApplyColorSpaceConversion(
        StaticBitmapImage::Create(surface->makeImageSnapshot(),
                                  input->ContextProviderWrapper()),
        parsed_options);
  }
  if (!image_)
    return;

  image_->SetOriginClean(
      !image->WouldTaintOrigin(document->GetSecurityOrigin()));
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video,
                         Optional<IntRect> crop_rect,
                         Document* document,
                         const ImageBitmapOptions& options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, video->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  std::unique_ptr<ImageBuffer> buffer =
      ImageBuffer::Create(IntSize(video->videoWidth(), video->videoHeight()),
                          kNonOpaque, kDoNotInitializeImagePixels);
  if (!buffer)
    return;

  video->PaintCurrentFrame(
      buffer->Canvas(),
      IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())),
      nullptr);
  RefPtr<StaticBitmapImage> input = buffer->NewImageSnapshot();
  image_ = CropImageAndApplyColorSpaceConversion(input, parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(
      !video->WouldTaintOrigin(document->GetSecurityOrigin()));
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas,
                         Optional<IntRect> crop_rect,
                         const ImageBitmapOptions& options) {
  DCHECK(canvas->IsPaintable());
  SourceImageStatus status;
  RefPtr<Image> image_input = canvas->GetSourceImageForCanvas(
      &status, kPreferAcceleration, kSnapshotReasonCreateImageBitmap,
      FloatSize());
  if (status != kNormalSourceImageStatus)
    return;
  DCHECK(image_input->IsStaticBitmapImage());
  RefPtr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(image_input.Get());

  ParsedOptions parsed_options = ParseOptions(
      options, crop_rect, IntSize(input->width(), input->height()));
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(canvas->OriginClean());
}

ImageBitmap::ImageBitmap(OffscreenCanvas* offscreen_canvas,
                         Optional<IntRect> crop_rect,
                         const ImageBitmapOptions& options) {
  SourceImageStatus status;
  RefPtr<Image> raw_input = offscreen_canvas->GetSourceImageForCanvas(
      &status, kPreferNoAcceleration, kSnapshotReasonCreateImageBitmap,
      FloatSize(offscreen_canvas->Size()));
  DCHECK(raw_input->IsStaticBitmapImage());
  RefPtr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(raw_input.Get());
  raw_input.Clear();

  if (status != kNormalSourceImageStatus)
    return;

  ParsedOptions parsed_options = ParseOptions(
      options, crop_rect, IntSize(input->width(), input->height()));
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;
  image_->SetOriginClean(offscreen_canvas->OriginClean());
}

ImageBitmap::ImageBitmap(const void* pixel_data,
                         uint32_t width,
                         uint32_t height,
                         bool is_image_bitmap_premultiplied,
                         bool is_image_bitmap_origin_clean,
                         const CanvasColorParams& color_params) {
  SkImageInfo info =
      SkImageInfo::Make(width, height, color_params.GetSkColorType(),
                        is_image_bitmap_premultiplied ? kPremul_SkAlphaType
                                                      : kUnpremul_SkAlphaType,
                        color_params.GetSkColorSpaceForSkSurfaces());
  SkPixmap pixmap(info, pixel_data, info.bytesPerPixel() * width);
  image_ = StaticBitmapImage::Create(SkImage::MakeRasterCopy(pixmap));
  if (!image_)
    return;
  image_->SetOriginClean(is_image_bitmap_origin_clean);
}

static void SwizzleImageDataIfNeeded(ImageData* data) {
  if (!data || (kN32_SkColorType != kBGRA_8888_SkColorType) ||
      (data->GetCanvasColorParams().GetSkColorSpaceForSkSurfaces() &&
       data->GetCanvasColorParams()
           .GetSkColorSpaceForSkSurfaces()
           ->gammaIsLinear()))
    return;
  SkSwapRB(static_cast<uint32_t*>(data->BufferBase()->Data()),
           static_cast<uint32_t*>(data->BufferBase()->Data()),
           data->Size().Height() * data->Size().Width());
}

ImageBitmap::ImageBitmap(ImageData* data,
                         Optional<IntRect> crop_rect,
                         const ImageBitmapOptions& options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, data->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  IntRect data_src_rect = IntRect(IntPoint(), data->Size());
  IntRect src_rect = crop_rect
                         ? Intersection(parsed_options.crop_rect, data_src_rect)
                         : data_src_rect;

  // If cropRect doesn't intersect the source image, return a transparent black
  // image.
  if (src_rect.IsEmpty()) {
    image_ = MakeBlankImage(parsed_options);
    return;
  }

  // crop/flip the input if needed
  bool crop_or_flip = (src_rect != data_src_rect) || parsed_options.flip_y;
  ImageData* cropped_data = data;
  if (crop_or_flip)
    cropped_data = data->CropRect(src_rect, parsed_options.flip_y);

  SwizzleImageDataIfNeeded(cropped_data);

  int byte_length = cropped_data->BufferBase()->ByteLength();
  RefPtr<ArrayBuffer> array_buffer = ArrayBuffer::CreateOrNull(byte_length, 1);
  if (!array_buffer)
    return;
  RefPtr<Uint8Array> image_pixels =
      Uint8Array::Create(std::move(array_buffer), 0, byte_length);
  memcpy(image_pixels->Data(), cropped_data->BufferBase()->Data(), byte_length);

  SkImageInfo info = SkImageInfo::Make(
      cropped_data->width(), cropped_data->height(),
      cropped_data->GetCanvasColorParams().GetSkColorType(),
      kUnpremul_SkAlphaType,
      cropped_data->GetCanvasColorParams().GetSkColorSpaceForSkSurfaces());

  // If we are in color correct rendering mode but we only color correct to
  // SRGB, we don't do any color conversion when transferring the pixels from
  // ImageData to ImageBitmap to avoid double gamma correction. We tag the
  // image with SRGB color space later in ApplyColorSpaceConversion().
  if (CanvasColorParams::ColorCorrectRenderingInSRGBOnly())
    info = info.makeColorSpace(nullptr);
  image_ = NewImageFromRaster(info, std::move(image_pixels));

  // swizzle back
  SwizzleImageDataIfNeeded(cropped_data);

  // premultiply if needed
  if (parsed_options.premultiply_alpha)
    image_ = GetImageWithAlphaDisposition(std::move(image_), kPremultiplyAlpha);

  // color correct if needed
  image_ = ApplyColorSpaceConversion(std::move(image_), parsed_options);

  // resize if needed
  if (parsed_options.should_scale_input) {
    image_ =
        ScaleImage(std::move(image_), parsed_options.resize_width,
                   parsed_options.resize_height, parsed_options.resize_quality);
  }
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap,
                         Optional<IntRect> crop_rect,
                         const ImageBitmapOptions& options) {
  RefPtr<StaticBitmapImage> input = bitmap->BitmapImage();
  if (!input)
    return;
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, input->Size());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);

  if (!image_)
    return;
  image_->SetOriginClean(bitmap->OriginClean());
}

ImageBitmap::ImageBitmap(RefPtr<StaticBitmapImage> image,
                         Optional<IntRect> crop_rect,
                         const ImageBitmapOptions& options) {
  bool origin_clean = image->OriginClean();
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->Size());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(image), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(origin_clean);
}

ImageBitmap::ImageBitmap(RefPtr<StaticBitmapImage> image) {
  image_ = std::move(image);
}

RefPtr<StaticBitmapImage> ImageBitmap::Transfer() {
  DCHECK(!IsNeutered());
  is_neutered_ = true;
  image_->Transfer();
  return std::move(image_);
}

ImageBitmap::~ImageBitmap() {}

ImageBitmap* ImageBitmap::Create(ImageElementBase* image,
                                 Optional<IntRect> crop_rect,
                                 Document* document,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(image, crop_rect, document, options);
}

ImageBitmap* ImageBitmap::Create(HTMLVideoElement* video,
                                 Optional<IntRect> crop_rect,
                                 Document* document,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(video, crop_rect, document, options);
}

ImageBitmap* ImageBitmap::Create(HTMLCanvasElement* canvas,
                                 Optional<IntRect> crop_rect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(canvas, crop_rect, options);
}

ImageBitmap* ImageBitmap::Create(OffscreenCanvas* offscreen_canvas,
                                 Optional<IntRect> crop_rect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(offscreen_canvas, crop_rect, options);
}

ImageBitmap* ImageBitmap::Create(ImageData* data,
                                 Optional<IntRect> crop_rect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(data, crop_rect, options);
}

ImageBitmap* ImageBitmap::Create(ImageBitmap* bitmap,
                                 Optional<IntRect> crop_rect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(bitmap, crop_rect, options);
}

ImageBitmap* ImageBitmap::Create(RefPtr<StaticBitmapImage> image,
                                 Optional<IntRect> crop_rect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(std::move(image), crop_rect, options);
}

ImageBitmap* ImageBitmap::Create(RefPtr<StaticBitmapImage> image) {
  return new ImageBitmap(std::move(image));
}

ImageBitmap* ImageBitmap::Create(const void* pixel_data,
                                 uint32_t width,
                                 uint32_t height,
                                 bool is_image_bitmap_premultiplied,
                                 bool is_image_bitmap_origin_clean,
                                 const CanvasColorParams& color_params) {
  return new ImageBitmap(pixel_data, width, height,
                         is_image_bitmap_premultiplied,
                         is_image_bitmap_origin_clean, color_params);
}

void ImageBitmap::ResolvePromiseOnOriginalThread(
    ScriptPromiseResolver* resolver,
    sk_sp<SkImage> skia_image,
    bool origin_clean,
    std::unique_ptr<ParsedOptions> parsed_options) {
  RefPtr<StaticBitmapImage> image =
      StaticBitmapImage::Create(std::move(skia_image));
  DCHECK(IsMainThread());
  if (!parsed_options->premultiply_alpha) {
    image =
        GetImageWithAlphaDisposition(std::move(image), kDontPremultiplyAlpha);
  }
  if (!image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  image = ApplyColorSpaceConversion(std::move(image), *(parsed_options.get()));
  if (!image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  ImageBitmap* bitmap = new ImageBitmap(image);
  if (bitmap && bitmap->BitmapImage())
    bitmap->BitmapImage()->SetOriginClean(origin_clean);
  if (bitmap && bitmap->BitmapImage()) {
    resolver->Resolve(bitmap);
  } else {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
  }
}

void ImageBitmap::RasterizeImageOnBackgroundThread(
    ScriptPromiseResolver* resolver,
    sk_sp<PaintRecord> paint_record,
    const IntRect& dst_rect,
    bool origin_clean,
    std::unique_ptr<ParsedOptions> parsed_options) {
  DCHECK(!IsMainThread());
  SkImageInfo info = SkImageInfo::Make(
      dst_rect.Width(), dst_rect.Height(),
      parsed_options->color_params.GetSkColorType(), kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  paint_record->Playback(surface->getCanvas());
  sk_sp<SkImage> skia_image = surface->makeImageSnapshot();
  RefPtr<WebTaskRunner> task_runner =
      Platform::Current()->MainThread()->GetWebTaskRunner();
  task_runner->PostTask(
      BLINK_FROM_HERE, CrossThreadBind(&ResolvePromiseOnOriginalThread,
                                       WrapCrossThreadPersistent(resolver),
                                       std::move(skia_image), origin_clean,
                                       WTF::Passed(std::move(parsed_options))));
}

ScriptPromise ImageBitmap::CreateAsync(ImageElementBase* image,
                                       Optional<IntRect> crop_rect,
                                       Document* document,
                                       ScriptState* script_state,
                                       const ImageBitmapOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  RefPtr<Image> input = image->CachedImage()->GetImage();
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options)) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return promise;
  }

  IntRect input_rect(IntPoint(), input->Size());
  const IntRect src_rect = Intersection(input_rect, parsed_options.crop_rect);

  // In the case when |crop_rect| doesn't intersect the source image, we return
  // a transparent black image, respecting the color_params but ignoring
  // poremultiply_alpha.
  if (src_rect.IsEmpty()) {
    ImageBitmap* bitmap = new ImageBitmap(MakeBlankImage(parsed_options));
    if (bitmap && bitmap->BitmapImage()) {
      bitmap->BitmapImage()->SetOriginClean(
          !image->WouldTaintOrigin(document->GetSecurityOrigin()));
    }
    if (bitmap && bitmap->BitmapImage()) {
      resolver->Resolve(bitmap);
    } else {
      resolver->Reject(
          ScriptValue(resolver->GetScriptState(),
                      v8::Null(resolver->GetScriptState()->GetIsolate())));
    }
    return promise;
  }

  IntRect draw_src_rect(parsed_options.crop_rect);
  IntRect draw_dst_rect(0, 0, parsed_options.resize_width,
                        parsed_options.resize_height);
  sk_sp<PaintRecord> paint_record =
      input->PaintRecordForContainer(NullURL(), input->Size(), draw_src_rect,
                                     draw_dst_rect, parsed_options.flip_y);
  std::unique_ptr<ParsedOptions> passed_parsed_options =
      WTF::MakeUnique<ParsedOptions>(parsed_options);
  BackgroundTaskRunner::PostOnBackgroundThread(
      BLINK_FROM_HERE,
      CrossThreadBind(&RasterizeImageOnBackgroundThread,
                      WrapCrossThreadPersistent(resolver),
                      std::move(paint_record), draw_dst_rect,
                      !image->WouldTaintOrigin(document->GetSecurityOrigin()),
                      WTF::Passed(std::move(passed_parsed_options))));

  return promise;
}

void ImageBitmap::close() {
  if (!image_ || is_neutered_)
    return;
  image_.Clear();
  is_neutered_ = true;
}

// static
ImageBitmap* ImageBitmap::Take(ScriptPromiseResolver*, sk_sp<SkImage> image) {
  return ImageBitmap::Create(StaticBitmapImage::Create(std::move(image)));
}

CanvasColorParams ImageBitmap::GetCanvasColorParams() {
  return CanvasColorParams(GetSkImageInfo(image_));
}

RefPtr<Uint8Array> ImageBitmap::CopyBitmapData(AlphaDisposition alpha_op,
                                               DataColorFormat format) {
  SkImageInfo info = GetSkImageInfo(image_);
  auto color_type = info.colorType();
  if (color_type == kN32_SkColorType && format == kRGBAColorType)
    color_type = kRGBA_8888_SkColorType;
  info =
      SkImageInfo::Make(width(), height(), color_type,
                        (alpha_op == kPremultiplyAlpha) ? kPremul_SkAlphaType
                                                        : kUnpremul_SkAlphaType,
                        info.refColorSpace());
  return CopyImageData(image_, info);
}

RefPtr<Uint8Array> ImageBitmap::CopyBitmapData() {
  return CopyImageData(image_);
}

unsigned long ImageBitmap::width() const {
  if (!image_)
    return 0;
  DCHECK_GT(image_->width(), 0);
  return image_->width();
}

unsigned long ImageBitmap::height() const {
  if (!image_)
    return 0;
  DCHECK_GT(image_->height(), 0);
  return image_->height();
}

bool ImageBitmap::IsAccelerated() const {
  return image_ && (image_->IsTextureBacked() || image_->HasMailbox());
}

IntSize ImageBitmap::Size() const {
  if (!image_)
    return IntSize();
  DCHECK_GT(image_->width(), 0);
  DCHECK_GT(image_->height(), 0);
  return IntSize(image_->width(), image_->height());
}

ScriptPromise ImageBitmap::CreateImageBitmap(ScriptState* script_state,
                                             EventTarget& event_target,
                                             Optional<IntRect> crop_rect,
                                             const ImageBitmapOptions& options,
                                             ExceptionState& exception_state) {
  if ((crop_rect && !IsSourceSizeValid(crop_rect->Width(), crop_rect->Height(),
                                       exception_state)) ||
      !IsSourceSizeValid(width(), height(), exception_state))
    return ScriptPromise();
  if (!IsResizeOptionValid(options, exception_state))
    return ScriptPromise();
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, Create(this, crop_rect, options));
}

RefPtr<Image> ImageBitmap::GetSourceImageForCanvas(SourceImageStatus* status,
                                                   AccelerationHint,
                                                   SnapshotReason,
                                                   const FloatSize&) {
  *status = kNormalSourceImageStatus;
  if (!image_)
    return nullptr;
  if (image_->IsPremultiplied())
    return image_;
  // Skia does not support drawing unpremul SkImage on SkCanvas.
  // Premultiply and return.
  return GetImageWithAlphaDisposition(std::move(image_), kPremultiplyAlpha);
}

void ImageBitmap::AdjustDrawRects(FloatRect* src_rect,
                                  FloatRect* dst_rect) const {}

FloatSize ImageBitmap::ElementSize(const FloatSize&) const {
  return FloatSize(width(), height());
}

DEFINE_TRACE(ImageBitmap) {}

}  // namespace blink
