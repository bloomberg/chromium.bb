// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/GridLayoutUtils.h"

#include "platform/LengthFunctions.h"

namespace blink {

static LayoutUnit ComputeMarginLogicalSizeForChild(
    const LayoutGrid& grid,
    MarginDirection for_direction,
    const LayoutBox& child) {
  if (!child.StyleRef().HasMargin())
    return LayoutUnit();

  bool is_inline_direction = for_direction == kInlineDirection;
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  LayoutUnit logical_size =
      is_inline_direction ? child.LogicalWidth() : child.LogicalHeight();
  Length margin_start_length = is_inline_direction
                                   ? child.StyleRef().MarginStart()
                                   : child.StyleRef().MarginBefore();
  Length margin_end_length = is_inline_direction
                                 ? child.StyleRef().MarginEnd()
                                 : child.StyleRef().MarginAfter();
  child.ComputeMarginsForDirection(
      for_direction, &grid, child.ContainingBlockLogicalWidthForContent(),
      logical_size, margin_start, margin_end, margin_start_length,
      margin_end_length);

  return margin_start + margin_end;
}

LayoutUnit GridLayoutUtils::MarginLogicalWidthForChild(const LayoutGrid& grid,
                                                       const LayoutBox& child) {
  return child.NeedsLayout()
             ? ComputeMarginLogicalSizeForChild(grid, kInlineDirection, child)
             : child.MarginLogicalWidth();
}

LayoutUnit GridLayoutUtils::MarginLogicalHeightForChild(
    const LayoutGrid& grid,
    const LayoutBox& child) {
  return child.NeedsLayout()
             ? ComputeMarginLogicalSizeForChild(grid, kBlockDirection, child)
             : child.MarginLogicalHeight();
}

}  // namespace blink
