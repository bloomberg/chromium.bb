// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositing_recorder.h"

#include "cc/resources/compositing_display_item.h"
#include "cc/resources/display_item_list.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"

namespace ui {

CompositingRecorder::CompositingRecorder(const PaintContext& context,
                                         uint8_t alpha)
    : context_(context), saved_(alpha < 255) {
  if (!saved_)
    return;

  if (context_.canvas_) {
    context_.canvas_->SaveLayerAlpha(alpha);
  } else {
    context_.list_->AppendItem(cc::CompositingDisplayItem::Create(
        alpha, SkXfermode::kSrcOver_Mode, nullptr /* no bounds */,
        skia::RefPtr<SkColorFilter>()));
  }
}

CompositingRecorder::~CompositingRecorder() {
  if (!saved_)
    return;

  if (context_.canvas_) {
    context_.canvas_->Restore();
  } else {
    context_.list_->AppendItem(cc::EndCompositingDisplayItem::Create());
  }
}

}  // namespace ui
