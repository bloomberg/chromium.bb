// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(
    const NGConstraintSpace& parent_space)
    : NGConstraintSpaceBuilder(parent_space.GetWritingMode(),
                               parent_space.InitialContainingBlockSize()) {
  parent_percentage_resolution_size_ = parent_space.PercentageResolutionSize();

  flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
  if (parent_space.IsIntermediateLayout())
    flags_ |= NGConstraintSpace::kIntermediateLayout;
}

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(WritingMode writing_mode,
                                                   NGPhysicalSize icb_size)
    : initial_containing_block_size_(icb_size),
      parent_writing_mode_(writing_mode) {
  flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::AddBaselineRequest(
    const NGBaselineRequest& request) {
  for (const auto& existing : baseline_requests_) {
    if (existing == request)
      return *this;
  }
  baseline_requests_.push_back(request);
  return *this;
}

}  // namespace blink
