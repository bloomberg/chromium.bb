// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

struct PaintInfo;
class PaintInfoWithOffset;
class InlineBox;
class LayoutBlock;
class LayoutBox;
class LayoutFlexibleBox;
class LayoutPoint;

class BlockPainter {
  STACK_ALLOCATED();

 public:
  BlockPainter(const LayoutBlock& block) : layout_block_(block) {}

  void Paint(const PaintInfo&);
  void PaintObject(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintContents(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintChildren(const PaintInfo&);
  void PaintChild(const LayoutBox&, const PaintInfo&);
  void PaintOverflowControlsIfNeeded(const PaintInfo&,
                                     const LayoutPoint& paint_offset);

  // See ObjectPainter::paintAllPhasesAtomically().
  void PaintAllChildPhasesAtomically(const LayoutBox&, const PaintInfo&);
  static void PaintChildrenOfFlexibleBox(const LayoutFlexibleBox&,
                                         const PaintInfo&);
  static void PaintInlineBox(const InlineBox&, const PaintInfo&);

  bool ShouldPaint(const PaintInfoWithOffset&) const;

 private:
  // Paint scroll hit test placeholders in the correct paint order (see:
  // ScrollHitTestDisplayItem.h).
  void PaintScrollHitTestDisplayItem(const PaintInfo&);
  // Paint a hit test display item and record hit test data. This should be
  // called in the background paint phase even if there is no other painted
  // content.
  void RecordHitTestData(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintBlockFlowContents(const PaintInfo&, const LayoutPoint&);
  void PaintCarets(const PaintInfo&, const LayoutPoint& paint_offset);

  const LayoutBlock& layout_block_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_PAINTER_H_
