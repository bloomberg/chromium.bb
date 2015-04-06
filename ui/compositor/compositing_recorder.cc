// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositing_recorder.h"

#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"

namespace ui {

CompositingRecorder::CompositingRecorder(const PaintContext& context,
                                         float opacity)
    : canvas_(context.canvas()) {
  canvas_->SaveLayerAlpha(0xff * opacity);
}

CompositingRecorder::~CompositingRecorder() {
  canvas_->Restore();
}

}  // namespace ui
