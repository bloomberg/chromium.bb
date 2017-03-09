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

void BlockPaintInvalidator::clearPreviousVisualRects() {
  m_block.frame()->selection().clearPreviousCaretVisualRect(m_block);
  m_block.frame()->page()->dragCaret().clearPreviousVisualRect(m_block);
}

PaintInvalidationReason BlockPaintInvalidator::invalidatePaintIfNeeded(
    const PaintInvalidatorContext& context) {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(m_block, context).invalidatePaintIfNeeded();

  m_block.frame()->selection().invalidatePaintIfNeeded(m_block, context);
  m_block.frame()->page()->dragCaret().invalidatePaintIfNeeded(m_block,
                                                               context);

  return reason;
}

}  // namespace blink
