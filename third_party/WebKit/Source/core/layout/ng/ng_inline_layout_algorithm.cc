// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_line_builder.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    LayoutObject* layout_object,
    PassRefPtr<const ComputedStyle> style,
    NGInlineNode* first_child,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kInlineLayoutAlgorithm),
      style_(style),
      first_child_(first_child),
      constraint_space_(constraint_space),
      break_token_(break_token),
      builder_(new NGFragmentBuilder(NGPhysicalFragment::kFragmentBox,
                                     layout_object)) {
  DCHECK(style_);
}

NGPhysicalFragment* NGInlineLayoutAlgorithm::Layout() {
  // TODO(kojii): Implement sizing and child constraint spaces. Share common
  // logic with NGBlockLayoutAlgorithm using composition.
  builder_->SetWritingMode(constraint_space_->WritingMode());
  builder_->SetDirection(constraint_space_->Direction());
  NGInlineNode* current_child = first_child_;

  Member<NGLineBuilder> line_builder;
  // TODO(kojii): Since line_builder_ is bound to NGLayoutInlineItem
  // in current_child, changing the current_child needs more work.
  if (current_child) {
    Member<NGConstraintSpace> space_for_current_child =
        CreateConstraintSpaceForChild(*current_child);
    line_builder = new NGLineBuilder(current_child, space_for_current_child);
    current_child->LayoutInline(space_for_current_child, line_builder);
  }
  line_builder->CreateFragments(builder_);
  NGPhysicalFragment* fragment = builder_->ToBoxFragment();
  line_builder->CopyFragmentDataToLayoutBlockFlow();
  return fragment;
}

NGConstraintSpace* NGInlineLayoutAlgorithm::CreateConstraintSpaceForChild(
    const NGInlineNode& child) const {
  // TODO(kojii): Implement child constraint space.
  NGConstraintSpace* child_space =
      NGConstraintSpaceBuilder(constraint_space_->WritingMode())
          .SetTextDirection(constraint_space_->Direction())
          .ToConstraintSpace();
  return child_space;
}

DEFINE_TRACE(NGInlineLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
  visitor->trace(builder_);
}

}  // namespace blink
