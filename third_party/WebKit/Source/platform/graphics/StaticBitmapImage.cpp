// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/UnacceleratedStaticBitmapImage.h"
#include "platform/graphics/paint/PaintImage.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

RefPtr<StaticBitmapImage> StaticBitmapImage::Create(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper) {
  if (image->isTextureBacked()) {
    CHECK(context_provider_wrapper);
    return AcceleratedStaticBitmapImage::CreateFromSkImage(
        image, std::move(context_provider_wrapper));
  }
  return UnacceleratedStaticBitmapImage::Create(image);
}

RefPtr<StaticBitmapImage> StaticBitmapImage::Create(PaintImage image) {
  DCHECK(!image.GetSkImage()->isTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(image));
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

RefPtr<StaticBitmapImage> StaticBitmapImage::ConvertToColorSpace(
    sk_sp<SkColorSpace> target,
    SkTransferFunctionBehavior premulBehavior) {
  sk_sp<SkImage> skia_image = PaintImageForCurrentFrame().GetSkImage();
  sk_sp<SkImage> converted_skia_image =
      skia_image->makeColorSpace(target, premulBehavior);

  if (skia_image == converted_skia_image)
    return this;

  return StaticBitmapImage::Create(converted_skia_image,
                                   ContextProviderWrapper());
}

}  // namespace blink
