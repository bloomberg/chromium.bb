// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_layout_algorithm.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_box.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGInlineBox* first_child,
    NGConstraintSpace* constraint_space)
    : style_(style),
      first_child_(first_child),
      constraint_space_(constraint_space) {
  DCHECK(style_);
}

bool NGInlineLayoutAlgorithm::Layout(NGPhysicalFragment** out) {
  NGFragmentBuilder builder(NGPhysicalFragmentBase::FragmentBox);

  *out = builder.ToFragment();
  return true;
}

DEFINE_TRACE(NGInlineLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(constraint_space_);
}

}  // namespace blink
