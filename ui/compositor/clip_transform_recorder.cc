// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_transform_recorder.h"

#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/transform.h"

namespace ui {

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context)
    : canvas_(context.canvas_) {
  canvas_->Save();
}

ClipTransformRecorder::~ClipTransformRecorder() {
  canvas_->Restore();
}

void ClipTransformRecorder::ClipRect(const gfx::Rect& clip_rect) {
  canvas_->ClipRect(clip_rect);
}

void ClipTransformRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  canvas_->ClipPath(clip_path, anti_alias);
}

void ClipTransformRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  canvas_->ClipPath(clip_path, anti_alias);
}

void ClipTransformRecorder::Transform(const gfx::Transform& transform) {
  canvas_->Transform(transform);
}

}  // namespace ui
