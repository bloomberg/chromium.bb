// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBox* first_child)
    : style_(style), first_child_(first_child), state_(kStateInit) {}

bool NGBlockLayoutAlgorithm::Layout(const NGConstraintSpace* constraint_space,
                                    NGFragment** out) {
  switch (state_) {
    case kStateInit: {
      LayoutUnit inline_size =
          computeInlineSizeForFragment(*constraint_space, *style_);
      // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
      // -1?
      LayoutUnit block_size = computeBlockSizeForFragment(
          *constraint_space, *style_, LayoutUnit(-1));
      constraint_space_for_children_ = new NGConstraintSpace(
          *constraint_space, NGLogicalSize(inline_size, block_size));
      content_size_ = LayoutUnit();

      builder_ = new NGFragmentBuilder(NGFragmentBase::FragmentBox);
      builder_->SetInlineSize(inline_size).SetBlockSize(block_size);
      current_child_ = first_child_;
      state_ = kStateChildLayout;
      return false;
    }
    case kStateChildLayout: {
      if (current_child_) {
        NGFragment* fragment;
        if (!current_child_->Layout(constraint_space_for_children_, &fragment))
          return false;
        NGBoxStrut child_margins = computeMargins(
            *constraint_space_for_children_, *current_child_->Style());
        // TODO(layout-ng): Support auto margins
        fragment->SetOffset(child_margins.inline_start,
                            content_size_ + child_margins.block_start);
        current_child_->PositionUpdated(*fragment);
        content_size_ += fragment->BlockSize() + child_margins.BlockSum();
        max_inline_size_ =
            std::max(max_inline_size_,
                     fragment->InlineSize() + child_margins.InlineSum());
        builder_->AddChild(fragment);
        current_child_ = current_child_->NextSibling();
        if (current_child_)
          return false;
      }

      state_ = kStateFinalize;
      return false;
    }
    case kStateFinalize: {
      // Recompute the block-axis size now that we know our content size.
      LayoutUnit block_size = computeBlockSizeForFragment(
          *constraint_space, *style_, content_size_);

      builder_->SetBlockSize(block_size)
          .SetInlineOverflow(max_inline_size_)
          .SetBlockOverflow(content_size_);
      *out = builder_->ToFragment();
      state_ = kStateInit;
      return true;
    }
  };
  NOTREACHED();
  *out = nullptr;
  return true;
}

}  // namespace blink
