// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_relative_utils.h"

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "wtf/Optional.h"

namespace blink {

// Returns the child's relative position wrt the containing fragment.
NGLogicalOffset ComputeRelativeOffset(const ComputedStyle& child_style,
                                      NGWritingMode container_writing_mode,
                                      TextDirection container_direction,
                                      NGLogicalSize container_logical_size) {
  NGLogicalOffset offset;
  NGPhysicalSize container_size =
      container_logical_size.ConvertToPhysical(container_writing_mode);

  Optional<LayoutUnit> left, right, top, bottom;

  if (!child_style.left().isAuto())
    left = valueForLength(child_style.left(), container_size.width);
  if (!child_style.right().isAuto())
    right = valueForLength(child_style.right(), container_size.width);
  if (!child_style.top().isAuto())
    top = valueForLength(child_style.top(), container_size.height);
  if (!child_style.bottom().isAuto())
    bottom = valueForLength(child_style.bottom(), container_size.height);

  // Implements confict resolution rules from spec:
  // https://www.w3.org/TR/css-position-3/#rel-pos
  if (!left && !right) {
    left = LayoutUnit();
    right = LayoutUnit();
  }
  if (!left)
    left = -right.value();
  if (!right)
    right = -left.value();
  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  }
  if (!top)
    top = -bottom.value();
  if (!bottom)
    bottom = -top.value();

  bool is_ltr = container_direction == TextDirection::kLtr;

  switch (container_writing_mode) {
    case kHorizontalTopBottom:
      offset.inline_offset = is_ltr ? left.value() : right.value();
      offset.block_offset = top.value();
      break;
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      offset.inline_offset = is_ltr ? top.value() : bottom.value();
      offset.block_offset = right.value();
      break;
    case kVerticalLeftRight:
      offset.inline_offset = is_ltr ? top.value() : bottom.value();
      offset.block_offset = left.value();
      break;
    case kSidewaysLeftRight:
      offset.inline_offset = is_ltr ? bottom.value() : top.value();
      offset.block_offset = left.value();
      break;
  }
  return offset;
}

}  // namespace blink
