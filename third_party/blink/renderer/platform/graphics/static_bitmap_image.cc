// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

#include "base/numerics/checked_math.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer_contents.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "v8/include/v8.h"

namespace blink {

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (!image)
    return nullptr;
  if (image->isTextureBacked()) {
    CHECK(context_provider_wrapper);
    return AcceleratedStaticBitmapImage::CreateFromSkImage(
        image, std::move(context_provider_wrapper));
  }
  return UnacceleratedStaticBitmapImage::Create(image);
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(PaintImage image) {
  DCHECK(!image.GetSkImage()->isTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(image));
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    sk_sp<SkData> data,
    const SkImageInfo& info) {
  return Create(
      SkImage::MakeRasterData(info, std::move(data), info.minRowBytes()));
}

void StaticBitmapImage::DrawHelper(cc::PaintCanvas* canvas,
                                   const PaintFlags& flags,
                                   const FloatRect& dst_rect,
                                   const FloatRect& src_rect,
                                   ImageClampingMode clamp_mode,
                                   const PaintImage& image) {
  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::MakeWH(image.width(), image.height()));

  if (dst_rect.IsEmpty() || adjusted_src_rect.IsEmpty())
    return;  // Nothing to draw.

  canvas->drawImageRect(image, adjusted_src_rect, dst_rect, &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::ConvertToColorSpace(
    sk_sp<SkColorSpace> color_space,
    SkColorType color_type) {
  DCHECK(color_space);
  sk_sp<SkImage> skia_image = PaintImageForCurrentFrame().GetSkImage();

  // If we don't need to change the color type, use SkImage::makeColorSpace()
  if (skia_image->colorType() == color_type) {
    skia_image = skia_image->makeColorSpace(color_space);
  } else {
    skia_image =
        skia_image->makeColorTypeAndColorSpace(color_type, color_space);
  }

  return StaticBitmapImage::Create(skia_image, skia_image->isTextureBacked()
                                                   ? ContextProviderWrapper()
                                                   : nullptr);
}

bool StaticBitmapImage::ConvertToArrayBufferContents(
    scoped_refptr<StaticBitmapImage> src_image,
    WTF::ArrayBufferContents& dest_contents,
    const IntRect& rect,
    const CanvasColorParams& color_params,
    bool is_accelerated) {
  uint8_t bytes_per_pixel = color_params.BytesPerPixel();
  base::CheckedNumeric<int> data_size = bytes_per_pixel;
  data_size *= rect.Size().Area();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return false;

  int alloc_size_in_bytes = data_size.ValueOrDie();
  if (!src_image) {
    WTF::ArrayBufferContents result(alloc_size_in_bytes, 1,
                                    WTF::ArrayBufferContents::kNotShared,
                                    WTF::ArrayBufferContents::kZeroInitialize);

    // Check if the ArrayBuffer backing store was allocated correctly.
    if (result.DataLength() != static_cast<size_t>(alloc_size_in_bytes)) {
      return false;
    }
    result.Transfer(dest_contents);
    return true;
  }

  const bool may_have_stray_area =
      is_accelerated  // GPU readback may fail silently
      || rect.X() < 0 || rect.Y() < 0 ||
      rect.MaxX() > src_image->Size().Width() ||
      rect.MaxY() > src_image->Size().Height();
  WTF::ArrayBufferContents::InitializationPolicy initialization_policy =
      may_have_stray_area ? WTF::ArrayBufferContents::kZeroInitialize
                          : WTF::ArrayBufferContents::kDontInitialize;

  WTF::ArrayBufferContents result(alloc_size_in_bytes, 1,
                                  WTF::ArrayBufferContents::kNotShared,
                                  initialization_policy);
  // Check if the ArrayBuffer backing store was allocated correctly.
  if (result.DataLength() != static_cast<size_t>(alloc_size_in_bytes)) {
    return false;
  }

  SkColorType color_type =
      (color_params.GetSkColorType() == kRGBA_F16_SkColorType)
          ? kRGBA_F16_SkColorType
          : kRGBA_8888_SkColorType;
  SkImageInfo info = SkImageInfo::Make(
      rect.Width(), rect.Height(), color_type, kUnpremul_SkAlphaType,
      color_params.GetSkColorSpaceForSkSurfaces());
  sk_sp<SkImage> sk_image = src_image->PaintImageForCurrentFrame().GetSkImage();
  if (!sk_image)
    return false;
  bool read_pixels_successful = sk_image->readPixels(
      info, result.Data(), info.minRowBytes(), rect.X(), rect.Y());
  DCHECK(read_pixels_successful ||
         !sk_image->bounds().intersect(SkIRect::MakeXYWH(
             rect.X(), rect.Y(), info.width(), info.height())));
  result.Transfer(dest_contents);
  return true;
}

const gpu::SyncToken& StaticBitmapImage::GetSyncToken() const {
  static const gpu::SyncToken sync_token;
  return sync_token;
}

}  // namespace blink
