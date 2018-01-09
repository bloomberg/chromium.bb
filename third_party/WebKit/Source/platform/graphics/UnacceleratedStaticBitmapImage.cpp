// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/UnacceleratedStaticBitmapImage.h"

#include "components/viz/common/gpu/context_provider.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(sk_sp<SkImage> image) {
  DCHECK(!image->isTextureBacked());
  return base::AdoptRef(new UnacceleratedStaticBitmapImage(std::move(image)));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    sk_sp<SkImage> image) {
  CHECK(image);
  DCHECK(!image->isLazyGenerated());

  paint_image_ =
      CreatePaintImageBuilder().set_image(std::move(image)).TakePaintImage();
}

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(PaintImage image) {
  return base::AdoptRef(new UnacceleratedStaticBitmapImage(std::move(image)));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(PaintImage image)
    : paint_image_(std::move(image)) {
  CHECK(paint_image_.GetSkImage());
}

UnacceleratedStaticBitmapImage::~UnacceleratedStaticBitmapImage() = default;

IntSize UnacceleratedStaticBitmapImage::Size() const {
  return IntSize(paint_image_.width(), paint_image_.height());
}

bool UnacceleratedStaticBitmapImage::IsPremultiplied() const {
  return paint_image_.GetSkImage()->alphaType() ==
         SkAlphaType::kPremul_SkAlphaType;
}

scoped_refptr<StaticBitmapImage>
UnacceleratedStaticBitmapImage::MakeAccelerated(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_wrapper) {
  if (!context_wrapper)
    return nullptr;  // Can happen if the context is lost.

  GrContext* grcontext = context_wrapper->ContextProvider()->GetGrContext();
  if (!grcontext)
    return nullptr;  // Can happen if the context is lost.

  // TODO(crbug.com/782383): This can return a SkColorSpace, which should be
  // passed along.
  sk_sp<SkImage> gpu_skimage =
      paint_image_.GetSkImage()->makeTextureImage(grcontext, nullptr);
  if (!gpu_skimage)
    return nullptr;

  return AcceleratedStaticBitmapImage::CreateFromSkImage(
      std::move(gpu_skimage), std::move(context_wrapper));
}

bool UnacceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque(MetadataMode) {
  return paint_image_.GetSkImage()->isOpaque();
}

void UnacceleratedStaticBitmapImage::Draw(PaintCanvas* canvas,
                                          const PaintFlags& flags,
                                          const FloatRect& dst_rect,
                                          const FloatRect& src_rect,
                                          RespectImageOrientationEnum,
                                          ImageClampingMode clamp_mode,
                                          ImageDecodingMode) {
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect, clamp_mode,
                                PaintImageForCurrentFrame());
}

PaintImage UnacceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  return paint_image_;
}

}  // namespace blink
