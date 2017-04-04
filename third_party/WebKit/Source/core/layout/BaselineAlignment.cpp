// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/BaselineAlignment.h"

namespace blink {

BaselineGroup::BaselineGroup(WritingMode blockFlow,
                             ItemPosition childPreference)
    : m_maxAscent(0), m_maxDescent(0), m_items() {
  m_blockFlow = blockFlow;
  m_preference = childPreference;
}

void BaselineGroup::update(const LayoutBox& child,
                           LayoutUnit ascent,
                           LayoutUnit descent) {
  if (m_items.insert(&child).isNewEntry) {
    m_maxAscent = std::max(m_maxAscent, ascent);
    m_maxDescent = std::max(m_maxDescent, descent);
  }
}

bool BaselineGroup::isOppositeBlockFlow(WritingMode blockFlow) const {
  switch (blockFlow) {
    case WritingMode::kHorizontalTb:
      return false;
    case WritingMode::kVerticalLr:
      return m_blockFlow == WritingMode::kVerticalRl;
    case WritingMode::kVerticalRl:
      return m_blockFlow == WritingMode::kVerticalLr;
    default:
      NOTREACHED();
      return false;
  }
}

bool BaselineGroup::isOrthogonalBlockFlow(WritingMode blockFlow) const {
  switch (blockFlow) {
    case WritingMode::kHorizontalTb:
      return m_blockFlow != WritingMode::kHorizontalTb;
    case WritingMode::kVerticalLr:
    case WritingMode::kVerticalRl:
      return m_blockFlow == WritingMode::kHorizontalTb;
    default:
      NOTREACHED();
      return false;
  }
}

bool BaselineGroup::isCompatible(WritingMode childBlockFlow,
                                 ItemPosition childPreference) const {
  DCHECK(isBaselinePosition(childPreference));
  DCHECK_GT(size(), 0);
  return ((m_blockFlow == childBlockFlow ||
           isOrthogonalBlockFlow(childBlockFlow)) &&
          m_preference == childPreference) ||
         (isOppositeBlockFlow(childBlockFlow) &&
          m_preference != childPreference);
}

BaselineContext::BaselineContext(const LayoutBox& child,
                                 ItemPosition preference,
                                 LayoutUnit ascent,
                                 LayoutUnit descent) {
  DCHECK(isBaselinePosition(preference));
  updateSharedGroup(child, preference, ascent, descent);
}

const BaselineGroup& BaselineContext::getSharedGroup(
    const LayoutBox& child,
    ItemPosition preference) const {
  DCHECK(isBaselinePosition(preference));
  return const_cast<BaselineContext*>(this)->findCompatibleSharedGroup(
      child, preference);
}

void BaselineContext::updateSharedGroup(const LayoutBox& child,
                                        ItemPosition preference,
                                        LayoutUnit ascent,
                                        LayoutUnit descent) {
  DCHECK(isBaselinePosition(preference));
  BaselineGroup& group = findCompatibleSharedGroup(child, preference);
  group.update(child, ascent, descent);
}

// TODO Properly implement baseline-group compatibility
// See https://github.com/w3c/csswg-drafts/issues/721
BaselineGroup& BaselineContext::findCompatibleSharedGroup(
    const LayoutBox& child,
    ItemPosition preference) {
  WritingMode blockDirection = child.styleRef().getWritingMode();
  for (auto& group : m_sharedGroups) {
    if (group.isCompatible(blockDirection, preference))
      return group;
  }
  m_sharedGroups.push_front(BaselineGroup(blockDirection, preference));
  return m_sharedGroups[0];
}

}  // namespace blink
