// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_exclusion_space.h"

#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"

namespace blink {

NGExclusionSpace::NGExclusionSpace()
    : last_float_block_start_(LayoutUnit::Min()),
      left_float_clear_offset_(LayoutUnit::Min()),
      right_float_clear_offset_(LayoutUnit::Min()),
      has_left_float_(false),
      has_right_float_(false) {}

void NGExclusionSpace::Add(const NGExclusion& exclusion) {
  storage_.push_back(exclusion);
  last_float_block_start_ =
      std::max(last_float_block_start_, exclusion.rect.BlockStartOffset());

  if (exclusion.type == NGExclusion::kFloatLeft) {
    has_left_float_ = true;
    left_float_clear_offset_ =
        std::max(left_float_clear_offset_, exclusion.rect.BlockEndOffset());
  } else if (exclusion.type == NGExclusion::kFloatRight) {
    has_right_float_ = true;
    right_float_clear_offset_ =
        std::max(right_float_clear_offset_, exclusion.rect.BlockEndOffset());
  }
}

NGLayoutOpportunity NGExclusionSpace::FindLayoutOpportunity(
    const NGLogicalOffset& offset,
    const NGLogicalSize& available_size,
    const NGLogicalSize& minimum_size) const {
  NGLayoutOpportunityIterator opportunity_iter(this, available_size, offset);
  NGLayoutOpportunity opportunity;

  while (true) {
    opportunity = opportunity_iter.Next();
    if (opportunity_iter.IsAtEnd() ||
        (opportunity.size.inline_size >= minimum_size.inline_size &&
         opportunity.size.block_size >= minimum_size.block_size))
      return opportunity;
  }

  NOTREACHED();
  return NGLayoutOpportunity();
}

LayoutUnit NGExclusionSpace::ClearanceOffset(EClear clear_type) const {
  switch (clear_type) {
    case EClear::kNone:
      return LayoutUnit::Min();  // nothing to do here.
    case EClear::kLeft:
      return left_float_clear_offset_;
    case EClear::kRight:
      return right_float_clear_offset_;
    case EClear::kBoth:
      return std::max(left_float_clear_offset_, right_float_clear_offset_);
    default:
      NOTREACHED();
  }

  return LayoutUnit::Min();
}

bool NGExclusionSpace::operator==(const NGExclusionSpace& other) const {
  return storage_ == other.storage_;
}

}  // namespace blink
