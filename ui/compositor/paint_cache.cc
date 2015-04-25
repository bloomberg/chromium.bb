// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include "cc/resources/display_item_list.h"
#include "cc/resources/drawing_display_item.h"
#include "ui/compositor/paint_context.h"

namespace ui {

PaintCache::PaintCache() {
}

PaintCache::~PaintCache() {
}

bool PaintCache::UseCache(const PaintContext& context) {
  if (!display_item_)
    return false;
  DCHECK(context.list_);
  context.list_->AppendItem(display_item_->Clone());
  return true;
}

void PaintCache::SetCache(scoped_ptr<cc::DrawingDisplayItem> item) {
  display_item_ = item.Pass();
}

}  // namespace ui
