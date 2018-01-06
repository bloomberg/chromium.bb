// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/UnacceleratedStaticBitmapImage.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
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
    scoped_refptr<Uint8Array>&& image_pixels,
    const SkImageInfo& info) {
  SkPixmap pixmap(info, image_pixels->Data(), info.minRowBytes());

  Uint8Array* pixels = image_pixels.get();
  if (pixels) {
    pixels->AddRef();
    image_pixels = nullptr;
  }

  return Create(SkImage::MakeFromRaster(
      pixmap,
      [](const void*, void* p) { static_cast<Uint8Array*>(p)->Release(); },
      pixels));
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    WTF::ArrayBufferContents& contents,
    const SkImageInfo& info) {
  SkPixmap pixmap(info, contents.Data(), info.minRowBytes());
  return Create(SkImage::MakeFromRaster(pixmap, nullptr, nullptr));
}

void StaticBitmapImage::DrawHelper(PaintCanvas* canvas,
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
    sk_sp<SkColorSpace> target,
    SkTransferFunctionBehavior transfer_function_behavior) {
  sk_sp<SkImage> skia_image = PaintImageForCurrentFrame().GetSkImage();
  sk_sp<SkColorSpace> src_color_space = skia_image->refColorSpace();
  if (!src_color_space.get())
    src_color_space = SkColorSpace::MakeSRGB();
  sk_sp<SkColorSpace> dst_color_space = target;
  if (!dst_color_space.get())
    dst_color_space = SkColorSpace::MakeSRGB();
  if (SkColorSpace::Equals(src_color_space.get(), dst_color_space.get()))
    return this;

  sk_sp<SkImage> converted_skia_image =
      skia_image->makeColorSpace(dst_color_space, transfer_function_behavior);
  DCHECK(converted_skia_image.get());
  DCHECK(skia_image.get() != converted_skia_image.get());

  return StaticBitmapImage::Create(converted_skia_image,
                                   ContextProviderWrapper());
}

bool StaticBitmapImage::ConvertToArrayBufferContents(
    scoped_refptr<StaticBitmapImage> src_image,
    WTF::ArrayBufferContents& dest_contents,
    const IntRect& rect,
    const CanvasColorParams& color_params,
    bool is_accelerated) {
  uint8_t bytes_per_pixel = color_params.BytesPerPixel();
  CheckedNumeric<int> data_size = bytes_per_pixel;
  data_size *= rect.Size().Area();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return false;

  size_t alloc_size_in_bytes = rect.Size().Area() * bytes_per_pixel;
  if (!src_image) {
    auto data = WTF::ArrayBufferContents::CreateDataHandle(
        alloc_size_in_bytes, WTF::ArrayBufferContents::kZeroInitialize);
    if (!data)
      return false;
    WTF::ArrayBufferContents result(std::move(data),
                                    WTF::ArrayBufferContents::kNotShared);
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
  auto data = WTF::ArrayBufferContents::CreateDataHandle(alloc_size_in_bytes,
                                                         initialization_policy);
  if (!data)
    return false;
  WTF::ArrayBufferContents result(std::move(data),
                                  WTF::ArrayBufferContents::kNotShared);

  SkColorType color_type =
      (color_params.GetSkColorType() == kRGBA_F16_SkColorType)
          ? kRGBA_F16_SkColorType
          : kRGBA_8888_SkColorType;
  SkImageInfo info = SkImageInfo::Make(rect.Width(), rect.Height(), color_type,
                                       kUnpremul_SkAlphaType);
  sk_sp<SkImage> sk_image = src_image->PaintImageForCurrentFrame().GetSkImage();
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
