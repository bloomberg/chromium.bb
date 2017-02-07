// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockPaintInvalidator_h
#define BlockPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

struct PaintInvalidatorContext;
class LayoutBlock;

class BlockPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BlockPaintInvalidator(const LayoutBlock& block) : m_block(block) {}

  void clearPreviousVisualRects();
  PaintInvalidationReason invalidatePaintIfNeeded(
      const PaintInvalidatorContext&);

 private:
  const LayoutBlock& m_block;
};

}  // namespace blink

#endif
