// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

#include "components/viz/common/gpu/context_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(sk_sp<SkImage> image,
                                       ImageOrientation orientation) {
  DCHECK(!image->isTextureBacked());
  return base::AdoptRef(
      new UnacceleratedStaticBitmapImage(std::move(image), orientation));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    ImageOrientation orientation)
    : StaticBitmapImage(orientation) {
  CHECK(image);
  DCHECK(!image->isLazyGenerated());
  paint_image_ =
      CreatePaintImageBuilder()
          .set_image(std::move(image), cc::PaintImage::GetNextContentId())
          .TakePaintImage();
}

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(PaintImage image,
                                       ImageOrientation orientation) {
  return base::AdoptRef(
      new UnacceleratedStaticBitmapImage(std::move(image), orientation));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    PaintImage image,
    ImageOrientation orientation)
    : StaticBitmapImage(orientation), paint_image_(std::move(image)) {
  CHECK(paint_image_.GetSkImage());
}

UnacceleratedStaticBitmapImage::~UnacceleratedStaticBitmapImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!original_skia_image_)
    return;

  if (!original_skia_image_task_runner_->BelongsToCurrentThread()) {
    PostCrossThreadTask(
        *original_skia_image_task_runner_, FROM_HERE,
        CrossThreadBindOnce([](sk_sp<SkImage> image) { image.reset(); },
                            std::move(original_skia_image_)));
  } else {
    original_skia_image_.reset();
  }
}

IntSize UnacceleratedStaticBitmapImage::Size() const {
  return IntSize(paint_image_.width(), paint_image_.height());
}

bool UnacceleratedStaticBitmapImage::IsPremultiplied() const {
  return paint_image_.GetSkImage()->alphaType() ==
         SkAlphaType::kPremul_SkAlphaType;
}

bool UnacceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque() {
  return paint_image_.GetSkImage()->isOpaque();
}

void UnacceleratedStaticBitmapImage::Draw(
    cc::PaintCanvas* canvas,
    const cc::PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    RespectImageOrientationEnum should_respect_image_orientation,
    ImageClampingMode clamp_mode,
    ImageDecodingMode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect, clamp_mode,
                                should_respect_image_orientation,
                                PaintImageForCurrentFrame());
}

PaintImage UnacceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  return paint_image_;
}

void UnacceleratedStaticBitmapImage::Transfer() {
  DETACH_FROM_THREAD(thread_checker_);

  original_skia_image_ = paint_image_.GetSkImage();
  original_skia_image_task_runner_ = Thread::Current()->GetTaskRunner();
}

scoped_refptr<StaticBitmapImage>
UnacceleratedStaticBitmapImage::ConvertToColorSpace(
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
  return UnacceleratedStaticBitmapImage::Create(skia_image, orientation_);
}

}  // namespace blink
