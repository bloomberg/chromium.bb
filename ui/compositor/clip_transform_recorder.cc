// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_transform_recorder.h"

#include "cc/resources/clip_display_item.h"
#include "cc/resources/clip_path_display_item.h"
#include "cc/resources/display_item_list.h"
#include "cc/resources/transform_display_item.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"

namespace ui {

ClipTransformRecorder::ClipTransformRecorder(const PaintContext& context)
    : context_(context) {
  if (context_.canvas_)
    context_.canvas_->Save();
}

ClipTransformRecorder::~ClipTransformRecorder() {
  if (context_.canvas_)
    context_.canvas_->Restore();
  for (Closer c : closers_) {
    switch (c) {
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
  if (context_.list_) {
    auto* item = context_.list_->CreateAndAppendItem<cc::ClipDisplayItem>();
    item->SetNew(clip_rect, std::vector<SkRRect>());
    closers_.push_back(CLIP_RECT);
  } else {
    context_.canvas_->ClipRect(clip_rect);
  }
}

void ClipTransformRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  if (context_.list_) {
    auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
    item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
    closers_.push_back(CLIP_PATH);
  } else {
    context_.canvas_->ClipPath(clip_path, anti_alias);
  }
}

void ClipTransformRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  if (context_.list_) {
    auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
    item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
    closers_.push_back(CLIP_PATH);
  } else {
    context_.canvas_->ClipPath(clip_path, anti_alias);
  }
}

void ClipTransformRecorder::Transform(const gfx::Transform& transform) {
  if (context_.list_) {
    auto* item =
        context_.list_->CreateAndAppendItem<cc::TransformDisplayItem>();
    item->SetNew(transform);
    closers_.push_back(TRANSFORM);
  } else {
    context_.canvas_->Transform(transform);
  }
}

}  // namespace ui
