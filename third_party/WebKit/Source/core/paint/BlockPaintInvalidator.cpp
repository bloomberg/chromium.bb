// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockPaintInvalidator.h"

#include "core/editing/DragCaret.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBlock.h"
#include "core/page/Page.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

void BlockPaintInvalidator::ClearPreviousVisualRects() {
  block_.GetFrame()->Selection().ClearPreviousCaretVisualRect(block_);
  block_.GetFrame()->GetPage()->GetDragCaret().ClearPreviousVisualRect(block_);
}

PaintInvalidationReason BlockPaintInvalidator::InvalidatePaintIfNeeded(
    const PaintInvalidatorContext& context) {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(block_, context).InvalidatePaintIfNeeded();

  block_.GetFrame()->Selection().InvalidatePaintIfNeeded(block_, context);
  block_.GetFrame()->GetPage()->GetDragCaret().InvalidatePaintIfNeeded(block_,
                                                                       context);

  return reason;
}

}  // namespace blink
