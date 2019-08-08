// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"

#include <utility>

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"

namespace blink {

namespace {
void DrawInternal(cc::PaintCanvas* canvas,
                  const FloatRect& dest_rect,
                  const FloatRect& src_rect,
                  const PaintFlags& flags,
                  scoped_refptr<PaintWorkletInput> input) {
  canvas->drawImage(PaintImageBuilder::WithDefault()
                        .set_paint_worklet_input(std::move(input))
                        .set_id(PaintImage::GetNextId())
                        .TakePaintImage(),
                    0, 0, &flags);
}
}  // namespace

void PaintWorkletDeferredImage::Draw(cc::PaintCanvas* canvas,
                                     const PaintFlags& flags,
                                     const FloatRect& dest_rect,
                                     const FloatRect& src_rect,
                                     RespectImageOrientationEnum,
                                     ImageClampingMode,
                                     ImageDecodingMode) {
  DrawInternal(canvas, dest_rect, src_rect, flags, input_);
}

void PaintWorkletDeferredImage::DrawTile(GraphicsContext& context,
                                         const FloatRect& src_rect) {
  DrawInternal(context.Canvas(), FloatRect(), src_rect, context.FillFlags(),
               input_);
}

}  // namespace blink
