// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockFlowPaintInvalidator_h
#define BlockFlowPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutBlockFlow;

class BlockFlowPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BlockFlowPaintInvalidator(const LayoutBlockFlow& block_flow)
      : block_flow_(block_flow) {}

  void InvalidatePaintForOverhangingFloats() {
    InvalidatePaintForOverhangingFloatsInternal(kInvalidateDescendants);
  }

  void InvalidateDisplayItemClients(PaintInvalidationReason);

 private:
  enum InvalidateDescendantMode {
    kDontInvalidateDescendants,
    kInvalidateDescendants
  };
  void InvalidatePaintForOverhangingFloatsInternal(InvalidateDescendantMode);

  const LayoutBlockFlow& block_flow_;
};

}  // namespace blink

#endif
