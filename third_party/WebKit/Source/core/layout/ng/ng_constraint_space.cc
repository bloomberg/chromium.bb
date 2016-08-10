// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBox.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(LayoutUnit inlineContainerSize,
                                     LayoutUnit blockContainerSize) {
  m_inlineContainerSize = inlineContainerSize;
  m_blockContainerSize = blockContainerSize;
  m_inlineTriggersScrollbar = 0;
  m_blockTriggersScrollbar = 0;
  m_fixedInlineSize = 0;
  m_fixedBlockSize = 0;
  m_blockFragmentationType = FragmentNone;
}

NGConstraintSpace NGConstraintSpace::fromLayoutObject(const LayoutBox& child) {
  bool fixedInline = false, fixedBlock = false;
  // XXX for orthogonal writing mode this is not right
  LayoutUnit containerLogicalWidth =
      std::max(LayoutUnit(), child.containingBlockLogicalWidthForContent());
  // XXX Make sure this height is correct
  LayoutUnit containerLogicalHeight =
      child.containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
  if (child.hasOverrideLogicalContentWidth()) {
    containerLogicalWidth = child.overrideLogicalContentWidth();
    fixedInline = true;
  }
  if (child.hasOverrideLogicalContentHeight()) {
    containerLogicalWidth = child.overrideLogicalContentHeight();
    fixedBlock = true;
  }
  NGConstraintSpace space(containerLogicalWidth, containerLogicalHeight);
  // XXX vertical writing mode
  space.setOverflowTriggersScrollbar(
      child.styleRef().overflowX() == OverflowAuto,
      child.styleRef().overflowY() == OverflowAuto);
  space.setFixedSize(fixedInline, fixedBlock);
  return space;
}

void NGConstraintSpace::addExclusion(const NGExclusion exclusion,
                                     unsigned options) {}

void NGConstraintSpace::setOverflowTriggersScrollbar(bool inlineTriggers,
                                                     bool blockTriggers) {
  m_inlineTriggersScrollbar = inlineTriggers;
  m_blockTriggersScrollbar = blockTriggers;
}

void NGConstraintSpace::setFixedSize(bool inlineFixed, bool blockFixed) {
  m_fixedInlineSize = inlineFixed;
  m_fixedBlockSize = blockFixed;
}

void NGConstraintSpace::setFragmentationType(NGFragmentationType type) {
  m_blockFragmentationType = type;
}

DoublyLinkedList<const NGExclusion> NGConstraintSpace::exclusions(
    unsigned options) const {
  DoublyLinkedList<const NGExclusion> exclusions;
  // TODO(eae): Implement.
  return exclusions;
}

NGLayoutOpportunityIterator NGConstraintSpace::layoutOpportunities(
    unsigned clear,
    NGExclusionFlowType avoid) const {
  // TODO(eae): Implement.
  NGLayoutOpportunityIterator iterator(this, clear, avoid);
  return iterator;
}

}  // namespace blink
