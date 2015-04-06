// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_transform_recorder.h"

#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context,
                                             const gfx::Rect& clip_rect)
    : canvas_(context.canvas()) {
  canvas_->Save();
  canvas_->ClipRect(clip_rect);
}

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context,
                                             const gfx::Transform& transform)
    : canvas_(context.canvas()) {
  canvas_->Save();
  canvas_->Transform(transform);
}

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context,
                                             const gfx::Rect& clip_rect,
                                             const gfx::Transform& transform)
    : canvas_(context.canvas()) {
  canvas_->Save();
  canvas_->ClipRect(clip_rect);
  canvas_->Transform(transform);
}

ClipTransformRecorder::~ClipTransformRecorder() {
  canvas_->Restore();
}

}  // namespace ui
