// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_recorder.h"

#include "ui/compositor/paint_context.h"

namespace ui {

PaintRecorder::PaintRecorder(const PaintContext& context)
    : canvas_(context.canvas_) {
}

PaintRecorder::~PaintRecorder() {
}

}  // namespace ui
