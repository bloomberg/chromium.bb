// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/BaselineAlignment.h"

namespace blink {

BaselineGroup::BaselineGroup(WritingMode block_flow,
                             ItemPosition child_preference)
    : max_ascent_(0), max_descent_(0), items_() {
  block_flow_ = block_flow;
  preference_ = child_preference;
}

void BaselineGroup::Update(const LayoutBox& child,
                           LayoutUnit ascent,
                           LayoutUnit descent) {
  if (items_.insert(&child).is_new_entry) {
    max_ascent_ = std::max(max_ascent_, ascent);
    max_descent_ = std::max(max_descent_, descent);
  }
}

bool BaselineGroup::IsOppositeBlockFlow(WritingMode block_flow) const {
  switch (block_flow) {
    case WritingMode::kHorizontalTb:
      return false;
    case WritingMode::kVerticalLr:
      return block_flow_ == WritingMode::kVerticalRl;
    case WritingMode::kVerticalRl:
      return block_flow_ == WritingMode::kVerticalLr;
    default:
      NOTREACHED();
      return false;
  }
}

bool BaselineGroup::IsOrthogonalBlockFlow(WritingMode block_flow) const {
  switch (block_flow) {
    case WritingMode::kHorizontalTb:
      return block_flow_ != WritingMode::kHorizontalTb;
    case WritingMode::kVerticalLr:
    case WritingMode::kVerticalRl:
      return block_flow_ == WritingMode::kHorizontalTb;
    default:
      NOTREACHED();
      return false;
  }
}

bool BaselineGroup::IsCompatible(WritingMode child_block_flow,
                                 ItemPosition child_preference) const {
  DCHECK(IsBaselinePosition(child_preference));
  DCHECK_GT(size(), 0);
  return ((block_flow_ == child_block_flow ||
           IsOrthogonalBlockFlow(child_block_flow)) &&
          preference_ == child_preference) ||
         (IsOppositeBlockFlow(child_block_flow) &&
          preference_ != child_preference);
}

BaselineContext::BaselineContext(const LayoutBox& child,
                                 ItemPosition preference,
                                 LayoutUnit ascent,
                                 LayoutUnit descent) {
  DCHECK(IsBaselinePosition(preference));
  UpdateSharedGroup(child, preference, ascent, descent);
}

const BaselineGroup& BaselineContext::GetSharedGroup(
    const LayoutBox& child,
    ItemPosition preference) const {
  DCHECK(IsBaselinePosition(preference));
  return const_cast<BaselineContext*>(this)->FindCompatibleSharedGroup(
      child, preference);
}

void BaselineContext::UpdateSharedGroup(const LayoutBox& child,
                                        ItemPosition preference,
                                        LayoutUnit ascent,
                                        LayoutUnit descent) {
  DCHECK(IsBaselinePosition(preference));
  BaselineGroup& group = FindCompatibleSharedGroup(child, preference);
  group.Update(child, ascent, descent);
}

// TODO Properly implement baseline-group compatibility
// See https://github.com/w3c/csswg-drafts/issues/721
BaselineGroup& BaselineContext::FindCompatibleSharedGroup(
    const LayoutBox& child,
    ItemPosition preference) {
  WritingMode block_direction = child.StyleRef().GetWritingMode();
  for (auto& group : shared_groups_) {
    if (group.IsCompatible(block_direction, preference))
      return group;
  }
  shared_groups_.push_front(BaselineGroup(block_direction, preference));
  return shared_groups_[0];
}

}  // namespace blink
