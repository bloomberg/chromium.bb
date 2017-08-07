// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_result.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(
    NGWritingMode writing_mode,
    TextDirection direction,
    NGLogicalSize available_size,
    NGLogicalSize percentage_resolution_size,
    Optional<LayoutUnit> parent_percentage_resolution_inline_size,
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
    const WTF::Optional<NGLogicalOffset>& floats_bfc_offset,
    const std::shared_ptr<NGExclusions>& exclusions,
    Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
    const WTF::Optional<LayoutUnit>& clearance_offset,
    Vector<NGBaselineRequest>& baseline_requests)
    : available_size_(available_size),
      percentage_resolution_size_(percentage_resolution_size),
      parent_percentage_resolution_inline_size_(
          parent_percentage_resolution_inline_size),
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
      floats_bfc_offset_(floats_bfc_offset),
      exclusions_(exclusions),
      clearance_offset_(clearance_offset) {
  unpositioned_floats_.swap(unpositioned_floats);
  baseline_requests_.swap(baseline_requests);
}

RefPtr<NGConstraintSpace> NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box,
    Optional<LayoutUnit> override_logical_width,
    Optional<LayoutUnit> override_logical_height) {
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
  } else if (override_logical_width.has_value()) {
    available_size.inline_size =
        box.BorderAndPaddingLogicalWidth() + override_logical_width.value();
    fixed_inline = true;
  }
  if (box.HasOverrideLogicalContentHeight()) {
    available_size.block_size = box.BorderAndPaddingLogicalHeight() +
                                box.OverrideLogicalContentHeight();
    fixed_block = true;
  } else if (override_logical_height.has_value()) {
    available_size.block_size =
        box.BorderAndPaddingLogicalHeight() + override_logical_height.value();
    fixed_block = true;
  }

  bool is_new_fc = true;
  // TODO(ikilpatrick): This DCHECK needs to be enabled once we've switched
  // LayoutTableCell, etc over to LayoutNG.
  //
  // We currently need to "force" LayoutNG roots to be formatting contexts so
  // that floats have layout performed on them.
  //
  // DCHECK(is_new_fc,
  //  box.IsLayoutBlock() && ToLayoutBlock(box).CreatesNewFormattingContext());

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

Optional<LayoutUnit> NGConstraintSpace::ParentPercentageResolutionInlineSize()
    const {
  if (!parent_percentage_resolution_inline_size_.has_value())
    return {};
  if (*parent_percentage_resolution_inline_size_ != NGSizeIndefinite)
    return *parent_percentage_resolution_inline_size_;
  return initial_containing_block_size_.ConvertToLogical(WritingMode())
      .inline_size;
}

void NGConstraintSpace::AddExclusion(const NGExclusion& exclusion) {
  exclusions_->Add(exclusion);
}

NGFragmentationType NGConstraintSpace::BlockFragmentationType() const {
  return static_cast<NGFragmentationType>(block_direction_fragmentation_type_);
}

bool NGConstraintSpace::operator==(const NGConstraintSpace& other) const {
  // TODO(cbiesinger): For simplicity and performance, for now, we only
  // consider two constraint spaces equal if neither one has unpositioned
  // floats. We should consider changing this in the future.
  if (unpositioned_floats_.size() || other.unpositioned_floats_.size())
    return false;

  if (exclusions_ && other.exclusions_ && *exclusions_ != *other.exclusions_)
    return false;

  return available_size_ == other.available_size_ &&
         percentage_resolution_size_ == other.percentage_resolution_size_ &&
         parent_percentage_resolution_inline_size_ ==
             other.parent_percentage_resolution_inline_size_ &&
         initial_containing_block_size_ ==
             other.initial_containing_block_size_ &&
         fragmentainer_space_available_ ==
             other.fragmentainer_space_available_ &&
         is_fixed_size_inline_ == other.is_fixed_size_inline_ &&
         is_fixed_size_block_ == other.is_fixed_size_block_ &&
         is_shrink_to_fit_ == other.is_shrink_to_fit_ &&
         is_inline_direction_triggers_scrollbar_ ==
             other.is_inline_direction_triggers_scrollbar_ &&
         is_block_direction_triggers_scrollbar_ ==
             other.is_block_direction_triggers_scrollbar_ &&
         block_direction_fragmentation_type_ ==
             other.block_direction_fragmentation_type_ &&
         is_new_fc_ == other.is_new_fc_ &&
         is_anonymous_ == other.is_anonymous_ &&
         writing_mode_ == other.writing_mode_ &&
         direction_ == other.direction_ &&
         margin_strut_ == other.margin_strut_ &&
         bfc_offset_ == other.bfc_offset_ &&
         floats_bfc_offset_ == other.floats_bfc_offset_ &&
         clearance_offset_ == other.clearance_offset_ &&
         baseline_requests_ == other.baseline_requests_;
}

bool NGConstraintSpace::operator!=(const NGConstraintSpace& other) const {
  return !(*this == other);
}

String NGConstraintSpace::ToString() const {
  return String::Format(
      "Offset: %s,%s Size: %sx%s Clearance: %s",
      bfc_offset_.inline_offset.ToString().Ascii().data(),
      bfc_offset_.block_offset.ToString().Ascii().data(),
      AvailableSize().inline_size.ToString().Ascii().data(),
      AvailableSize().block_size.ToString().Ascii().data(),
      clearance_offset_.has_value()
          ? clearance_offset_.value().ToString().Ascii().data()
          : "none");
}

}  // namespace blink
