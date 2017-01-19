// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    PassRefPtr<const ComputedStyle> container_style,
    NGLogicalSize container_size) {
  NGWritingMode writing_mode(
      FromPlatformWritingMode(container_style->getWritingMode()));

  NGBoxStrut borders = ComputeBorders(*container_style);
  parent_border_offset_ =
      NGLogicalOffset{borders.inline_start, borders.block_start};
  parent_border_physical_offset_ = parent_border_offset_.ConvertToPhysical(
      writing_mode, container_style->direction(),
      container_size.ConvertToPhysical(writing_mode), NGPhysicalSize());

  NGLogicalSize space_size = container_size;
  space_size.block_size -= borders.BlockSum();
  space_size.inline_size -= borders.InlineSum();

  // Initialize ConstraintSpace
  NGConstraintSpaceBuilder space_builder(writing_mode);
  space_builder.SetAvailableSize(space_size);
  space_builder.SetPercentageResolutionSize(space_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetTextDirection(container_style->direction());
  parent_space_ = space_builder.ToConstraintSpace();
}

void NGOutOfFlowLayoutPart::Layout(NGBlockNode& node,
                                   NGStaticPosition static_position,
                                   NGFragment** fragment_out,
                                   NGLogicalOffset* offset) {
  // Adjust the static_position origin. The static_position coordinate origin is
  // relative to the parent's border box, ng_absolute_utils expects it to be
  // relative to the parent's padding box.
  static_position.offset -= parent_border_physical_offset_;

  NGFragment* fragment = nullptr;
  Optional<MinAndMaxContentSizes> inline_estimate;
  Optional<LayoutUnit> block_estimate;

  if (AbsoluteNeedsChildInlineSize(*node.Style())) {
    inline_estimate = node.ComputeMinAndMaxContentSizesSync();
  }

  NGAbsolutePhysicalPosition node_position =
      ComputePartialAbsoluteWithChildInlineSize(
          *parent_space_, *node.Style(), static_position, inline_estimate);

  if (AbsoluteNeedsChildBlockSize(*node.Style())) {
    fragment = GenerateFragment(node, block_estimate, node_position);
    block_estimate = fragment->BlockSize();
  }

  ComputeFullAbsoluteWithChildBlockSize(*parent_space_, *node.Style(),
                                        static_position, block_estimate,
                                        &node_position);

  // Skip this step if we produced a fragment when estimating the block size.
  if (!fragment) {
    block_estimate =
        node_position.size.ConvertToLogical(parent_space_->WritingMode())
            .block_size;
    fragment = GenerateFragment(node, block_estimate, node_position);
  }

  *fragment_out = fragment;

  // Compute logical offset, NGAbsolutePhysicalPosition is calculated relative
  // to the padding box so add back the parent's borders.
  NGBoxStrut inset = node_position.inset.ConvertToLogical(
      parent_space_->WritingMode(), parent_space_->Direction());
  offset->inline_offset =
      inset.inline_start + parent_border_offset_.inline_offset;
  offset->block_offset = inset.block_start + parent_border_offset_.block_offset;
}

NGFragment* NGOutOfFlowLayoutPart::GenerateFragment(
    NGBlockNode& node,
    const Optional<LayoutUnit>& block_estimate,
    const NGAbsolutePhysicalPosition node_position) {
  // The fragment is generated in one of these two scenarios:
  // 1. To estimate child's block size, in this case block_size is parent's
  //    available size.
  // 2. To compute final fragment, when block size is known from the absolute
  //    position calculation.
  LayoutUnit inline_size =
      node_position.size.ConvertToLogical(parent_space_->WritingMode())
          .inline_size;
  LayoutUnit block_size = block_estimate
                              ? *block_estimate
                              : parent_space_->AvailableSize().block_size;

  NGLogicalSize available_size{inline_size, block_size};

  NGConstraintSpaceBuilder builder(parent_space_->WritingMode());
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(parent_space_->AvailableSize());
  if (block_estimate)
    builder.SetIsFixedSizeBlock(true);
  builder.SetIsFixedSizeInline(true);
  builder.SetIsNewFormattingContext(true);
  NGConstraintSpace* space = builder.ToConstraintSpace();

  NGFragment* fragment;
  node.LayoutSync(space, &fragment);
  return fragment;
}

DEFINE_TRACE(NGOutOfFlowLayoutPart) {
  visitor->trace(parent_space_);
}

}  // namespace blink
