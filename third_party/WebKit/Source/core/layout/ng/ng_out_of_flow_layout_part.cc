// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/style/ComputedStyle.h"

namespace blink {

namespace {

// True if the container will contain an absolute descendant.
bool IsContainingBlockForAbsoluteDescendant(
    const ComputedStyle& container_style,
    const ComputedStyle& descendant_style) {
  EPosition position = descendant_style.GetPosition();
  bool contains_fixed = container_style.CanContainFixedPositionObjects();
  bool contains_absolute =
      container_style.CanContainAbsolutePositionObjects() || contains_fixed;

  return (contains_absolute && position == EPosition::kAbsolute) ||
         (contains_fixed && position == EPosition::kFixed);
}

}  // namespace

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    const NGConstraintSpace& container_space,
    const ComputedStyle& container_style,
    NGFragmentBuilder* container_builder)
    : container_style_(container_style), container_builder_(container_builder) {
  NGWritingMode writing_mode(
      FromPlatformWritingMode(container_style_.GetWritingMode()));

  NGBoxStrut borders = ComputeBorders(container_space, container_style_);
  container_border_offset_ =
      NGLogicalOffset{borders.inline_start, borders.block_start};
  container_border_physical_offset_ =
      container_border_offset_.ConvertToPhysical(
          writing_mode, container_style_.Direction(),
          container_builder_->Size().ConvertToPhysical(writing_mode),
          NGPhysicalSize());

  NGLogicalSize space_size = container_builder_->Size();
  space_size.block_size -= borders.BlockSum();
  space_size.inline_size -= borders.InlineSum();

  // Initialize ConstraintSpace
  NGConstraintSpaceBuilder space_builder(writing_mode);
  space_builder.SetAvailableSize(space_size);
  space_builder.SetPercentageResolutionSize(space_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetTextDirection(container_style_.Direction());
  container_space_ = space_builder.ToConstraintSpace(writing_mode);
}

void NGOutOfFlowLayoutPart::Run() {
  Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
  container_builder_->GetAndClearOutOfFlowDescendantCandidates(
      &descendant_candidates);

  while (descendant_candidates.size() > 0) {
    for (auto& candidate : descendant_candidates) {
      if (IsContainingBlockForAbsoluteDescendant(container_style_,
                                                 candidate.node.Style())) {
        NGLogicalOffset offset;
        RefPtr<NGLayoutResult> result = LayoutDescendant(
            candidate.node, candidate.static_position, &offset);
        // TODO(atotic) Need to adjust size of overflow rect per spec.
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

  // Adjust the static_position origin. The static_position coordinate origin is
  // relative to the container's border box, ng_absolute_utils expects it to be
  // relative to the container's padding box.
  static_position.offset -= container_border_physical_offset_;

  // The block estimate is in the descendant's writing mode.
  Optional<MinMaxSize> min_max_size;
  Optional<LayoutUnit> block_estimate;

  RefPtr<NGLayoutResult> layout_result = nullptr;
  NGWritingMode descendant_writing_mode(
      FromPlatformWritingMode(descendant.Style().GetWritingMode()));

  if (AbsoluteNeedsChildInlineSize(descendant.Style()) ||
      NeedMinMaxSize(descendant.Style())) {
    min_max_size = descendant.ComputeMinMaxSize();
  }

  Optional<NGLogicalSize> replaced_size;
  if (descendant.IsReplaced())
    replaced_size =
        ComputeReplacedSize(descendant, *container_space_, min_max_size);

  NGAbsolutePhysicalPosition node_position =
      ComputePartialAbsoluteWithChildInlineSize(
          *container_space_, descendant.Style(), static_position, min_max_size,
          replaced_size);

  if (AbsoluteNeedsChildBlockSize(descendant.Style())) {
    layout_result = GenerateFragment(descendant, block_estimate, node_position);

    NGBoxFragment fragment(
        descendant_writing_mode,
        ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get()));

    block_estimate = fragment.BlockSize();
  }

  ComputeFullAbsoluteWithChildBlockSize(*container_space_, descendant.Style(),
                                        static_position, block_estimate,
                                        replaced_size, &node_position);

  // Skip this step if we produced a fragment when estimating the block size.
  if (!layout_result) {
    block_estimate =
        node_position.size.ConvertToLogical(descendant_writing_mode).block_size;
    layout_result = GenerateFragment(descendant, block_estimate, node_position);
  }

  // Compute logical offset, NGAbsolutePhysicalPosition is calculated relative
  // to the padding box so add back the container's borders.
  NGBoxStrut inset = node_position.inset.ConvertToLogical(
      container_space_->WritingMode(), container_space_->Direction());
  offset->inline_offset =
      inset.inline_start + container_border_offset_.inline_offset;
  offset->block_offset =
      inset.block_start + container_border_offset_.block_offset;

  return layout_result;
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
  NGLogicalSize container_size(
      container_space_->AvailableSize()
          .ConvertToPhysical(container_space_->WritingMode())
          .ConvertToLogical(writing_mode));

  LayoutUnit inline_size =
      node_position.size.ConvertToLogical(writing_mode).inline_size;
  LayoutUnit block_size =
      block_estimate ? *block_estimate : container_size.block_size;

  NGLogicalSize available_size{inline_size, block_size};

  // TODO(atotic) will need to be adjusted for scrollbars.
  NGConstraintSpaceBuilder builder(writing_mode);
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(container_size);
  if (block_estimate)
    builder.SetIsFixedSizeBlock(true);
  builder.SetIsFixedSizeInline(true);
  builder.SetIsNewFormattingContext(true);
  RefPtr<NGConstraintSpace> space = builder.ToConstraintSpace(writing_mode);

  return descendant.Layout(space.Get());
}

}  // namespace blink
