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

sk_sp<SkImage> PlaceholderImage::imageForCurrentFrame(
    const ColorBehavior& colorBehavior) {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  if (m_imageForCurrentFrame)
    return m_imageForCurrentFrame;

  const FloatRect destRect(0.0f, 0.0f, static_cast<float>(m_size.width()),
                           static_cast<float>(m_size.height()));
  PaintRecordBuilder builder(destRect);
  GraphicsContext& context = builder.context();
  context.beginRecording(destRect);

  context.setFillColor(kFillColor);
  context.fillRect(destRect);

  m_imageForCurrentFrame = SkImage::MakeFromPicture(
      ToSkPicture(builder.endRecording()),
      SkISize::Make(m_size.width(), m_size.height()), nullptr, nullptr,
      SkImage::BitDepth::kU8, SkColorSpace::MakeSRGB());

  return m_imageForCurrentFrame;
}

void PlaceholderImage::draw(PaintCanvas* canvas,
                            const PaintFlags& baseFlags,
                            const FloatRect& destRect,
                            const FloatRect& srcRect,
                            RespectImageOrientationEnum,
                            ImageClampingMode) {
  if (!srcRect.intersects(FloatRect(0.0f, 0.0f,
                                    static_cast<float>(m_size.width()),
                                    static_cast<float>(m_size.height())))) {
    return;
  }

  PaintFlags flags(baseFlags);
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(kFillColor);
  canvas->drawRect(destRect, flags);
}

void PlaceholderImage::destroyDecodedData() {
  m_imageForCurrentFrame.reset();
}

}  // namespace blink
