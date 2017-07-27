// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/UnacceleratedStaticBitmapImage.h"

#include "third_party/skia/include/core/SkImage.h"

namespace blink {

PassRefPtr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(sk_sp<SkImage> image) {
  DCHECK(!image->isTextureBacked());
  return AdoptRef(new UnacceleratedStaticBitmapImage(std::move(image)));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    sk_sp<SkImage> image)
    : image_(std::move(image)) {
  DCHECK(image_);
}

UnacceleratedStaticBitmapImage::~UnacceleratedStaticBitmapImage() {}

IntSize UnacceleratedStaticBitmapImage::Size() const {
  return IntSize(image_->width(), image_->height());
}

bool UnacceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque(MetadataMode) {
  return image_->isOpaque();
}

void UnacceleratedStaticBitmapImage::Draw(PaintCanvas* canvas,
                                          const PaintFlags& flags,
                                          const FloatRect& dst_rect,
                                          const FloatRect& src_rect,
                                          RespectImageOrientationEnum,
                                          ImageClampingMode clamp_mode) {
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect, clamp_mode,
                                PaintImageForCurrentFrame());
}

sk_sp<SkImage> UnacceleratedStaticBitmapImage::ImageForCurrentFrame() {
  return image_;
}

}  // namespace blink
