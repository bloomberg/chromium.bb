// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/UnacceleratedStaticBitmapImage.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

PassRefPtr<StaticBitmapImage> StaticBitmapImage::Create(sk_sp<SkImage> image) {
  if (!image)
    return nullptr;
  if (image->isTextureBacked())
    return AcceleratedStaticBitmapImage::CreateFromSharedContextImage(
        std::move(image));
  return UnacceleratedStaticBitmapImage::Create(std::move(image));
}

void StaticBitmapImage::DrawHelper(PaintCanvas* canvas,
                                   const PaintFlags& flags,
                                   const FloatRect& dst_rect,
                                   const FloatRect& src_rect,
                                   ImageClampingMode clamp_mode,
                                   sk_sp<SkImage> image) {
  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::Make(image->bounds()));

  if (dst_rect.IsEmpty() || adjusted_src_rect.IsEmpty())
    return;  // Nothing to draw.

  canvas->drawImageRect(std::move(image), adjusted_src_rect, dst_rect, &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));
}

}  // namespace blink
