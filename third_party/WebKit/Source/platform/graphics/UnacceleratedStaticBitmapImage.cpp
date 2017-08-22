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
    sk_sp<SkImage> image) {
  DCHECK(!image->isLazyGenerated());

  PaintImageBuilder builder;
  InitPaintImageBuilder(builder);
  builder.set_image(std::move(image));
  paint_image_ = builder.TakePaintImage();
}

PassRefPtr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(PaintImage image) {
  return AdoptRef(new UnacceleratedStaticBitmapImage(std::move(image)));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(PaintImage image)
    : paint_image_(std::move(image)) {
  DCHECK(paint_image_);
}

UnacceleratedStaticBitmapImage::~UnacceleratedStaticBitmapImage() {}

IntSize UnacceleratedStaticBitmapImage::Size() const {
  return IntSize(paint_image_.width(), paint_image_.height());
}

bool UnacceleratedStaticBitmapImage::IsPremultiplied() const {
  return paint_image_.GetSkImage()->alphaType() ==
         SkAlphaType::kPremul_SkAlphaType;
}

bool UnacceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque(MetadataMode) {
  return paint_image_.GetSkImage()->isOpaque();
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

PaintImage UnacceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  return paint_image_;
}

}  // namespace blink
