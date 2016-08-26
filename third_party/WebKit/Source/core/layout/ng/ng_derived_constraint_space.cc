// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_derived_constraint_space.h"

#include "core/layout/LayoutBox.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGDerivedConstraintSpace* NGDerivedConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  bool fixed_inline = false, fixed_block = false;
  // XXX for orthogonal writing mode this is not right
  LayoutUnit container_logical_width =
      std::max(LayoutUnit(), box.containingBlockLogicalWidthForContent());
  // XXX Make sure this height is correct
  LayoutUnit container_logical_height =
      box.containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
  if (box.hasOverrideLogicalContentWidth()) {
    container_logical_width = box.overrideLogicalContentWidth();
    fixed_inline = true;
  }
  if (box.hasOverrideLogicalContentHeight()) {
    container_logical_width = box.overrideLogicalContentHeight();
    fixed_block = true;
  }

  return new NGDerivedConstraintSpace(
      FromPlatformWritingMode(box.styleRef().getWritingMode()),
      NGLogicalSize(container_logical_width, container_logical_height),
      box.styleRef().overflowInlineDirection() == OverflowAuto,
      box.styleRef().overflowBlockDirection() == OverflowAuto, fixed_inline,
      fixed_block, FragmentNone);
}

NGDerivedConstraintSpace::NGDerivedConstraintSpace(NGWritingMode writing_mode,
                                                   NGLogicalSize container_size,
                                                   bool inline_triggers,
                                                   bool block_triggers,
                                                   bool fixed_inline,
                                                   bool fixed_block,
                                                   NGFragmentationType type)
    : NGConstraintSpace(writing_mode, container_size), direction_(LeftToRight) {
  SetOverflowTriggersScrollbar(inline_triggers, block_triggers);
  SetFixedSize(fixed_inline, fixed_block);
  SetFragmentationType(type);
}

}  // namespace blink
