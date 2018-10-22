// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"

#include <memory>
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"

namespace blink {

PaintRenderingContext2D::PaintRenderingContext2D(
    const IntSize& container_size,
    const CanvasColorParams& color_params,
    const PaintRenderingContext2DSettings& context_settings,
    float zoom)
    : container_size_(container_size),
      color_params_(color_params),
      context_settings_(context_settings),
      effective_zoom_(zoom) {
  InitializePaintRecorder();

  clip_antialiasing_ = kAntiAliased;
  ModifiableState().SetShouldAntialias(true);

  Canvas()->clear(context_settings.alpha() ? SK_ColorTRANSPARENT
                                           : SK_ColorBLACK);
  did_record_draw_commands_in_paint_recorder_ = true;
  Canvas()->scale(zoom, zoom);
}

void PaintRenderingContext2D::InitializePaintRecorder() {
  paint_recorder_ = std::make_unique<PaintRecorder>();
  cc::PaintCanvas* canvas = paint_recorder_->beginRecording(
      container_size_.Width(), container_size_.Height());

  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  did_record_draw_commands_in_paint_recorder_ = false;
}

cc::PaintCanvas* PaintRenderingContext2D::Canvas() const {
  DCHECK(paint_recorder_);
  DCHECK(paint_recorder_->getRecordingCanvas());
  return paint_recorder_->getRecordingCanvas();
}

void PaintRenderingContext2D::DidDraw(const SkIRect&) {
  did_record_draw_commands_in_paint_recorder_ = true;
}

int PaintRenderingContext2D::Width() const {
  return container_size_.Width();
}

int PaintRenderingContext2D::Height() const {
  return container_size_.Height();
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

// We need to account for the |effective_zoom_| for shadow effects only, and not
// for line width. This is because the line width is affected by skia's current
// transform matrix (CTM) while shadows are not. The skia's CTM combines both
// the canvas context transform and the CSS layout transform. That means, the
// |effective_zoom_| is implictly applied to line width through CTM.
double PaintRenderingContext2D::shadowBlur() const {
  return BaseRenderingContext2D::shadowBlur() / effective_zoom_;
}

void PaintRenderingContext2D::setShadowBlur(double blur) {
  BaseRenderingContext2D::setShadowBlur(blur * effective_zoom_);
}

double PaintRenderingContext2D::shadowOffsetX() const {
  return BaseRenderingContext2D::shadowOffsetX() / effective_zoom_;
}

void PaintRenderingContext2D::setShadowOffsetX(double x) {
  BaseRenderingContext2D::setShadowOffsetX(x * effective_zoom_);
}

double PaintRenderingContext2D::shadowOffsetY() const {
  return BaseRenderingContext2D::shadowOffsetY() / effective_zoom_;
}

void PaintRenderingContext2D::setShadowOffsetY(double y) {
  BaseRenderingContext2D::setShadowOffsetY(y * effective_zoom_);
}

cc::PaintCanvas* PaintRenderingContext2D::DrawingCanvas() const {
  return Canvas();
}

cc::PaintCanvas* PaintRenderingContext2D::ExistingDrawingCanvas() const {
  return Canvas();
}

void PaintRenderingContext2D::ValidateStateStack() const {
#if DCHECK_IS_ON()
  if (cc::PaintCanvas* sk_canvas = ExistingDrawingCanvas()) {
    DCHECK_EQ(static_cast<size_t>(sk_canvas->getSaveCount()),
              state_stack_.size() + 1);
  }
#endif
}

bool PaintRenderingContext2D::StateHasFilter() {
  return GetState().HasFilterForOffscreenCanvas(IntSize(Width(), Height()),
                                                this);
}

sk_sp<PaintFilter> PaintRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(IntSize(Width(), Height()),
                                                this);
}

void PaintRenderingContext2D::WillOverwriteCanvas() {
  previous_frame_.reset();
  if (did_record_draw_commands_in_paint_recorder_) {
    // Discard previous draw commands
    paint_recorder_->finishRecordingAsPicture();
    InitializePaintRecorder();
  }
}

sk_sp<PaintRecord> PaintRenderingContext2D::GetRecord() {
  if (!did_record_draw_commands_in_paint_recorder_ && !!previous_frame_) {
    return previous_frame_;  // Reuse the previous frame
  }

  CHECK(paint_recorder_);
  DCHECK(paint_recorder_->getRecordingCanvas());
  previous_frame_ = paint_recorder_->finishRecordingAsPicture();
  InitializePaintRecorder();
  return previous_frame_;
}

}  // namespace blink
