// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockFlowPainter_h
#define BlockFlowPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class LayoutBlockFlow;

class BlockFlowPainter {
  STACK_ALLOCATED();

 public:
  BlockFlowPainter(const LayoutBlockFlow& layout_block_flow)
      : layout_block_flow_(layout_block_flow) {}
  void PaintContents(const PaintInfo&, const LayoutPoint&);
  void PaintFloats(const PaintInfo&, const LayoutPoint&);

 private:
  const LayoutBlockFlow& layout_block_flow_;
};

}  // namespace blink

#endif  // BlockFlowPainter_h
