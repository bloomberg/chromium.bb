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
  for (auto it = closers_.rbegin(); it != closers_.rend(); ++it)
    context_.list_->AppendItem(make_scoped_ptr(*it));
}

void ClipTransformRecorder::ClipRect(const gfx::Rect& clip_rect) {
  if (context_.list_) {
    context_.list_->AppendItem(
        cc::ClipDisplayItem::Create(clip_rect, std::vector<SkRRect>()));
    closers_.push_back(cc::EndClipDisplayItem::Create().release());
  } else {
    context_.canvas_->ClipRect(clip_rect);
  }
}

void ClipTransformRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  if (context_.list_) {
    context_.list_->AppendItem(cc::ClipPathDisplayItem::Create(
        clip_path, SkRegion::kIntersect_Op, anti_alias));
    closers_.push_back(cc::EndClipPathDisplayItem::Create().release());
  } else {
    context_.canvas_->ClipPath(clip_path, anti_alias);
  }
}

void ClipTransformRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  if (context_.list_) {
    context_.list_->AppendItem(cc::ClipPathDisplayItem::Create(
        clip_path, SkRegion::kIntersect_Op, anti_alias));
    closers_.push_back(cc::EndClipPathDisplayItem::Create().release());
  } else {
    context_.canvas_->ClipPath(clip_path, anti_alias);
  }
}

void ClipTransformRecorder::Transform(const gfx::Transform& transform) {
  if (context_.list_) {
    context_.list_->AppendItem(cc::TransformDisplayItem::Create(transform));
    closers_.push_back(cc::EndTransformDisplayItem::Create().release());
  } else {
    context_.canvas_->Transform(transform);
  }
}

}  // namespace ui
