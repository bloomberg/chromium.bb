// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositing_recorder.h"

#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"

namespace ui {

CompositingRecorder::CompositingRecorder(const PaintContext& context,
                                         uint8_t alpha)
    : canvas_(context.canvas()), saved_(alpha < 255) {
  if (saved_)
    canvas_->SaveLayerAlpha(alpha);
}

CompositingRecorder::~CompositingRecorder() {
  if (saved_)
    canvas_->Restore();
}

}  // namespace ui
