// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PlaceholderImage.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace blink {

namespace {

// Gray with 40% opacity.
const RGBA32 kFillColor = 0x66808080;

}  // namespace

PlaceholderImage::~PlaceholderImage() {}

sk_sp<SkImage> PlaceholderImage::ImageForCurrentFrame() {
  if (image_for_current_frame_)
    return image_for_current_frame_;

  const FloatRect dest_rect(0.0f, 0.0f, static_cast<float>(size_.Width()),
                            static_cast<float>(size_.Height()));
  PaintRecordBuilder builder(dest_rect);
  GraphicsContext& context = builder.Context();
  context.BeginRecording(dest_rect);

  context.SetFillColor(kFillColor);
  context.FillRect(dest_rect);

  image_for_current_frame_ = SkImage::MakeFromPicture(
      ToSkPicture(builder.EndRecording()),
      SkISize::Make(size_.Width(), size_.Height()), nullptr, nullptr,
      SkImage::BitDepth::kU8, SkColorSpace::MakeSRGB());

  return image_for_current_frame_;
}

void PlaceholderImage::Draw(PaintCanvas* canvas,
                            const PaintFlags& base_flags,
                            const FloatRect& dest_rect,
                            const FloatRect& src_rect,
                            RespectImageOrientationEnum,
                            ImageClampingMode) {
  if (!src_rect.Intersects(FloatRect(0.0f, 0.0f,
                                     static_cast<float>(size_.Width()),
                                     static_cast<float>(size_.Height())))) {
    return;
  }

  PaintFlags flags(base_flags);
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(kFillColor);
  canvas->drawRect(dest_rect, flags);
}

void PlaceholderImage::DestroyDecodedData() {
  image_for_current_frame_.reset();
}

}  // namespace blink
