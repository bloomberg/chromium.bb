// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_recorder.h"

#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/display_item_list.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"

namespace ui {

ClipRecorder::ClipRecorder(const PaintContext& context)
    : context_(context), num_closers_(0) {
}

ClipRecorder::~ClipRecorder() {
  for (size_t i = num_closers_; i > 0; --i) {
    switch (closers_[i - 1]) {
      case CLIP_RECT:
        context_.list_->CreateAndAppendItem<cc::EndClipDisplayItem>();
        break;
      case CLIP_PATH:
        context_.list_->CreateAndAppendItem<cc::EndClipPathDisplayItem>();
        break;
    }
  }
}

void ClipRecorder::ClipRect(const gfx::Rect& clip_rect) {
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipDisplayItem>();
  item->SetNew(clip_rect, std::vector<SkRRect>());
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_RECT;
}

void ClipRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
  item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

void ClipRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
  item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

}  // namespace ui
