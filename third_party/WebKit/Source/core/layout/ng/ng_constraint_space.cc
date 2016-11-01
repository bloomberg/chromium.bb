// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     NGDirection direction,
                                     NGPhysicalConstraintSpace* physical_space)
    : physical_space_(physical_space),
      size_(physical_space->ContainerSize().ConvertToLogical(writing_mode)),
      writing_mode_(writing_mode),
      direction_(direction) {}

NGConstraintSpace* NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  bool fixed_inline = false, fixed_block = false, is_new_fc = false;
  // XXX for orthogonal writing mode this is not right
  LayoutUnit container_logical_width =
      std::max(LayoutUnit(), box.containingBlockLogicalWidthForContent());
  LayoutUnit container_logical_height;
  if (!box.parent()) {
    container_logical_height = box.view()->viewLogicalHeightForPercentages();
  } else if (box.containingBlock()) {
    container_logical_height =
        box.containingBlock()->availableLogicalHeightForPercentageComputation();
  }
  // When we have an override size, the container_logical_{width,height} will be
  // used as the final size of the box, so it has to include border and
  // padding.
  if (box.hasOverrideLogicalContentWidth()) {
    container_logical_width =
        box.borderAndPaddingLogicalWidth() + box.overrideLogicalContentWidth();
    fixed_inline = true;
  }
  if (box.hasOverrideLogicalContentHeight()) {
    container_logical_height = box.borderAndPaddingLogicalHeight() +
                               box.overrideLogicalContentHeight();
    fixed_block = true;
  }

  if (box.isLayoutBlock() && toLayoutBlock(box).createsNewFormattingContext())
    is_new_fc = true;

  NGConstraintSpaceBuilder builder(
      FromPlatformWritingMode(box.styleRef().getWritingMode()));
  builder
      .SetContainerSize(
          NGLogicalSize(container_logical_width, container_logical_height))
      .SetIsInlineDirectionTriggersScrollbar(
          box.styleRef().overflowInlineDirection() == OverflowAuto)
      .SetIsBlockDirectionTriggersScrollbar(
          box.styleRef().overflowBlockDirection() == OverflowAuto)
      .SetIsFixedSizeInline(fixed_inline)
      .SetIsFixedSizeBlock(fixed_block)
      .SetIsNewFormattingContext(is_new_fc);

  return new NGConstraintSpace(
      FromPlatformWritingMode(box.styleRef().getWritingMode()),
      FromPlatformDirection(box.styleRef().direction()),
      builder.ToConstraintSpace());
}

void NGConstraintSpace::AddExclusion(const NGExclusion* exclusion) const {
  MutablePhysicalSpace()->AddExclusion(exclusion);
}

NGLogicalSize NGConstraintSpace::ContainerSize() const {
  return physical_space_->container_size_.ConvertToLogical(
      static_cast<NGWritingMode>(writing_mode_));
}

void NGConstraintSpace::SetSize(NGLogicalSize size) {
  size_ = size;
}

bool NGConstraintSpace::IsNewFormattingContext() const {
  return physical_space_->is_new_fc_;
}

bool NGConstraintSpace::InlineTriggersScrollbar() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_space_->width_direction_triggers_scrollbar_
             : physical_space_->height_direction_triggers_scrollbar_;
}

bool NGConstraintSpace::BlockTriggersScrollbar() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_space_->height_direction_triggers_scrollbar_
             : physical_space_->width_direction_triggers_scrollbar_;
}

bool NGConstraintSpace::FixedInlineSize() const {
  return writing_mode_ == HorizontalTopBottom ? physical_space_->fixed_width_
                                              : physical_space_->fixed_height_;
}

bool NGConstraintSpace::FixedBlockSize() const {
  return writing_mode_ == HorizontalTopBottom ? physical_space_->fixed_height_
                                              : physical_space_->fixed_width_;
}

NGFragmentationType NGConstraintSpace::BlockFragmentationType() const {
  return static_cast<NGFragmentationType>(
      writing_mode_ == HorizontalTopBottom
          ? physical_space_->height_direction_fragmentation_type_
          : physical_space_->width_direction_fragmentation_type_);
}

void NGConstraintSpace::Subtract(const NGFragment*) {
  // TODO(layout-ng): Implement.
}

NGLayoutOpportunityIterator* NGConstraintSpace::LayoutOpportunities(
    unsigned clear,
    bool for_inline_or_bfc) {
  NGLayoutOpportunityIterator* iterator = new NGLayoutOpportunityIterator(this);
  return iterator;
}

void NGConstraintSpace::SetOverflowTriggersScrollbar(bool inline_triggers,
                                                     bool block_triggers) {
  if (writing_mode_ == HorizontalTopBottom) {
    physical_space_->width_direction_triggers_scrollbar_ = inline_triggers;
    physical_space_->height_direction_triggers_scrollbar_ = block_triggers;
  } else {
    physical_space_->width_direction_triggers_scrollbar_ = block_triggers;
    physical_space_->height_direction_triggers_scrollbar_ = inline_triggers;
  }
}

void NGConstraintSpace::SetFixedSize(bool inline_fixed, bool block_fixed) {
  if (writing_mode_ == HorizontalTopBottom) {
    physical_space_->fixed_width_ = inline_fixed;
    physical_space_->fixed_height_ = block_fixed;
  } else {
    physical_space_->fixed_width_ = block_fixed;
    physical_space_->fixed_height_ = inline_fixed;
  }
}

void NGConstraintSpace::SetFragmentationType(NGFragmentationType type) {
  if (writing_mode_ == HorizontalTopBottom) {
    DCHECK_EQ(static_cast<NGFragmentationType>(
                  physical_space_->width_direction_fragmentation_type_),
              FragmentNone);
    physical_space_->height_direction_fragmentation_type_ = type;
  } else {
    DCHECK_EQ(static_cast<NGFragmentationType>(
                  physical_space_->height_direction_fragmentation_type_),
              FragmentNone);
    physical_space_->width_direction_triggers_scrollbar_ = type;
  }
}

void NGConstraintSpace::SetIsNewFormattingContext(bool is_new_fc) {
  physical_space_->is_new_fc_ = is_new_fc;
}

String NGConstraintSpace::ToString() const {
  return String::format("%s,%s %sx%s",
                        offset_.inline_offset.toString().ascii().data(),
                        offset_.block_offset.toString().ascii().data(),
                        size_.inline_size.toString().ascii().data(),
                        size_.block_size.toString().ascii().data());
}

}  // namespace blink
