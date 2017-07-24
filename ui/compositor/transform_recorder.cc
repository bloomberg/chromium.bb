// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_recorder.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/compositor/paint_context.h"

namespace ui {

TransformRecorder::TransformRecorder(const PaintContext& context)
    : context_(context), transformed_(false) {}

TransformRecorder::~TransformRecorder() {
  if (!transformed_)
    return;

  context_.list_->StartPaint();
  context_.list_->push<cc::RestoreOp>();
  context_.list_->EndPaintOfPairedEnd();
}

void TransformRecorder::Transform(const gfx::Transform& transform) {
  DCHECK(!transformed_);
  if (transform.IsIdentity())
    return;

  context_.list_->StartPaint();
  context_.list_->push<cc::SaveOp>();
  context_.list_->push<cc::ConcatOp>(static_cast<SkMatrix>(transform.matrix()));
  context_.list_->EndPaintOfPairedBegin();

  transformed_ = true;
}

}  // namespace ui
