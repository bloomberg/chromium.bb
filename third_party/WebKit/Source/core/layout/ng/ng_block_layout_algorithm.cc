// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

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
    : style_(style),
      first_child_(first_child),
      state_(kStateInit),
      is_fragment_margin_strut_block_start_updated_(false) {
  DCHECK(style_);
}

bool NGBlockLayoutAlgorithm::Layout(const NGConstraintSpace* constraint_space,
                                    NGPhysicalFragment** out) {
  switch (state_) {
    case kStateInit: {
      border_and_padding_ =
          ComputeBorders(*style_) + ComputePadding(*constraint_space, *style_);

      LayoutUnit inline_size =
          ComputeInlineSizeForFragment(*constraint_space, *style_);
      LayoutUnit adjusted_inline_size =
          inline_size - border_and_padding_.InlineSum();
      // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
      // -1?
      LayoutUnit block_size = ComputeBlockSizeForFragment(
          *constraint_space, *style_, NGSizeIndefinite);
      LayoutUnit adjusted_block_size(block_size);
      // Our calculated block-axis size may be indefinite at this point.
      // If so, just leave the size as NGSizeIndefinite instead of subtracting
      // borders and padding.
      if (adjusted_block_size != NGSizeIndefinite)
        adjusted_block_size -= border_and_padding_.BlockSum();
      constraint_space_for_children_ = new NGConstraintSpace(
          FromPlatformWritingMode(style_->getWritingMode()),
          FromPlatformDirection(style_->direction()), *constraint_space,
          NGLogicalSize(adjusted_inline_size, adjusted_block_size));
      content_size_ = border_and_padding_.block_start;

      builder_ = new NGFragmentBuilder(NGPhysicalFragmentBase::FragmentBox);
      builder_->SetDirection(constraint_space->Direction());
      builder_->SetWritingMode(constraint_space->WritingMode());
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
        NGBoxStrut child_margins = ComputeMargins(
            *constraint_space_for_children_, *current_child_->Style(),
            constraint_space_for_children_->WritingMode(),
            constraint_space_for_children_->Direction());
        ApplyAutoMargins(*constraint_space_for_children_,
                         *current_child_->Style(), *fragment, child_margins);

        const NGBoxStrut margins =
            CollapseMargins(*constraint_space, child_margins, *fragment);

        // TODO(layout-ng): Support auto margins
        builder_->AddChild(
            fragment, NGLogicalOffset(border_and_padding_.inline_start +
                                          child_margins.inline_start,
                                      content_size_ + margins.block_start));

        content_size_ += fragment->BlockSize() + margins.BlockSum();
        max_inline_size_ =
            std::max(max_inline_size_, fragment->InlineSize() +
                                           child_margins.InlineSum() +
                                           border_and_padding_.InlineSum());
        current_child_ = current_child_->NextSibling();
        if (current_child_)
          return false;
      }
      state_ = kStateFinalize;
      return false;
    }
    case kStateFinalize: {
      content_size_ += border_and_padding_.block_end;

      // Recompute the block-axis size now that we know our content size.
      LayoutUnit block_size = ComputeBlockSizeForFragment(
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

NGBoxStrut NGBlockLayoutAlgorithm::CollapseMargins(
    const NGConstraintSpace& space,
    const NGBoxStrut& margins,
    const NGFragment& fragment) {
  bool is_zero_height_box = !fragment.BlockSize() && margins.IsEmpty() &&
                            fragment.MarginStrut().IsEmpty();
  // Create the current child's margin strut from its children's margin strut or
  // use margin strut from the the last non-empty child.
  NGMarginStrut curr_margin_strut =
      is_zero_height_box ? prev_child_margin_strut_ : fragment.MarginStrut();

  // Calculate borders and padding for the current child.
  NGBoxStrut border_and_padding =
      ComputeBorders(*current_child_->Style()) +
      ComputePadding(space, *current_child_->Style());

  // Collapse BLOCK-START margins if there is no padding or border between
  // parent (current child) and its first in-flow child.
  if (border_and_padding.block_start) {
    curr_margin_strut.SetMarginBlockStart(margins.block_start);
  } else {
    curr_margin_strut.AppendMarginBlockStart(margins.block_start);
  }

  // Collapse BLOCK-END margins if
  // 1) there is no padding or border between parent (current child) and its
  //    first/last in-flow child
  // 2) parent's logical height is auto.
  if (current_child_->Style()->logicalHeight().isAuto() &&
      !border_and_padding.block_end) {
    curr_margin_strut.AppendMarginBlockEnd(margins.block_end);
  } else {
    curr_margin_strut.SetMarginBlockEnd(margins.block_end);
  }

  NGBoxStrut result_margins;
  // Margins of the newly established formatting context do not participate
  // in Collapsing Margins:
  // - Compute margins block start for adjoining blocks *including* 1st block.
  // - Compute margins block end for the last block.
  // - Do not set the computed margins to the parent fragment.
  if (space.IsNewFormattingContext()) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
    bool is_last_child = !current_child_->NextSibling();
    if (is_last_child)
      result_margins.block_end = curr_margin_strut.BlockEndSum();
    return result_margins;
  }

  // Zero-height boxes are ignored and do not participate in margin collapsing.
  if (is_zero_height_box)
    return result_margins;

  // Compute the margin block start for adjoining blocks *excluding* 1st block
  if (is_fragment_margin_strut_block_start_updated_) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
  }

  // Update the parent fragment's margin strut
  UpdateMarginStrut(curr_margin_strut);

  prev_child_margin_strut_ = curr_margin_strut;
  return result_margins;
}

void NGBlockLayoutAlgorithm::UpdateMarginStrut(const NGMarginStrut& from) {
  if (!is_fragment_margin_strut_block_start_updated_) {
    builder_->SetMarginStrutBlockStart(from);
    is_fragment_margin_strut_block_start_updated_ = true;
  }
  builder_->SetMarginStrutBlockEnd(from);
}

}  // namespace blink
