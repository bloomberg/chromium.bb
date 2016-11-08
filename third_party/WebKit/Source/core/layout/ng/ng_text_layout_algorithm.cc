// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_layout_algorithm.h"

#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

NGTextLayoutAlgorithm::NGTextLayoutAlgorithm(
    NGInlineBox* inline_box,
    NGConstraintSpace* constraint_space)
    : inline_box_(inline_box), constraint_space_(constraint_space) {
  DCHECK(inline_box_);
}

bool NGTextLayoutAlgorithm::Layout(NGPhysicalFragment** out) {
  // TODO(layout-dev): implement.
  *out = nullptr;
  return true;
}

DEFINE_TRACE(NGTextLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(inline_box_);
  visitor->trace(constraint_space_);
}

}  // namespace blink
