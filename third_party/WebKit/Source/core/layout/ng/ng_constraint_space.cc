// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/ng/ng_units.h"

namespace blink {

// TODO: This should set the size of the NGPhysicalConstraintSpace. Or we could
// remove it requiring that a NGConstraintSpace is created from a
// NGPhysicalConstraintSpace.
NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     NGLogicalSize container_size)
    : physical_space_(new NGPhysicalConstraintSpace()),
      writing_mode_(writing_mode) {
  SetContainerSize(container_size);
}

NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     NGPhysicalConstraintSpace* physical_space)
    : physical_space_(physical_space), writing_mode_(writing_mode) {
}

NGConstraintSpace::NGConstraintSpace(const NGConstraintSpace& other,
                                     NGLogicalSize container_size)
    : physical_space_(*other.physical_space_),
      writing_mode_(other.writing_mode_) {
  SetContainerSize(container_size);
}

NGLogicalSize NGConstraintSpace::ContainerSize() const {
  return physical_space_->container_size_.ConvertToLogical(
      static_cast<NGWritingMode>(writing_mode_));
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

NGLayoutOpportunityIterator NGConstraintSpace::LayoutOpportunities(
    unsigned clear,
    bool for_inline_or_bfc) {
  NGLayoutOpportunityIterator iterator(this, clear, for_inline_or_bfc);
  return iterator;
}

void NGConstraintSpace::SetContainerSize(NGLogicalSize container_size) {
  physical_space_->container_size_ = container_size.ConvertToPhysical(
      static_cast<NGWritingMode>(writing_mode_));
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

NGConstraintSpace* NGLayoutOpportunityIterator::Next() {
  auto* exclusions = constraint_space_->PhysicalSpace()->Exclusions();
  if (!exclusions->head())
    return new NGConstraintSpace(constraint_space_->WritingMode(),
                                 constraint_space_->PhysicalSpace());
  return nullptr;
}

}  // namespace blink
