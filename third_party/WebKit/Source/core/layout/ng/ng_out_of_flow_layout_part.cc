// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    const NGBlockNode container,
    const NGConstraintSpace& container_space,
    const ComputedStyle& container_style,
    NGFragmentBuilder* container_builder)
    : container_style_(container_style),
      container_builder_(container_builder),
      contains_absolute_(container.IsAbsoluteContainer()),
      contains_fixed_(container.IsFixedContainer()) {
  NGWritingMode writing_mode(
      FromPlatformWritingMode(container_style_.GetWritingMode()));

  NGBoxStrut borders = ComputeBorders(container_space, container_style_);
  NGBoxStrut scrollers = container.GetScrollbarSizes();
  NGBoxStrut borders_and_scrollers = borders + scrollers;
  content_offset_ = NGLogicalOffset{borders_and_scrollers.inline_start,
                                    borders_and_scrollers.block_start};

  NGPhysicalBoxStrut physical_borders = borders_and_scrollers.ConvertToPhysical(
      writing_mode, container_style_.Direction());
  content_physical_offset_ =
      NGPhysicalOffset(physical_borders.left, physical_borders.top);

  container_size_ = container_builder_->Size();
  container_size_.inline_size -= borders_and_scrollers.InlineSum();
  container_size_.block_size -= borders_and_scrollers.BlockSum();

  icb_size_ = container_space.InitialContainingBlockSize();
}

void NGOutOfFlowLayoutPart::Run() {
  Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
  container_builder_->GetAndClearOutOfFlowDescendantCandidates(
      &descendant_candidates);

  while (descendant_candidates.size() > 0) {
    for (auto& candidate : descendant_candidates) {
      if (IsContainingBlockForDescendant(candidate.node.Style())) {
        NGLogicalOffset offset;
        RefPtr<NGLayoutResult> result = LayoutDescendant(
            candidate.node, candidate.static_position, &offset);
        container_builder_->AddChild(std::move(result), offset);
      } else {
        container_builder_->AddOutOfFlowDescendant(candidate);
      }
    }
    // Sweep any descendants that might have been added.
    // This happens when an absolute container has a fixed child.
    descendant_candidates.clear();
    container_builder_->GetAndClearOutOfFlowDescendantCandidates(
        &descendant_candidates);
  }
}

RefPtr<NGLayoutResult> NGOutOfFlowLayoutPart::LayoutDescendant(
    NGBlockNode descendant,
    NGStaticPosition static_position,
    NGLogicalOffset* offset) {
  DCHECK(descendant);

  NGWritingMode container_writing_mode(
      FromPlatformWritingMode(container_style_.GetWritingMode()));
  NGWritingMode descendant_writing_mode(
      FromPlatformWritingMode(descendant.Style().GetWritingMode()));

  // Adjust the static_position origin. The static_position coordinate origin is
  // relative to the container's border box, ng_absolute_utils expects it to be
  // relative to the container's padding box.
  static_position.offset -= content_physical_offset_;

  // The block estimate is in the descendant's writing mode.
  RefPtr<NGConstraintSpace> descendant_constraint_space =
      NGConstraintSpaceBuilder(container_writing_mode, icb_size_)
          .SetTextDirection(container_style_.Direction())
          .SetAvailableSize(container_size_)
          .SetPercentageResolutionSize(container_size_)
          .ToConstraintSpace(descendant_writing_mode);
  Optional<MinMaxSize> min_max_size;
  Optional<LayoutUnit> block_estimate;

  RefPtr<NGLayoutResult> layout_result = nullptr;

  if (AbsoluteNeedsChildInlineSize(descendant.Style()) ||
      NeedMinMaxSize(descendant.Style())) {
    min_max_size = descendant.ComputeMinMaxSize();
  }

  Optional<NGLogicalSize> replaced_size;
  if (descendant.IsReplaced())
    replaced_size = ComputeReplacedSize(
        descendant, *descendant_constraint_space, min_max_size);

  NGAbsolutePhysicalPosition node_position =
      ComputePartialAbsoluteWithChildInlineSize(
          *descendant_constraint_space, descendant.Style(), static_position,
          min_max_size, replaced_size);

  if (AbsoluteNeedsChildBlockSize(descendant.Style())) {
    layout_result = GenerateFragment(descendant, block_estimate, node_position);

    DCHECK(layout_result->PhysicalFragment().get());
    NGFragment fragment(descendant_writing_mode,
                        *layout_result->PhysicalFragment());

    block_estimate = fragment.BlockSize();
  }

  ComputeFullAbsoluteWithChildBlockSize(
      *descendant_constraint_space, descendant.Style(), static_position,
      block_estimate, replaced_size, &node_position);

  // Skip this step if we produced a fragment when estimating the block size.
  if (!layout_result) {
    block_estimate =
        node_position.size.ConvertToLogical(descendant_writing_mode).block_size;
    layout_result = GenerateFragment(descendant, block_estimate, node_position);
  }

  // Compute logical offset, NGAbsolutePhysicalPosition is calculated relative
  // to the padding box so add back the container's borders.
  NGBoxStrut inset = node_position.inset.ConvertToLogical(
      container_writing_mode, container_style_.Direction());
  offset->inline_offset = inset.inline_start + content_offset_.inline_offset;
  offset->block_offset = inset.block_start + content_offset_.block_offset;

  return layout_result;
}

bool NGOutOfFlowLayoutPart::IsContainingBlockForDescendant(
    const ComputedStyle& descendant_style) {
  EPosition position = descendant_style.GetPosition();
  return (contains_absolute_ && position == EPosition::kAbsolute) ||
         (contains_fixed_ && position == EPosition::kFixed);
}

// The fragment is generated in one of these two scenarios:
// 1. To estimate descendant's block size, in this case block_size is
//    container's available size.
// 2. To compute final fragment, when block size is known from the absolute
//    position calculation.
RefPtr<NGLayoutResult> NGOutOfFlowLayoutPart::GenerateFragment(
    NGBlockNode descendant,
    const Optional<LayoutUnit>& block_estimate,
    const NGAbsolutePhysicalPosition node_position) {
  // As the block_estimate is always in the descendant's writing mode, we build
  // the constraint space in the descendant's writing mode.
  NGWritingMode writing_mode(
      FromPlatformWritingMode(descendant.Style().GetWritingMode()));
  NGLogicalSize container_size(container_size_
                                   .ConvertToPhysical(FromPlatformWritingMode(
                                       container_style_.GetWritingMode()))
                                   .ConvertToLogical(writing_mode));

  LayoutUnit inline_size =
      node_position.size.ConvertToLogical(writing_mode).inline_size;
  LayoutUnit block_size =
      block_estimate ? *block_estimate : container_size.block_size;

  NGLogicalSize available_size{inline_size, block_size};

  // TODO(atotic) will need to be adjusted for scrollbars.
  NGConstraintSpaceBuilder builder(writing_mode, icb_size_);
  builder.SetAvailableSize(available_size)
      .SetTextDirection(descendant.Style().Direction())
      .SetPercentageResolutionSize(container_size)
      .SetIsNewFormattingContext(true)
      .SetIsFixedSizeInline(true);
  if (block_estimate)
    builder.SetIsFixedSizeBlock(true);
  RefPtr<NGConstraintSpace> space = builder.ToConstraintSpace(writing_mode);

  return descendant.Layout(*space);
}

}  // namespace blink
