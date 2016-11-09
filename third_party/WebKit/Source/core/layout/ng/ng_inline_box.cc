// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_box.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_text_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_direction.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

NGInlineBox::NGInlineBox(LayoutObject* start_inline)
    : start_inline_(start_inline) {
  DCHECK(start_inline);

  for (LayoutObject* curr = start_inline_;; curr = curr->nextSibling()) {
    if (curr->isAtomicInlineLevel() && !curr->isFloating() &&
        !curr->isOutOfFlowPositioned())
      break;
    last_inline_ = curr;
  }
}

bool NGInlineBox::Layout(const NGConstraintSpace* constraint_space,
                         NGFragmentBase** out) {
  // TODO(layout-dev): Perform pre-layout text step.

  // NOTE: We don't need to change the coordinate system here as we are an
  // inline.
  NGConstraintSpace* child_constraint_space = new NGConstraintSpace(
      constraint_space->WritingMode(), constraint_space->Direction(),
      constraint_space->MutablePhysicalSpace());

  if (!layout_algorithm_)
    // TODO(layout-dev): If an atomic inline run the appropriate algorithm.
    layout_algorithm_ = new NGTextLayoutAlgorithm(this, child_constraint_space);

  NGPhysicalFragmentBase* fragment = nullptr;
  if (!layout_algorithm_->Layout(&fragment))
    return false;

  // TODO(layout-dev): Implement copying of fragment data to LayoutObject tree.

  *out = new NGTextFragment(constraint_space->WritingMode(),
                            constraint_space->Direction(),
                            toNGPhysicalTextFragment(fragment));

  // Reset algorithm for future use
  layout_algorithm_ = nullptr;
  return true;
}

NGInlineBox* NGInlineBox::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling =
        last_inline_ ? last_inline_->nextSibling() : nullptr;
    next_sibling_ = next_sibling ? new NGInlineBox(next_sibling) : nullptr;
  }
  return next_sibling_;
}

DEFINE_TRACE(NGInlineBox) {
  visitor->trace(next_sibling_);
  visitor->trace(layout_algorithm_);
}

}  // namespace blink
