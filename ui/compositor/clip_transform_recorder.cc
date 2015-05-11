// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_transform_recorder.h"

#include "base/logging.h"
#include "cc/resources/clip_display_item.h"
#include "cc/resources/clip_path_display_item.h"
#include "cc/resources/display_item_list.h"
#include "cc/resources/transform_display_item.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"

namespace ui {

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context)
    : context_(context), num_closers_(0) {
}

ClipTransformRecorder::~ClipTransformRecorder() {
  if (context_.list_) {
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
  } else if (num_closers_) {
    context_.canvas_->Restore();
  }
}

void ClipTransformRecorder::ClipRect(const gfx::Rect& clip_rect) {
  if (context_.list_) {
    auto* item = context_.list_->CreateAndAppendItem<cc::ClipDisplayItem>();
    item->SetNew(clip_rect, std::vector<SkRRect>());
  } else {
    if (!num_closers_)
      context_.canvas_->Save();
    context_.canvas_->ClipRect(clip_rect);
  }
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_RECT;
}

void ClipTransformRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  if (context_.list_) {
    auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
    item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  } else {
    if (!num_closers_)
      context_.canvas_->Save();
    context_.canvas_->ClipPath(clip_path, anti_alias);
  }
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

void ClipTransformRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  if (context_.list_) {
    auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
    item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  } else {
    if (!num_closers_)
      context_.canvas_->Save();
    context_.canvas_->ClipPath(clip_path, anti_alias);
  }
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

void ClipTransformRecorder::Transform(const gfx::Transform& transform) {
  if (context_.list_) {
    auto* item =
        context_.list_->CreateAndAppendItem<cc::TransformDisplayItem>();
    item->SetNew(transform);
  } else {
    if (!num_closers_)
      context_.canvas_->Save();
    context_.canvas_->Transform(transform);
  }
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = TRANSFORM;
}

}  // namespace ui
