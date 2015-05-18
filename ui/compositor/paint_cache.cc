// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include "cc/playback/display_item_list.h"
#include "ui/compositor/paint_context.h"

namespace ui {

PaintCache::PaintCache() : has_cache_(false) {
}

PaintCache::~PaintCache() {
}

bool PaintCache::UseCache(const PaintContext& context) {
  if (!has_cache_)
    return false;
  DCHECK(context.list_);
  auto* item = context.list_->CreateAndAppendItem<cc::DrawingDisplayItem>();
  display_item_.CloneTo(item);
  return true;
}

void PaintCache::SetCache(const cc::DrawingDisplayItem* item) {
  item->CloneTo(&display_item_);
  has_cache_ = true;
}

}  // namespace ui
