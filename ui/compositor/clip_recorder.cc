// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_recorder.h"

#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/display_item_list.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"

namespace ui {

ClipRecorder::ClipRecorder(const PaintContext& context,
                           const gfx::Size& size_in_layer)
    : context_(context),
      bounds_in_layer_(context.ToLayerSpaceBounds(size_in_layer)),
      num_closers_(0) {}

ClipRecorder::~ClipRecorder() {
  for (size_t i = num_closers_; i > 0; --i) {
    switch (closers_[i - 1]) {
      case CLIP_RECT:
        context_.list_->CreateAndAppendItem<cc::EndClipDisplayItem>(
            bounds_in_layer_);
        break;
      case CLIP_PATH:
        context_.list_->CreateAndAppendItem<cc::EndClipPathDisplayItem>(
            bounds_in_layer_);
        break;
    }
  }
}

void ClipRecorder::ClipRect(const gfx::Rect& clip_rect) {
  gfx::Rect clip_in_layer_space = context_.ToLayerSpaceRect(clip_rect);
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipDisplayItem>(
      clip_in_layer_space);
  item->SetNew(clip_rect, std::vector<SkRRect>());
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_RECT;
}

void ClipRecorder::ClipPath(const gfx::Path& clip_path) {
  bool anti_alias = false;
  // As a further optimization, consider passing a more granular visual rect.
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>(
      bounds_in_layer_);
  item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

void ClipRecorder::ClipPathWithAntiAliasing(
    const gfx::Path& clip_path) {
  bool anti_alias = true;
  // As a further optimization, consider passing a more granular visual rect.
  auto* item = context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>(
      bounds_in_layer_);
  item->SetNew(clip_path, SkRegion::kIntersect_Op, anti_alias);
  DCHECK_LT(num_closers_, arraysize(closers_));
  closers_[num_closers_++] = CLIP_PATH;
}

}  // namespace ui
