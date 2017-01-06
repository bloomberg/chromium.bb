// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_legacy_block_layout_algorithm.h"

#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGLegacyBlockLayoutAlgorithm::NGLegacyBlockLayoutAlgorithm(
    NGBlockNode* block,
    const NGConstraintSpace* constraint_space)
    : NGLayoutAlgorithm(kLegacyBlockLayoutAlgorithm),
      block_(block),
      constraint_space_(constraint_space) {}

NGLayoutStatus NGLegacyBlockLayoutAlgorithm::Layout(
    NGPhysicalFragment*,
    NGPhysicalFragment** fragment_out,
    NGLayoutAlgorithm**) {
  *fragment_out = block_->RunOldLayout(*constraint_space_);
  return kNewFragment;
}

DEFINE_TRACE(NGLegacyBlockLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(block_);
  visitor->trace(constraint_space_);
}
}
