// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/MultiColumnSetPainter.h"

#include "core/layout/LayoutMultiColumnSet.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void MultiColumnSetPainter::PaintObject(const PaintInfo& paint_info,
                                        const LayoutPoint& paint_offset) {
  if (layout_multi_column_set_.Style()->Visibility() != EVisibility::kVisible)
    return;

  BlockPainter(layout_multi_column_set_).PaintObject(paint_info, paint_offset);

  // FIXME: Right now we're only painting in the foreground phase.
  // Columns should technically respect phases and allow for
  // background/float/foreground overlap etc., just like LayoutBlocks do. Note
  // this is a pretty minor issue, since the old column implementation clipped
  // columns anyway, thus making it impossible for them to overlap one another.
  // It's also really unlikely that the columns would overlap another block.
  if (!layout_multi_column_set_.FlowThread() ||
      (paint_info.phase != PaintPhase::kForeground &&
       paint_info.phase != PaintPhase::kSelection))
    return;

  PaintColumnRules(paint_info, paint_offset);
}

void MultiColumnSetPainter::PaintColumnRules(const PaintInfo& paint_info,
                                             const LayoutPoint& paint_offset) {
  Vector<LayoutRect> column_rule_bounds;
  if (!layout_multi_column_set_.ComputeColumnRuleBounds(paint_offset,
                                                        column_rule_bounds))
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_multi_column_set_,
          DisplayItem::kColumnRules))
    return;

  LayoutRect paint_rect = layout_multi_column_set_.VisualOverflowRect();
  paint_rect.MoveBy(paint_offset);
  LayoutObjectDrawingRecorder drawing_recorder(
      paint_info.context, layout_multi_column_set_, DisplayItem::kColumnRules,
      paint_rect);

  const ComputedStyle& block_style =
      layout_multi_column_set_.MultiColumnBlockFlow()->StyleRef();
  EBorderStyle rule_style = block_style.ColumnRuleStyle();
  bool left_to_right =
      layout_multi_column_set_.Style()->IsLeftToRightDirection();
  BoxSide box_side = layout_multi_column_set_.IsHorizontalWritingMode()
                         ? left_to_right ? kBSLeft : kBSRight
                         : left_to_right ? kBSTop : kBSBottom;
  const Color& rule_color = layout_multi_column_set_.ResolveColor(
      block_style, CSSPropertyColumnRuleColor);

  for (auto& bound : column_rule_bounds) {
    IntRect pixel_snapped_rule_rect = PixelSnappedIntRect(bound);
    ObjectPainter::DrawLineForBoxSide(
        paint_info.context, pixel_snapped_rule_rect.X(),
        pixel_snapped_rule_rect.Y(), pixel_snapped_rule_rect.MaxX(),
        pixel_snapped_rule_rect.MaxY(), box_side, rule_color, rule_style, 0, 0,
        true);
  }
}

}  // namespace blink
