// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(
    NGWritingMode writing_mode,
    TextDirection direction,
    NGLogicalSize available_size,
    NGLogicalSize percentage_resolution_size,
    NGPhysicalSize initial_containing_block_size,
    LayoutUnit fragmentainer_space_available,
    bool is_fixed_size_inline,
    bool is_fixed_size_block,
    bool is_shrink_to_fit,
    bool is_inline_direction_triggers_scrollbar,
    bool is_block_direction_triggers_scrollbar,
    NGFragmentationType block_direction_fragmentation_type,
    bool is_new_fc,
    bool is_anonymous,
    const NGMarginStrut& margin_strut,
    const NGLogicalOffset& bfc_offset,
    const std::shared_ptr<NGExclusions>& exclusions,
    Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
    const WTF::Optional<LayoutUnit>& clearance_offset)
    : available_size_(available_size),
      percentage_resolution_size_(percentage_resolution_size),
      initial_containing_block_size_(initial_containing_block_size),
      fragmentainer_space_available_(fragmentainer_space_available),
      is_fixed_size_inline_(is_fixed_size_inline),
      is_fixed_size_block_(is_fixed_size_block),
      is_shrink_to_fit_(is_shrink_to_fit),
      is_inline_direction_triggers_scrollbar_(
          is_inline_direction_triggers_scrollbar),
      is_block_direction_triggers_scrollbar_(
          is_block_direction_triggers_scrollbar),
      block_direction_fragmentation_type_(block_direction_fragmentation_type),
      is_new_fc_(is_new_fc),
      is_anonymous_(is_anonymous),
      writing_mode_(writing_mode),
      direction_(static_cast<unsigned>(direction)),
      margin_strut_(margin_strut),
      bfc_offset_(bfc_offset),
      exclusions_(exclusions),
      clearance_offset_(clearance_offset),
      layout_opp_iter_(nullptr) {
  unpositioned_floats_.swap(unpositioned_floats);
}

RefPtr<NGConstraintSpace> NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  auto writing_mode = FromPlatformWritingMode(box.StyleRef().GetWritingMode());
  bool parallel_containing_block = IsParallelWritingMode(
      FromPlatformWritingMode(
          box.ContainingBlock()->StyleRef().GetWritingMode()),
      writing_mode);
  bool fixed_inline = false, fixed_block = false;

  LayoutUnit available_logical_width;
  if (parallel_containing_block)
    available_logical_width = box.ContainingBlockLogicalWidthForContent();
  else
    available_logical_width = box.PerpendicularContainingBlockLogicalHeight();
  available_logical_width = std::max(LayoutUnit(), available_logical_width);

  LayoutUnit available_logical_height;
  if (!box.Parent()) {
    available_logical_height = box.View()->ViewLogicalHeightForPercentages();
  } else if (box.ContainingBlock()) {
    if (parallel_containing_block) {
      available_logical_height =
          box.ContainingBlock()
              ->AvailableLogicalHeightForPercentageComputation();
    } else {
      available_logical_height = box.ContainingBlockLogicalWidthForContent();
    }
  }
  NGLogicalSize percentage_size = {available_logical_width,
                                   available_logical_height};
  NGLogicalSize available_size = percentage_size;
  // When we have an override size, the available_logical_{width,height} will be
  // used as the final size of the box, so it has to include border and
  // padding.
  if (box.HasOverrideLogicalContentWidth()) {
    available_size.inline_size =
        box.BorderAndPaddingLogicalWidth() + box.OverrideLogicalContentWidth();
    fixed_inline = true;
  }
  if (box.HasOverrideLogicalContentHeight()) {
    available_size.block_size = box.BorderAndPaddingLogicalHeight() +
                                box.OverrideLogicalContentHeight();
    fixed_block = true;
  }

  bool is_new_fc =
      box.IsLayoutBlock() && ToLayoutBlock(box).CreatesNewFormattingContext();

  FloatSize icb_float_size = box.View()->ViewportSizeForViewportUnits();
  NGPhysicalSize initial_containing_block_size{
      LayoutUnit(icb_float_size.Width()), LayoutUnit(icb_float_size.Height())};

  // ICB cannot be indefinite by the spec.
  DCHECK_GE(initial_containing_block_size.width, LayoutUnit());
  DCHECK_GE(initial_containing_block_size.height, LayoutUnit());

  return NGConstraintSpaceBuilder(writing_mode)
      .SetAvailableSize(available_size)
      .SetPercentageResolutionSize(percentage_size)
      .SetInitialContainingBlockSize(initial_containing_block_size)
      .SetIsInlineDirectionTriggersScrollbar(
          box.StyleRef().OverflowInlineDirection() == EOverflow::kAuto)
      .SetIsBlockDirectionTriggersScrollbar(
          box.StyleRef().OverflowBlockDirection() == EOverflow::kAuto)
      .SetIsFixedSizeInline(fixed_inline)
      .SetIsFixedSizeBlock(fixed_block)
      .SetIsShrinkToFit(
          box.SizesLogicalWidthToFitContent(box.StyleRef().LogicalWidth()))
      .SetIsNewFormattingContext(is_new_fc)
      .SetTextDirection(box.StyleRef().Direction())
      .ToConstraintSpace(writing_mode);
}

void NGConstraintSpace::AddExclusion(const NGExclusion& exclusion) {
  exclusions_->Add(exclusion);
  // Invalidate the Layout Opportunity Iterator.
  layout_opp_iter_.reset();
}

NGFragmentationType NGConstraintSpace::BlockFragmentationType() const {
  return static_cast<NGFragmentationType>(block_direction_fragmentation_type_);
}

NGLayoutOpportunityIterator* NGConstraintSpace::LayoutOpportunityIterator(
    const NGLogicalOffset& iter_offset) {
  if (layout_opp_iter_ && layout_opp_iter_->Offset() != iter_offset)
    layout_opp_iter_.reset();

  if (!layout_opp_iter_) {
    layout_opp_iter_ = WTF::MakeUnique<NGLayoutOpportunityIterator>(
        Exclusions().get(), AvailableSize(), iter_offset);
  }
  return layout_opp_iter_.get();
}

String NGConstraintSpace::ToString() const {
  return String::Format(
      "Offset: %s,%s Size: %sx%s MarginStrut: %s"
      " Clearance: %s",
      bfc_offset_.inline_offset.ToString().Ascii().data(),
      bfc_offset_.block_offset.ToString().Ascii().data(),
      AvailableSize().inline_size.ToString().Ascii().data(),
      AvailableSize().block_size.ToString().Ascii().data(),
      margin_strut_.ToString().Ascii().data(),
      clearance_offset_.has_value()
          ? clearance_offset_.value().ToString().Ascii().data()
          : "none");
}

}  // namespace blink
