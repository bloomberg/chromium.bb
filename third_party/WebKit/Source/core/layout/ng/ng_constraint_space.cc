// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBox.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(NGLogicalSize container_size) {
  container_size_ = container_size;
  inline_triggers_scrollbar_ = 0;
  block_triggers_scrollbar_ = 0;
  fixed_inline_size_ = 0;
  fixed_block_size_ = 0;
  block_fragmentation_type_ = FragmentNone;
}

NGConstraintSpace::NGConstraintSpace(const NGConstraintSpace& other,
                                     NGLogicalSize container_size)
    : NGConstraintSpace(container_size) {
  exclusions_ = other.exclusions_;
}

NGConstraintSpace NGConstraintSpace::fromLayoutObject(const LayoutBox& child) {
  bool fixedInline = false, fixedBlock = false;
  // XXX for orthogonal writing mode this is not right
  LayoutUnit container_logical_width =
      std::max(LayoutUnit(), child.containingBlockLogicalWidthForContent());
  // XXX Make sure this height is correct
  LayoutUnit container_logical_height =
      child.containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
  if (child.hasOverrideLogicalContentWidth()) {
    container_logical_width = child.overrideLogicalContentWidth();
    fixedInline = true;
  }
  if (child.hasOverrideLogicalContentHeight()) {
    container_logical_width = child.overrideLogicalContentHeight();
    fixedBlock = true;
  }
  NGLogicalSize size;
  size.inline_size = container_logical_width;
  size.block_size = container_logical_height;
  NGConstraintSpace space(size);
  space.setOverflowTriggersScrollbar(
      child.styleRef().overflowInlineDirection() == OverflowAuto,
      child.styleRef().overflowBlockDirection() == OverflowAuto);
  space.setFixedSize(fixedInline, fixedBlock);
  return space;
}

void NGConstraintSpace::addExclusion(const NGExclusion exclusion,
                                     unsigned options) {}

void NGConstraintSpace::setOverflowTriggersScrollbar(bool inline_triggers,
                                                     bool block_triggers) {
  inline_triggers_scrollbar_ = inline_triggers;
  block_triggers_scrollbar_ = block_triggers;
}

void NGConstraintSpace::setFixedSize(bool inline_fixed, bool block_fixed) {
  fixed_inline_size_ = inline_fixed;
  fixed_block_size_ = block_fixed;
}

void NGConstraintSpace::setFragmentationType(NGFragmentationType type) {
  block_fragmentation_type_ = type;
}

DoublyLinkedList<const NGExclusion> NGConstraintSpace::exclusions(
    unsigned options) const {
  DoublyLinkedList<const NGExclusion> exclusions;
  // TODO(eae): Implement.
  return exclusions;
}

NGLayoutOpportunityIterator NGConstraintSpace::layoutOpportunities(
    unsigned clear,
    bool for_inline_or_bfc) const {
  // TODO(eae): Implement.
  NGLayoutOpportunityIterator iterator(this, clear, for_inline_or_bfc);
  return iterator;
}

}  // namespace blink
