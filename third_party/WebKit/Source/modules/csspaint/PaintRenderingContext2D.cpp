// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintRenderingContext2D.h"

#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include <memory>

namespace blink {

PaintRenderingContext2D::PaintRenderingContext2D(
    std::unique_ptr<ImageBuffer> image_buffer,
    const PaintRenderingContext2DSettings& context_settings,
    float zoom)
    : image_buffer_(std::move(image_buffer)),
      context_settings_(context_settings) {
  clip_antialiasing_ = kAntiAliased;
  ModifiableState().SetShouldAntialias(true);

  // RecordingImageBufferSurface doesn't call ImageBufferSurface::clear
  // explicitly.
  DCHECK(image_buffer_);
  image_buffer_->Canvas()->clear(context_settings.alpha() ? SK_ColorTRANSPARENT
                                                          : SK_ColorBLACK);
  image_buffer_->DidDraw(FloatRect(0, 0, Width(), Height()));

  image_buffer_->Canvas()->scale(zoom, zoom);
}

int PaintRenderingContext2D::Width() const {
  DCHECK(image_buffer_);
  return image_buffer_->Size().Width();
}

int PaintRenderingContext2D::Height() const {
  DCHECK(image_buffer_);
  return image_buffer_->Size().Height();
}

bool PaintRenderingContext2D::ParseColorOrCurrentColor(
    Color& color,
    const String& color_string) const {
  // We ignore "currentColor" for PaintRenderingContext2D and just make it
  // "black". "currentColor" can be emulated by having "color" as an input
  // property for the css-paint-api.
  // https://github.com/w3c/css-houdini-drafts/issues/133
  return ::blink::ParseColorOrCurrentColor(color, color_string, nullptr);
}

PaintCanvas* PaintRenderingContext2D::DrawingCanvas() const {
  return image_buffer_->Canvas();
}

PaintCanvas* PaintRenderingContext2D::ExistingDrawingCanvas() const {
  DCHECK(image_buffer_);
  return image_buffer_->Canvas();
}

AffineTransform PaintRenderingContext2D::BaseTransform() const {
  DCHECK(image_buffer_);
  return image_buffer_->BaseTransform();
}

void PaintRenderingContext2D::DidDraw(const SkIRect& dirty_rect) {
  DCHECK(image_buffer_);
  return image_buffer_->DidDraw(SkRect::Make(dirty_rect));
}

void PaintRenderingContext2D::ValidateStateStack() const {
#if DCHECK_IS_ON()
  if (PaintCanvas* sk_canvas = ExistingDrawingCanvas()) {
    DCHECK_EQ(static_cast<size_t>(sk_canvas->getSaveCount()),
              state_stack_.size() + 1);
  }
#endif
}

bool PaintRenderingContext2D::StateHasFilter() {
  return GetState().HasFilterForOffscreenCanvas(IntSize(Width(), Height()));
}

sk_sp<SkImageFilter> PaintRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(IntSize(Width(), Height()));
}

}  // namespace blink
