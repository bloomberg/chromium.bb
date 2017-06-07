// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_recorder.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"

namespace ui {

ClipRecorder::ClipRecorder(const PaintContext& context) : context_(context) {}

ClipRecorder::~ClipRecorder() {
  for (int i = 0; i < num_closers_; ++i) {
    // Each restore is part of a separate visual rect, so gets its own
    // StartPaint/EndPaintOfPairedEnd.
    cc::PaintOpBuffer* buffer = context_.list_->StartPaint();
    buffer->push<cc::RestoreOp>();
    context_.list_->EndPaintOfPairedEnd();
  }
}

void ClipRecorder::ClipRect(const gfx::Rect& clip_rect) {
  bool antialias = false;

  cc::PaintOpBuffer* buffer = context_.list_->StartPaint();
  buffer->push<cc::SaveOp>();
  buffer->push<cc::ClipRectOp>(gfx::RectToSkRect(clip_rect),
                               SkClipOp::kIntersect, antialias);
  context_.list_->EndPaintOfPairedBegin();
  ++num_closers_;
}

void ClipRecorder::ClipPath(const gfx::Path& clip_path) {
  bool antialias = false;

  cc::PaintOpBuffer* buffer = context_.list_->StartPaint();
  buffer->push<cc::SaveOp>();
  buffer->push<cc::ClipPathOp>(clip_path, SkClipOp::kIntersect, antialias);
  context_.list_->EndPaintOfPairedBegin();
  ++num_closers_;
}

void ClipRecorder::ClipPathWithAntiAliasing(const gfx::Path& clip_path) {
  bool antialias = true;

  cc::PaintOpBuffer* buffer = context_.list_->StartPaint();
  buffer->push<cc::SaveOp>();
  buffer->push<cc::ClipPathOp>(clip_path, SkClipOp::kIntersect, antialias);
  context_.list_->EndPaintOfPairedBegin();
  ++num_closers_;
}

}  // namespace ui
