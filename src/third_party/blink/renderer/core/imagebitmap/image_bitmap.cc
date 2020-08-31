// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"

namespace blink {

constexpr const char* kImageOrientationFlipY = "flipY";
constexpr const char* kImageBitmapOptionNone = "none";
constexpr const char* kImageBitmapOptionDefault = "default";
constexpr const char* kImageBitmapPixelFormatUint8Name = "uint8";
constexpr const char* kImageBitmapOptionPremultiply = "premultiply";
constexpr const char* kImageBitmapOptionResizeQualityHigh = "high";
constexpr const char* kImageBitmapOptionResizeQualityMedium = "medium";
constexpr const char* kImageBitmapOptionResizeQualityPixelated = "pixelated";
constexpr const char* kPreserveImageBitmapColorSpaceConversion = "preserve";
constexpr const char* kSRGBImageBitmapColorSpaceConversion = "srgb";
constexpr const char* kLinearRGBImageBitmapColorSpaceConversion = "linear-rgb";
constexpr const char* kP3ImageBitmapColorSpaceConversion = "p3";
constexpr const char* kRec2020ImageBitmapColorSpaceConversion = "rec2020";

namespace {

// The following two functions are helpers used in cropImage
static inline IntRect NormalizeRect(const IntRect& rect) {
  int x = rect.X();
  int y = rect.Y();
  int width = rect.Width();
  int height = rect.Height();
  if (width < 0) {
    x = base::ClampAdd(x, width);
    width = -width;
  }
  if (height < 0) {
    y = base::ClampAdd(y, height);
    height = -height;
  }
  return IntRect(x, y, width, height);
}

ImageBitmap::ParsedOptions ParseOptions(const ImageBitmapOptions* options,
                                        base::Optional<IntRect> crop_rect,
                                        IntSize source_size) {
  ImageBitmap::ParsedOptions parsed_options;
  if (options->imageOrientation() == kImageOrientationFlipY) {
    parsed_options.flip_y = true;
  } else {
    parsed_options.flip_y = false;
    DCHECK(options->imageOrientation() == kImageBitmapOptionNone);
  }

  if (options->imagePixelFormat() == kImageBitmapPixelFormatUint8Name)
    parsed_options.pixel_format = kImageBitmapPixelFormat_Uint8;

  if (options->premultiplyAlpha() == kImageBitmapOptionNone) {
    parsed_options.premultiply_alpha = false;
  } else {
    parsed_options.premultiply_alpha = true;
    DCHECK(options->premultiplyAlpha() == kImageBitmapOptionDefault ||
           options->premultiplyAlpha() == kImageBitmapOptionPremultiply);
  }

  parsed_options.has_color_space_conversion =
      (options->colorSpaceConversion() != kImageBitmapOptionNone);
  parsed_options.preserve_source_color_space =
      (options->colorSpaceConversion() ==
       kPreserveImageBitmapColorSpaceConversion);
  parsed_options.color_params.SetCanvasColorSpace(CanvasColorSpace::kSRGB);
  if (options->colorSpaceConversion() != kSRGBImageBitmapColorSpaceConversion &&
      options->colorSpaceConversion() !=
          kPreserveImageBitmapColorSpaceConversion &&
      options->colorSpaceConversion() != kImageBitmapOptionNone &&
      options->colorSpaceConversion() != kImageBitmapOptionDefault) {
    parsed_options.color_params.SetCanvasPixelFormat(CanvasPixelFormat::kF16);
    if (options->colorSpaceConversion() ==
        kLinearRGBImageBitmapColorSpaceConversion) {
      parsed_options.color_params.SetCanvasColorSpace(
          CanvasColorSpace::kLinearRGB);
    } else if (options->colorSpaceConversion() ==
               kP3ImageBitmapColorSpaceConversion) {
      parsed_options.color_params.SetCanvasColorSpace(CanvasColorSpace::kP3);
    } else if (options->colorSpaceConversion() ==
               kRec2020ImageBitmapColorSpaceConversion) {
      parsed_options.color_params.SetCanvasColorSpace(
          CanvasColorSpace::kRec2020);
    } else {
      NOTREACHED()
          << "Invalid ImageBitmap creation attribute colorSpaceConversion: "
          << options->colorSpaceConversion();
    }
  }

  int source_width = source_size.Width();
  int source_height = source_size.Height();
  if (!crop_rect) {
    parsed_options.crop_rect = IntRect(0, 0, source_width, source_height);
  } else {
    parsed_options.crop_rect = NormalizeRect(*crop_rect);
  }
  if (!options->hasResizeWidth() && !options->hasResizeHeight()) {
    parsed_options.resize_width = parsed_options.crop_rect.Width();
    parsed_options.resize_height = parsed_options.crop_rect.Height();
  } else if (options->hasResizeWidth() && options->hasResizeHeight()) {
    parsed_options.resize_width = options->resizeWidth();
    parsed_options.resize_height = options->resizeHeight();
  } else if (options->hasResizeWidth() && !options->hasResizeHeight()) {
    parsed_options.resize_width = options->resizeWidth();
    parsed_options.resize_height = ceil(
        static_cast<float>(options->resizeWidth()) /
        parsed_options.crop_rect.Width() * parsed_options.crop_rect.Height());
  } else {
    parsed_options.resize_height = options->resizeHeight();
    parsed_options.resize_width = ceil(
        static_cast<float>(options->resizeHeight()) /
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

  if (options->resizeQuality() == kImageBitmapOptionResizeQualityHigh)
    parsed_options.resize_quality = kHigh_SkFilterQuality;
  else if (options->resizeQuality() == kImageBitmapOptionResizeQualityMedium)
    parsed_options.resize_quality = kMedium_SkFilterQuality;
  else if (options->resizeQuality() == kImageBitmapOptionResizeQualityPixelated)
    parsed_options.resize_quality = kNone_SkFilterQuality;
  else
    parsed_options.resize_quality = kLow_SkFilterQuality;
  return parsed_options;
}

// The function dstBufferSizeHasOverflow() is being called at the beginning of
// each ImageBitmap() constructor, which makes sure that doing
// width * height * bytesPerPixel will never overflow unsigned.
bool DstBufferSizeHasOverflow(const ImageBitmap::ParsedOptions& options) {
  base::CheckedNumeric<unsigned> total_bytes = options.crop_rect.Width();
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

SkImageInfo GetSkImageInfo(const scoped_refptr<Image>& input) {
  auto image = input->PaintImageForCurrentFrame().GetSkImage();
  return SkImageInfo::Make(image->width(), image->height(), image->colorType(),
                           image->alphaType(), image->refColorSpace());
}

// This function results in a readback due to using SkImage::readPixels().
// Returns transparent black pixels if the input SkImageInfo.bounds() does
// not intersect with the input image boundaries.
Vector<uint8_t> CopyImageData(const scoped_refptr<StaticBitmapImage>& input,
                              const SkImageInfo& info,
                              const unsigned x = 0,
                              const unsigned y = 0) {
  if (info.isEmpty())
    return {};
  sk_sp<SkImage> sk_image = input->PaintImageForCurrentFrame().GetSkImage();
  if (sk_image->bounds().isEmpty())
    return {};

  wtf_size_t byte_length =
      base::checked_cast<wtf_size_t>(info.computeMinByteSize());
  Vector<uint8_t> dst_buffer(byte_length);

  bool read_pixels_successful =
      sk_image->readPixels(info, dst_buffer.data(), info.minRowBytes(), x, y);
  DCHECK(read_pixels_successful);
  if (!read_pixels_successful)
    return {};
  return dst_buffer;
}

Vector<uint8_t> CopyImageData(const scoped_refptr<StaticBitmapImage>& input) {
  SkImageInfo info = GetSkImageInfo(input);
  return CopyImageData(std::move(input), info);
}

static inline bool ShouldAvoidPremul(
    const ImageBitmap::ParsedOptions& options) {
  return options.source_is_unpremul && !options.premultiply_alpha;
}

std::unique_ptr<CanvasResourceProvider> CreateProvider(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    const SkImageInfo& info,
    const scoped_refptr<StaticBitmapImage>& source_image,
    bool fallback_to_software) {
  IntSize size(info.width(), info.height());
  CanvasColorParams color_params(info);

  if (context_provider) {
    uint32_t usage_flags =
        context_provider->ContextProvider()
            ->SharedImageInterface()
            ->UsageForMailbox(source_image->GetMailboxHolder().mailbox);
    auto resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
        size, context_provider, kLow_SkFilterQuality, color_params,
        source_image->IsOriginTopLeft(),
        CanvasResourceProvider::RasterMode::kGPU, usage_flags);
    if (resource_provider)
      return resource_provider;

    if (!fallback_to_software)
      return nullptr;
  }

  return CanvasResourceProvider::CreateBitmapProvider(
      size, kLow_SkFilterQuality, color_params);
}

scoped_refptr<StaticBitmapImage> FlipImageVertically(
    scoped_refptr<StaticBitmapImage> input,
    const ImageBitmap::ParsedOptions& parsed_options) {
  sk_sp<SkImage> image = input->PaintImageForCurrentFrame().GetSkImage();
  if (!image)
    return nullptr;

  if (ShouldAvoidPremul(parsed_options)) {
    // Unpremul code path results in a GPU readback if |input| is texture
    // backed since CopyImageData() uses  SkImage::readPixels() to extract the
    // pixels from SkImage.
    SkImageInfo info = GetSkImageInfo(input);
    if (info.isEmpty())
      return nullptr;

    sk_sp<SkImage> sk_image = input->PaintImageForCurrentFrame().GetSkImage();
    if (sk_image->bounds().isEmpty())
      return nullptr;

    sk_sp<SkData> image_pixels = TryAllocateSkData(info.computeMinByteSize());
    if (!image_pixels)
      return nullptr;

    uint8_t* writable_pixels =
        static_cast<uint8_t*>(image_pixels->writable_data());
    size_t image_row_bytes = static_cast<size_t>(info.minRowBytes64());
    bool read_successful =
        sk_image->readPixels(info, writable_pixels, image_row_bytes, 0, 0);
    DCHECK(read_successful);

    for (int i = 0; i < info.height() / 2; i++) {
      size_t top_first_element = i * image_row_bytes;
      size_t top_last_element = (i + 1) * image_row_bytes;
      size_t bottom_first_element = (info.height() - 1 - i) * image_row_bytes;
      std::swap_ranges(&writable_pixels[top_first_element],
                       &writable_pixels[top_last_element],
                       &writable_pixels[bottom_first_element]);
    }
    return StaticBitmapImage::Create(std::move(image_pixels), info,
                                     input->CurrentFrameOrientation());
  }

  // Since we are allowed to premul the input image if needed, we can use Skia
  // to flip the image by drawing it on a surface. If the image is premul, we
  // can use both accelerated and software surfaces. If the image is unpremul,
  // we have to use software surfaces.
  bool use_accelerated =
      image->isTextureBacked() && image->alphaType() == kPremul_SkAlphaType;
  auto resource_provider = CreateProvider(
      use_accelerated ? input->ContextProviderWrapper() : nullptr,
      GetSkImageInfo(input), input, true /* fallback_to_software */);
  if (!resource_provider)
    return nullptr;

  auto* canvas = resource_provider->Canvas();
  canvas->scale(1, -1);
  canvas->translate(0, -input->height());
  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas->drawImage(input->PaintImageForCurrentFrame(), 0, 0, &paint);
  return resource_provider->Snapshot(input->CurrentFrameOrientation());
}

scoped_refptr<StaticBitmapImage> GetImageWithAlphaDisposition(
    scoped_refptr<StaticBitmapImage>&& image,
    AlphaDisposition alpha_disposition) {
  DCHECK(alpha_disposition != kDontChangeAlpha);
  if (alpha_disposition == kDontChangeAlpha)
    return std::move(image);
  SkAlphaType alpha_type = (alpha_disposition == kPremultiplyAlpha)
                               ? kPremul_SkAlphaType
                               : kUnpremul_SkAlphaType;
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (skia_image->alphaType() == alpha_type)
    return std::move(image);

  SkImageInfo info = GetSkImageInfo(image.get()).makeAlphaType(alpha_type);
  // To premul, draw the unpremul image on a surface to avoid GPU read back if
  // image is texture backed.
  if (alpha_type == kPremul_SkAlphaType) {
    auto resource_provider = CreateProvider(
        image->IsTextureBacked() ? image->ContextProviderWrapper() : nullptr,
        info, image, true /* fallback_to_software */);
    if (!resource_provider)
      return nullptr;

    cc::PaintFlags paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    resource_provider->Canvas()->drawImage(image->PaintImageForCurrentFrame(),
                                           0, 0, &paint);
    return resource_provider->Snapshot(image->CurrentFrameOrientation());
  }

  // To unpremul, read back the pixels.

  if (skia_image->bounds().isEmpty())
    return nullptr;

  sk_sp<SkData> dst_pixels = TryAllocateSkData(info.computeMinByteSize());
  if (!dst_pixels)
    return nullptr;

  uint8_t* writable_pixels = static_cast<uint8_t*>(dst_pixels->writable_data());
  size_t image_row_bytes = static_cast<size_t>(info.minRowBytes64());
  bool read_successful =
      skia_image->readPixels(info, writable_pixels, image_row_bytes, 0, 0);
  DCHECK(read_successful);
  return StaticBitmapImage::Create(std::move(dst_pixels), info,
                                   image->CurrentFrameOrientation());
}

scoped_refptr<StaticBitmapImage> ScaleImage(
    scoped_refptr<StaticBitmapImage>&& image,
    const ImageBitmap::ParsedOptions& parsed_options) {
  auto sk_image = image->PaintImageForCurrentFrame().GetSkImage();
  auto image_info = GetSkImageInfo(image).makeWH(parsed_options.resize_width,
                                                 parsed_options.resize_height);

  // Try to avoid GPU read back by drawing accelerated premul image on an
  // accelerated surface.
  if (!ShouldAvoidPremul(parsed_options) && image->IsTextureBacked() &&
      sk_image->alphaType() == kPremul_SkAlphaType) {
    auto resource_provider =
        CreateProvider(image->ContextProviderWrapper(), image_info, image,
                       false /* fallback_to_software */);
    if (resource_provider) {
      cc::PaintFlags paint;
      paint.setFilterQuality(parsed_options.resize_quality);
      resource_provider->Canvas()->drawImageRect(
          image->PaintImageForCurrentFrame(),
          SkRect::MakeWH(sk_image->width(), sk_image->height()),
          SkRect::MakeWH(parsed_options.resize_width,
                         parsed_options.resize_height),
          &paint, cc::PaintCanvas::kStrict_SrcRectConstraint);
      return resource_provider->Snapshot(image->CurrentFrameOrientation());
    }
  }

  // Avoid sRGB transfer function by setting the color space to nullptr.
  if (image_info.colorSpace()->isSRGB())
    image_info = image_info.makeColorSpace(nullptr);

  sk_sp<SkData> image_pixels =
      TryAllocateSkData(image_info.computeMinByteSize());
  if (!image_pixels) {
    return nullptr;
  }

  SkPixmap resized_pixmap(image_info, image_pixels->data(),
                          image_info.minRowBytes());
  sk_image->scalePixels(resized_pixmap, parsed_options.resize_quality);
  // Tag the resized Pixmap with the correct color space.
  resized_pixmap.setColorSpace(GetSkImageInfo(image).refColorSpace());

  auto resized_sk_image =
      SkImage::MakeRasterData(resized_pixmap.info(), std::move(image_pixels),
                              resized_pixmap.rowBytes());
  if (!resized_sk_image)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(
      resized_sk_image, image->CurrentFrameOrientation());
}

scoped_refptr<StaticBitmapImage> ApplyColorSpaceConversion(
    scoped_refptr<StaticBitmapImage>&& image,
    ImageBitmap::ParsedOptions& options) {
  sk_sp<SkColorSpace> color_space = options.color_params.GetSkColorSpace();
  SkColorType color_type =
      image->IsTextureBacked() ? kRGBA_8888_SkColorType : kN32_SkColorType;
  sk_sp<SkImage> sk_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (!sk_image)
    return nullptr;

  // If we should preserve color precision, don't lose it in color space
  // conversion.
  if (options.pixel_format == kImageBitmapPixelFormat_Default &&
      (sk_image->colorType() == kRGBA_F16_SkColorType ||
       (sk_image->colorSpace() && sk_image->colorSpace()->gammaIsLinear()) ||
       (color_space && color_space->gammaIsLinear()))) {
    color_type = kRGBA_F16_SkColorType;
  }
  return image->ConvertToColorSpace(color_space, color_type);
}

scoped_refptr<StaticBitmapImage> MakeBlankImage(
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
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

scoped_refptr<StaticBitmapImage> GetImageWithPixelFormat(
    scoped_refptr<StaticBitmapImage>&& image,
    ImageBitmapPixelFormat pixel_format) {
  if (pixel_format == kImageBitmapPixelFormat_Default)
    return std::move(image);
  // If the the image is not half float backed, default and uint8 image bitmap
  // pixel formats result in the same uint8 backed image bitmap.
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (skia_image->colorType() != kRGBA_F16_SkColorType)
    return std::move(image);

  SkPixmap pixmap;
  skia_image->peekPixels(&pixmap);
  SkImageInfo target_info = pixmap.info().makeColorType(kN32_SkColorType);
  SkBitmap target_bitmap;
  target_bitmap.allocPixels(target_info);
  pixmap.readPixels(target_bitmap.pixmap(), 0, 0);
  return UnacceleratedStaticBitmapImage::Create(
      SkImage::MakeFromBitmap(target_bitmap));
}

static scoped_refptr<StaticBitmapImage> CropImageAndApplyColorSpaceConversion(
    scoped_refptr<StaticBitmapImage>&& image,
    ImageBitmap::ParsedOptions& parsed_options) {
  DCHECK(image);
  DCHECK(!image->Data());

  IntRect img_rect(IntPoint(), IntSize(image->width(), image->height()));
  const IntRect src_rect = Intersection(img_rect, parsed_options.crop_rect);

  // If cropRect doesn't intersect the source image, return a transparent black
  // image.
  if (src_rect.IsEmpty())
    return MakeBlankImage(parsed_options);

  scoped_refptr<StaticBitmapImage> result = image;
  if (src_rect != img_rect) {
    auto paint_image = result->PaintImageForCurrentFrame();
    if (result->IsTextureBacked()) {
      auto image_info = paint_image.GetSkImage()->imageInfo().makeWH(
          src_rect.Width(), src_rect.Height());
      auto resource_provider =
          CreateProvider(image->ContextProviderWrapper(), image_info, result,
                         true /* fallback_to_software*/);
      if (!resource_provider)
        return nullptr;
      cc::PaintFlags paint;
      resource_provider->Canvas()->drawImageRect(
          paint_image,
          SkRect::MakeXYWH(src_rect.X(), src_rect.Y(), src_rect.Width(),
                           src_rect.Height()),
          SkRect::MakeWH(src_rect.Width(), src_rect.Height()), &paint,
          cc::PaintCanvas::kStrict_SrcRectConstraint);
      result = resource_provider->Snapshot(image->CurrentFrameOrientation());
    } else {
      result = UnacceleratedStaticBitmapImage::Create(
          cc::PaintImageBuilder::WithCopy(std::move(paint_image))
              .make_subset(src_rect)
              .TakePaintImage(),
          image->CurrentFrameOrientation());
    }
  }

  // down-scaling has higher priority than other tasks, up-scaling has lower.
  bool down_scaling =
      parsed_options.should_scale_input &&
      (parsed_options.resize_width * parsed_options.resize_height <
       result->Size().Area());
  bool up_scaling = parsed_options.should_scale_input && !down_scaling;

  // resize if down-scaling
  if (down_scaling) {
    result = ScaleImage(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  // flip if needed
  if (parsed_options.flip_y) {
    result = FlipImageVertically(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  // color convert if needed
  if (parsed_options.has_color_space_conversion &&
      !parsed_options.preserve_source_color_space) {
    result = ApplyColorSpaceConversion(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  // premultiply / unpremultiply if needed
  result = GetImageWithAlphaDisposition(std::move(result),
                                        parsed_options.premultiply_alpha
                                            ? kPremultiplyAlpha
                                            : kUnpremultiplyAlpha);

  // convert pixel format if needed
  result =
      GetImageWithPixelFormat(std::move(result), parsed_options.pixel_format);

  // resize if up-scaling
  if (up_scaling) {
    result = ScaleImage(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  return result;
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

ImageBitmap::ImageBitmap(ImageElementBase* image,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  scoped_refptr<Image> input = image->CachedImage()->GetImage();
  DCHECK(!input->IsTextureBacked());

  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->BitmapSourceSize());
  parsed_options.source_is_unpremul =
      (input->PaintImageForCurrentFrame().GetSkImage()->alphaType() ==
       kUnpremul_SkAlphaType);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  cc::PaintImage paint_image = input->PaintImageForCurrentFrame();
  if (!paint_image)
    return;

  DCHECK(!paint_image.IsTextureBacked());
  if (input->IsBitmapImage()) {
    // A BitmapImage indicates that this is a coded backed image.
    if (!input->Data())
      return;

    DCHECK(paint_image.IsLazyGenerated());
    const bool data_complete = true;
    std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
        input->Data(), data_complete,
        parsed_options.premultiply_alpha ? ImageDecoder::kAlphaPremultiplied
                                         : ImageDecoder::kAlphaNotPremultiplied,
        ImageDecoder::kDefaultBitDepth,
        parsed_options.has_color_space_conversion ? ColorBehavior::Tag()
                                                  : ColorBehavior::Ignore(),
        ImageDecoder::OverrideAllowDecodeToYuv::kDeny));
    auto skia_image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
    if (!skia_image)
      return;

    paint_image = PaintImageBuilder::WithDefault()
                      .set_id(paint_image.stable_id())
                      .set_image(std::move(skia_image),
                                 paint_image.GetContentIdForFrame(0u))
                      .TakePaintImage();
  } else if (paint_image.IsLazyGenerated()) {
    // Other Image types can still produce lazy generated images (for example
    // SVGs).
    SkBitmap bitmap;
    SkImageInfo image_info = GetSkImageInfo(input);
    bitmap.allocPixels(image_info, image_info.minRowBytes());
    if (!paint_image.GetSkImage()->readPixels(bitmap.pixmap(), 0, 0))
      return;

    paint_image = PaintImageBuilder::WithDefault()
                      .set_id(paint_image.stable_id())
                      .set_image(SkImage::MakeFromBitmap(bitmap),
                                 paint_image.GetContentIdForFrame(0u))
                      .TakePaintImage();
  }

  auto static_input = UnacceleratedStaticBitmapImage::Create(
      std::move(paint_image), input->CurrentFrameOrientation());
  image_ = CropImageAndApplyColorSpaceConversion(std::move(static_input),
                                                 parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(!image->WouldTaintOrigin());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, video->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  // TODO(fserb): this shouldn't be software?
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateBitmapProvider(
          IntSize(video->videoWidth(), video->videoHeight()),
          kLow_SkFilterQuality,
          CanvasColorParams());  // TODO: set color space here to avoid clamping

  if (!resource_provider)
    return;

  video->PaintCurrentFrame(
      resource_provider->Canvas(),
      IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())),
      nullptr);
  scoped_refptr<StaticBitmapImage> input = resource_provider->Snapshot();
  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(!video->WouldTaintOrigin());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  SourceImageStatus status;
  scoped_refptr<Image> image_input = canvas->GetSourceImageForCanvas(
      &status, kPreferAcceleration, FloatSize());
  if (status != kNormalSourceImageStatus)
    return;
  DCHECK(IsA<StaticBitmapImage>(image_input.get()));
  scoped_refptr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(image_input.get());

  ParsedOptions parsed_options = ParseOptions(
      options, crop_rect, IntSize(input->width(), input->height()));
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(canvas->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(OffscreenCanvas* offscreen_canvas,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  SourceImageStatus status;
  scoped_refptr<Image> raw_input = offscreen_canvas->GetSourceImageForCanvas(
      &status, kPreferNoAcceleration, FloatSize(offscreen_canvas->Size()));
  DCHECK(IsA<StaticBitmapImage>(raw_input.get()));
  scoped_refptr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(raw_input.get());
  raw_input = nullptr;

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
  UpdateImageBitmapMemoryUsage();
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
  sk_sp<SkImage> raster_copy = SkImage::MakeRasterCopy(pixmap);
  if (!raster_copy)
    return;
  image_ = UnacceleratedStaticBitmapImage::Create(std::move(raster_copy));
  if (!image_)
    return;
  image_->SetOriginClean(is_image_bitmap_origin_clean);
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(ImageData* data,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, data->BitmapSourceSize());
  // ImageData is always unpremul.
  parsed_options.source_is_unpremul = true;
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

  // Copy / color convert the pixels
  size_t image_pixels_size;
  if (!base::CheckMul(parsed_options.color_params.BytesPerPixel(),
                      static_cast<unsigned>(src_rect.Width()),
                      static_cast<unsigned>(src_rect.Height()))
           .AssignIfValid(&image_pixels_size)) {
    return;
  }
  sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
  if (!image_pixels)
    return;
  if (!data->ImageDataInCanvasColorSettings(
          parsed_options.color_params.ColorSpace(),
          parsed_options.color_params.PixelFormat(),
          static_cast<unsigned char*>(image_pixels->writable_data()),
          kN32ColorType, &src_rect,
          parsed_options.premultiply_alpha ? kPremultiplyAlpha
                                           : kUnpremultiplyAlpha))
    return;

  // Create Image object
  SkImageInfo info = SkImageInfo::Make(
      src_rect.Width(), src_rect.Height(),
      parsed_options.color_params.GetSkColorType(),
      parsed_options.premultiply_alpha ? kPremul_SkAlphaType
                                       : kUnpremul_SkAlphaType,
      parsed_options.color_params.GetSkColorSpaceForSkSurfaces());
  image_ = StaticBitmapImage::Create(std::move(image_pixels), info);
  if (!image_)
    return;

  // down-scaling has higher priority than other tasks, up-scaling has lower.
  bool down_scaling =
      parsed_options.should_scale_input &&
      (parsed_options.resize_width * parsed_options.resize_height <
       image_->Size().Area());
  bool up_scaling = parsed_options.should_scale_input && !down_scaling;

  // resize if down-scaling
  if (down_scaling)
    image_ = ScaleImage(std::move(image_), parsed_options);
  if (!image_)
    return;

  // flip if needed
  if (parsed_options.flip_y)
    image_ = FlipImageVertically(std::move(image_), parsed_options);
  if (!image_)
    return;

  // resize if up-scaling
  if (up_scaling)
    image_ = ScaleImage(std::move(image_), parsed_options);

  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  scoped_refptr<StaticBitmapImage> input = bitmap->BitmapImage();
  if (!input)
    return;
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, input->Size());
  parsed_options.source_is_unpremul =
      (input->PaintImageForCurrentFrame().GetSkImage()->alphaType() ==
       kUnpremul_SkAlphaType);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(bitmap->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(scoped_refptr<StaticBitmapImage> image,
                         base::Optional<IntRect> crop_rect,
                         const ImageBitmapOptions* options) {
  bool origin_clean = image->OriginClean();
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->Size());
  parsed_options.source_is_unpremul =
      (image->PaintImageForCurrentFrame().GetSkImage()->alphaType() ==
       kUnpremul_SkAlphaType);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(image), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(origin_clean);
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(scoped_refptr<StaticBitmapImage> image) {
  image_ = std::move(image);
  UpdateImageBitmapMemoryUsage();
}

scoped_refptr<StaticBitmapImage> ImageBitmap::Transfer() {
  DCHECK(!IsNeutered());
  is_neutered_ = true;
  image_->Transfer();
  return std::move(image_);
}

void ImageBitmap::UpdateImageBitmapMemoryUsage() {
  // TODO(fserb): We should be calling GetCanvasColorParams().BytesPerPixel()
  // but this is breaking some tests due to the repaint of the image.
  int bytes_per_pixel = 4;

  int32_t new_memory_usage = 0;

  if (image_) {
    base::CheckedNumeric<int32_t> memory_usage_checked = bytes_per_pixel;
    memory_usage_checked *= image_->width();
    memory_usage_checked *= image_->height();
    new_memory_usage = memory_usage_checked.ValueOrDefault(
        std::numeric_limits<int32_t>::max());
  }

  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      new_memory_usage - memory_usage_);
  memory_usage_ = new_memory_usage;
}

ImageBitmap::~ImageBitmap() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -memory_usage_);
}

void ImageBitmap::ResolvePromiseOnOriginalThread(
    ScriptPromiseResolver* resolver,
    bool origin_clean,
    std::unique_ptr<ParsedOptions> parsed_options,
    sk_sp<SkImage> skia_image,
    const ImageOrientationEnum orientation) {
  if (!skia_image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState()->GetIsolate(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  scoped_refptr<StaticBitmapImage> image =
      UnacceleratedStaticBitmapImage::Create(std::move(skia_image),
                                             orientation);
  DCHECK(IsMainThread());
  if (!parsed_options->premultiply_alpha) {
    image = GetImageWithAlphaDisposition(std::move(image), kUnpremultiplyAlpha);
  }
  if (!image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState()->GetIsolate(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  image = ApplyColorSpaceConversion(std::move(image), *(parsed_options.get()));
  if (!image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState()->GetIsolate(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  ImageBitmap* bitmap = MakeGarbageCollected<ImageBitmap>(image);
  bitmap->BitmapImage()->SetOriginClean(origin_clean);
  resolver->Resolve(bitmap);
}

void ImageBitmap::RasterizeImageOnBackgroundThread(
    sk_sp<PaintRecord> paint_record,
    const IntRect& dst_rect,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    WTF::CrossThreadOnceFunction<void(sk_sp<SkImage>,
                                      const ImageOrientationEnum)> callback) {
  DCHECK(!IsMainThread());
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(dst_rect.Width(), dst_rect.Height());
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  sk_sp<SkImage> skia_image;
  if (surface) {
    paint_record->Playback(surface->getCanvas());
    skia_image = surface->makeImageSnapshot();
  }
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(std::move(callback), std::move(skia_image),
                          kDefaultImageOrientation));
}

ScriptPromise ImageBitmap::CreateAsync(ImageElementBase* image,
                                       base::Optional<IntRect> crop_rect,
                                       ScriptState* script_state,
                                       const ImageBitmapOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  scoped_refptr<Image> input = image->CachedImage()->GetImage();
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The ImageBitmap could not be allocated."));
    return promise;
  }

  IntRect input_rect(IntPoint(), input->Size());
  const IntRect src_rect = Intersection(input_rect, parsed_options.crop_rect);

  // In the case when |crop_rect| doesn't intersect the source image, we return
  // a transparent black image, respecting the color_params but ignoring
  // poremultiply_alpha.
  if (src_rect.IsEmpty()) {
    ImageBitmap* bitmap =
        MakeGarbageCollected<ImageBitmap>(MakeBlankImage(parsed_options));
    if (bitmap->BitmapImage()) {
      bitmap->BitmapImage()->SetOriginClean(!image->WouldTaintOrigin());
      resolver->Resolve(bitmap);
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The ImageBitmap could not be allocated."));
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
      std::make_unique<ParsedOptions>(parsed_options);
  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          &RasterizeImageOnBackgroundThread, std::move(paint_record),
          draw_dst_rect, Thread::MainThread()->GetTaskRunner(),
          CrossThreadBindOnce(&ResolvePromiseOnOriginalThread,
                              WrapCrossThreadPersistent(resolver),
                              !image->WouldTaintOrigin(),
                              WTF::Passed(std::move(passed_parsed_options)))));
  return promise;
}

void ImageBitmap::close() {
  if (!image_ || is_neutered_)
    return;
  image_ = nullptr;
  is_neutered_ = true;
}

// static
ImageBitmap* ImageBitmap::Take(ScriptPromiseResolver*, sk_sp<SkImage> image) {
  return MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(std::move(image)));
}

CanvasColorParams ImageBitmap::GetCanvasColorParams() {
  return CanvasColorParams(GetSkImageInfo(image_));
}

Vector<uint8_t> ImageBitmap::CopyBitmapData(AlphaDisposition alpha_op,
                                            DataU8ColorType u8_color_type) {
  DCHECK(alpha_op != kDontChangeAlpha);
  SkImageInfo info = GetSkImageInfo(image_);
  auto color_type = info.colorType();
  if (color_type == kN32_SkColorType && u8_color_type == kRGBAColorType)
    color_type = kRGBA_8888_SkColorType;
  info =
      SkImageInfo::Make(width(), height(), color_type,
                        (alpha_op == kPremultiplyAlpha) ? kPremul_SkAlphaType
                                                        : kUnpremul_SkAlphaType,
                        info.refColorSpace());
  return CopyImageData(image_, info);
}

Vector<uint8_t> ImageBitmap::CopyBitmapData() {
  return CopyImageData(image_);
}

unsigned ImageBitmap::width() const {
  if (!image_)
    return 0;
  IntSize size = image_->SizeRespectingOrientation();
  DCHECK_GT(size.Width(), 0);
  return size.Width();
}

unsigned ImageBitmap::height() const {
  if (!image_)
    return 0;
  IntSize size = image_->SizeRespectingOrientation();
  DCHECK_GT(size.Height(), 0);
  return size.Height();
}

bool ImageBitmap::IsAccelerated() const {
  return image_ && image_->IsTextureBacked();
}

IntSize ImageBitmap::Size() const {
  if (!image_)
    return IntSize();
  DCHECK_GT(image_->width(), 0);
  DCHECK_GT(image_->height(), 0);
  return IntSize(image_->width(), image_->height());
}

ScriptPromise ImageBitmap::CreateImageBitmap(ScriptState* script_state,
                                             base::Optional<IntRect> crop_rect,
                                             const ImageBitmapOptions* options,
                                             ExceptionState& exception_state) {
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      exception_state);
}

scoped_refptr<Image> ImageBitmap::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
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

FloatSize ImageBitmap::ElementSize(
    const FloatSize&,
    const RespectImageOrientationEnum respect_orientation) const {
  return FloatSize(image_->Size(respect_orientation));
}

}  // namespace blink
