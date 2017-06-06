// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/compositor/paint_context.h"

namespace ui {

PaintCache::PaintCache() {}

PaintCache::~PaintCache() {
}

bool PaintCache::UseCache(const PaintContext& context,
                          const gfx::Size& size_in_context) {
  if (!paint_op_buffer_)
    return false;
  DCHECK(context.list_);
  cc::PaintOpBuffer* buffer = context.list_->StartPaint();
  buffer->push<cc::DrawRecordOp>(paint_op_buffer_);
  gfx::Rect bounds_in_layer = context.ToLayerSpaceBounds(size_in_context);
  context.list_->EndPaintOfUnpaired(bounds_in_layer);
  return true;
}

cc::PaintOpBuffer* PaintCache::ResetCache() {
  paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
  return paint_op_buffer_.get();
}

void PaintCache::FinalizeCache() {
  paint_op_buffer_->ShrinkToFit();
}

}  // namespace ui
