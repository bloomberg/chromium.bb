// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"

namespace blink {

NGTextLayoutAlgorithm::NGTextLayoutAlgorithm(
    NGInlineNode* inline_box,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kTextLayoutAlgorithm),
      inline_box_(inline_box),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(inline_box_);
}

NGLayoutStatus NGTextLayoutAlgorithm::Layout(
    NGPhysicalFragmentBase*,
    NGPhysicalFragmentBase** fragment_out,
    NGLayoutAlgorithm**) {
  // TODO(kojii): What kind of fragment tree do we want for line boxes/root line
  // boxes? Just text, box, or new type of fragment?
  NGFragmentBuilder root_line_box_builder(NGPhysicalFragmentBase::kFragmentBox);
  root_line_box_builder.SetWritingMode(constraint_space_->WritingMode());
  root_line_box_builder.SetDirection(constraint_space_->Direction());
  unsigned start = 0;
  do {
    NGFragmentBuilder line_box_builder(NGPhysicalFragmentBase::kFragmentBox);
    start =
        inline_box_->CreateLine(start, constraint_space_, &line_box_builder);
    root_line_box_builder.AddChild(
        new NGFragment(constraint_space_->WritingMode(),
                       constraint_space_->Direction(),
                       line_box_builder.ToFragment()),
        NGLogicalOffset());
  } while (start);
  *fragment_out = root_line_box_builder.ToFragment();
  return kNewFragment;
}

DEFINE_TRACE(NGTextLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(inline_box_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
}

}  // namespace blink
