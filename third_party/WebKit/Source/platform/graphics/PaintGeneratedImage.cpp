// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PaintGeneratedImage.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

void PaintGeneratedImage::draw(SkCanvas* canvas,
                               const SkPaint& paint,
                               const FloatRect& destRect,
                               const FloatRect& srcRect,
                               RespectImageOrientationEnum,
                               ImageClampingMode,
                               const ColorBehavior& colorBehavior) {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  SkAutoCanvasRestore ar(canvas, true);
  canvas->clipRect(destRect);
  canvas->translate(destRect.x(), destRect.y());
  if (destRect.size() != srcRect.size())
    canvas->scale(destRect.width() / srcRect.width(),
                  destRect.height() / srcRect.height());
  canvas->translate(-srcRect.x(), -srcRect.y());
  canvas->drawPicture(m_picture.get(), nullptr, &paint);
}

void PaintGeneratedImage::drawTile(GraphicsContext& context,
                                   const FloatRect& srcRect) {
  // TODO(ccameron): This function should not ignore |context|'s color behavior.
  // https://crbug.com/672306
  context.drawPicture(m_picture.get());
}

}  // namespace blink
