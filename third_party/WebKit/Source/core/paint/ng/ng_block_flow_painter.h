// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_block_flow_painter_h
#define ng_block_flow_painter_h

#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class LayoutNGBlockFlow;
class NGPaintFragment;

// Painter for NGBlockFlow which represents the root of a LayoutNG sub-tree.
// Paints the root fragment associated with the NGBlockFlow recursively, walking
// the LayoutNG fragment tree instead of the legacy layout tree.
class NGBlockFlowPainter {
  STACK_ALLOCATED();

 public:
  NGBlockFlowPainter(const LayoutNGBlockFlow& layout_ng_block_flow)
      : block_(layout_ng_block_flow) {}
  void PaintContents(const PaintInfo&, const LayoutPoint&);

 private:
  void PaintBoxFragment(const NGPaintFragment&,
                        const PaintInfo&,
                        const LayoutPoint&);

  const LayoutNGBlockFlow& block_;
};

}  // namespace blink

#endif  // ng_block_flow_painter_h
