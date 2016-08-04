// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/NGConstraintSpace.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(LayoutUnit inlineContainerSize,
    LayoutUnit blockContainerSize)
{
    m_inlineContainerSize = inlineContainerSize;
    m_blockContainerSize = blockContainerSize;
    m_inlineTriggersScrollbar = 0;
    m_blockTriggersScrollbar = 0;
    m_fixedInlineSize = 0;
    m_fixedBlockSize = 0;
    m_blockFragmentationType = FragmentNone;
}

void NGConstraintSpace::addExclusion(const NGExclusion exclusion,
    unsigned options)
{

}

void NGConstraintSpace::setOverflowTriggersScrollbar(bool inlineTriggers,
    bool blockTriggers)
{
    m_inlineTriggersScrollbar = inlineTriggers;
    m_blockTriggersScrollbar = blockTriggers;
}

void NGConstraintSpace::setFixedSize(bool inlineFixed, bool blockFixed)
{
    m_fixedInlineSize = inlineFixed;
    m_fixedBlockSize = blockFixed;
}

void NGConstraintSpace::setFragmentationType(NGFragmentationType type)
{
    m_blockFragmentationType = type;
}

DoublyLinkedList<const NGExclusion> NGConstraintSpace::exclusions(
    unsigned options) const
{
    DoublyLinkedList<const NGExclusion> exclusions;
    // TODO(eae): Implement.
    return exclusions;
}

NGLayoutOpportunityIterator NGConstraintSpace::layoutOpportunities(
    unsigned clear, NGExclusionFlowType avoid) const
{
    // TODO(eae): Implement.
    NGLayoutOpportunityIterator iterator(this, clear, avoid);
    return iterator;
}

} // namespace blink
