// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_transform_recorder.h"

#include "base/logging.h"
#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/transform_display_item.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"

namespace ui {

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context)
    : context_(context), num_closers_(0) {
}

ClipTransformRecorder::~ClipTransformRecorder() {
  for (size_t i = 0; i < num_closers_; ++i) {
    switch (closers_[i]) {
      case CLIP_RECT:
        context_.list_->CreateAndAppendItem<cc::EndClipDisplayItem>();
        break;
      case CLIP_PATH:
        context_.list_->CreateAndAppendItem<cc::EndClipPathDisplayItem>();
        break;
      case TRANSFORM:
        context_.list_->CreateAndAppendItem<cc::EndTransformDisplayItem>();
        break;
    }
  }
}

void ClipTransformRecorder::ClipRect(const gfx::Rect& clip_rect) {
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipDisplayItem>();
  item->SetNew(clip_rect, std::vector<SkRRect>());
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_RECT;
}

void ClipTransformRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
  item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

void ClipTransformRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
  item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

void ClipTransformRecorder::Transform(const gfx::Transform& transform) {
  auto* item = context_.list_->CreateAndAppendItem<cc::TransformDisplayItem>();
  item->SetNew(transform);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = TRANSFORM;
}

}  // namespace ui
