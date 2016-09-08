// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {
namespace {

LayoutUnit ComputeCollapsedMarginBlockStart(
    const NGMarginStrut& prev_margin_strut,
    const NGMarginStrut& curr_margin_strut) {
  return std::max(prev_margin_strut.margin_block_end,
                  curr_margin_strut.margin_block_start) -
         std::max(prev_margin_strut.negative_margin_block_end.abs(),
                  curr_margin_strut.negative_margin_block_start.abs());
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBox* first_child)
    : style_(style), first_child_(first_child), state_(kStateInit) {
  DCHECK(style_);
}

bool NGBlockLayoutAlgorithm::Layout(const NGConstraintSpace* constraint_space,
                                    NGPhysicalFragment** out) {
  switch (state_) {
    case kStateInit: {
      LayoutUnit inline_size =
          computeInlineSizeForFragment(*constraint_space, *style_);
      // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
      // -1?
      LayoutUnit block_size = computeBlockSizeForFragment(
          *constraint_space, *style_, LayoutUnit(-1));
      constraint_space_for_children_ =
          new NGConstraintSpace(*constraint_space, NGLogicalOffset(),
                                NGLogicalSize(inline_size, block_size));
      content_size_ = LayoutUnit();

      builder_ = new NGFragmentBuilder(NGPhysicalFragmentBase::FragmentBox);
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

        LayoutUnit margin_block_start =
            CollapseMargins(child_margins, fragment->MarginStrut());

        // TODO(layout-ng): Support auto margins
        builder_->AddChild(fragment,
                           NGLogicalOffset(child_margins.inline_start,
                                           content_size_ + margin_block_start));

        content_size_ += fragment->BlockSize() + margin_block_start;
        max_inline_size_ =
            std::max(max_inline_size_,
                     fragment->InlineSize() + child_margins.InlineSum());
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

LayoutUnit NGBlockLayoutAlgorithm::CollapseMargins(
    const NGBoxStrut& margins,
    const NGMarginStrut& children_margin_strut) {
  // Calculate margin strut for the current child.
  NGMarginStrut curr_margin_strut = children_margin_strut;
  curr_margin_strut.AppendMarginBlockStart(margins.block_start);
  curr_margin_strut.AppendMarginBlockEnd(margins.block_end);

  // Set the margin strut for the resultant fragment if this is the first or
  // last child fragment.
  if (current_child_ == first_child_)
    builder_->SetMarginStrutBlockStart(curr_margin_strut);
  if (!current_child_->NextSibling())
    builder_->SetMarginStrutBlockEnd(curr_margin_strut);

  // Compute the margin block start for adjoining blocks.
  LayoutUnit margin_block_start;
  if (current_child_ != first_child_)
    margin_block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);

  prev_child_margin_strut_ = curr_margin_strut;
  // TODO(layout-ng): support other Margin Collapsing use cases,
  // i.e. support 0 height elements etc.
  return margin_block_start;
}

}  // namespace blink
