// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/NonCopyingSort.h"
#include <climits>

namespace blink {

// TODO: This should set the size of the NGPhysicalConstraintSpace. Or we could
// remove it requiring that a NGConstraintSpace is created from a
// NGPhysicalConstraintSpace.
NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     NGLogicalSize container_size)
    : physical_space_(new NGPhysicalConstraintSpace(
          container_size.ConvertToPhysical(writing_mode))),
      size_(container_size),
      writing_mode_(writing_mode) {}

NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     NGPhysicalConstraintSpace* physical_space)
    : physical_space_(physical_space),
      size_(physical_space->ContainerSize().ConvertToLogical(writing_mode)),
      writing_mode_(writing_mode) {}

NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     const NGConstraintSpace* constraint_space)
    : physical_space_(constraint_space->PhysicalSpace()),
      offset_(constraint_space->Offset()),
      size_(constraint_space->Size()),
      writing_mode_(writing_mode) {}

NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     const NGConstraintSpace& other,
                                     NGLogicalOffset offset,
                                     NGLogicalSize size)
    : physical_space_(other.PhysicalSpace()),
      offset_(offset),
      size_(size),
      writing_mode_(writing_mode) {}

NGConstraintSpace::NGConstraintSpace(const NGConstraintSpace& other,
                                     NGLogicalOffset offset,
                                     NGLogicalSize size)
    : physical_space_(other.PhysicalSpace()),
      offset_(offset),
      size_(size),
      writing_mode_(other.WritingMode()) {}

NGConstraintSpace* NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  bool fixed_inline = false, fixed_block = false;
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
  if (box.hasOverrideLogicalContentWidth()) {
    container_logical_width = box.overrideLogicalContentWidth();
    fixed_inline = true;
  }
  if (box.hasOverrideLogicalContentHeight()) {
    container_logical_width = box.overrideLogicalContentHeight();
    fixed_block = true;
  }

  NGConstraintSpace* derived_constraint_space = new NGConstraintSpace(
      FromPlatformWritingMode(box.styleRef().getWritingMode()),
      NGLogicalSize(container_logical_width, container_logical_height));
  derived_constraint_space->SetOverflowTriggersScrollbar(
      box.styleRef().overflowInlineDirection() == OverflowAuto,
      box.styleRef().overflowBlockDirection() == OverflowAuto);
  derived_constraint_space->SetFixedSize(fixed_inline, fixed_block);
  return derived_constraint_space;
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

NGLayoutOpportunityIterator* NGConstraintSpace::LayoutOpportunities(
    unsigned clear,
    bool for_inline_or_bfc) {
  NGLayoutOpportunityIterator* iterator =
      new NGLayoutOpportunityIterator(this, clear, for_inline_or_bfc);
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

String NGConstraintSpace::toString() const {
  return String::format("Size: %s, %s",
                        size_.inline_size.toString().ascii().data(),
                        size_.block_size.toString().ascii().data());
}

static inline bool AscendingTopCompare(const NGExclusion& a,
                                       const NGExclusion& b) {
  return a.Top() > b.Top();
}

NGLayoutOpportunityIterator::NGLayoutOpportunityIterator(
    NGConstraintSpace* space,
    unsigned clear,
    bool for_inline_or_bfc)
    : constraint_space_(space),
      clear_(clear),
      for_inline_or_bfc_(for_inline_or_bfc),
      current_exclusion_idx_(0) {
  for (const auto& item : constraint_space_->PhysicalSpace()->Exclusions())
    filtered_exclusions_.append(item);

  nonCopyingSort(filtered_exclusions_.begin(), filtered_exclusions_.end(),
                 AscendingTopCompare);

  // TODO(eae): Set based on offset.
  LayoutUnit left;
  LayoutUnit top;

  unsigned i = filtered_exclusions_.size();
  while (i--) {
    const NGExclusion& exclusion = filtered_exclusions_[i];

    // Remove items above OR to the left of the start offset as they have no
    // effect on layout opportunities within this view.
    if (exclusion.Right() <= left || exclusion.Bottom() <= top) {
      filtered_exclusions_.remove(i);
      continue;
    }

    // Remove items below AND to the right of the current exclusions as they're
    // occluded and won't affect the layout opportunities.
    for (unsigned j = filtered_exclusions_.size() - 1; j > i; j--) {
      const NGExclusion& item = filtered_exclusions_[j];
      if (item.Top() > exclusion.Top() && item.Left() > exclusion.Left())
        filtered_exclusions_.remove(j);
    }
  }
}

NGConstraintSpace* NGLayoutOpportunityIterator::Next() {
  if (current_opportunities_.isEmpty() &&
      current_exclusion_idx_ < filtered_exclusions_.size()) {
    computeForExclusion(current_exclusion_idx_);
    current_exclusion_idx_++;
  }

  if (!current_opportunities_.isEmpty()) {
    NGConstraintSpace* opportunity = current_opportunities_.last();
    current_opportunities_.removeLast();
    return opportunity;
  }

  if (filtered_exclusions_.isEmpty() && current_exclusion_idx_ == 0) {
    current_exclusion_idx_++;
    return new NGConstraintSpace(constraint_space_->WritingMode(),
                                 constraint_space_->PhysicalSpace());
  }

  return nullptr;
}

static inline bool DescendingWidthCompare(const NGConstraintSpace* a,
                                          const NGConstraintSpace* b) {
  return a->Size().inline_size > b->Size().inline_size;
}

void NGLayoutOpportunityIterator::computeForExclusion(unsigned index) {
  current_opportunities_.clear();

  // TODO(eae): Set based on index.
  LayoutUnit left;
  LayoutUnit top;

  // TODO(eae): Writing modes.
  LayoutUnit right = constraint_space_->Size().inline_size;
  LayoutUnit bottom = constraint_space_->Size().block_size;

  // TODO(eae): Filter based on clear_ and for_inline_or_bfc_. Return early for
  // now to make it clear neither are supported yet.
  if (clear_ != NGClearNone || !for_inline_or_bfc_)
    return;

  // Compute opportunity for the full width from the start position to the right
  // edge of the NGConstraintSpace.
  LayoutUnit opportunityHeight = heightForOpportunity(left, top, right, bottom);
  if (opportunityHeight && right > left)
    addLayoutOpportunity(left, top, right - left, opportunityHeight);

  // Compute the maximum available height between the current position and the
  // left edge of each exclusion. The distance between the current horizontal
  // position and the left edge of the exclusion determines the width of the
  // opportunity.
  for (const NGExclusion& exclusion : filtered_exclusions_) {
    opportunityHeight =
        heightForOpportunity(left, top, exclusion.Left(), bottom);
    if (opportunityHeight && exclusion.Left() > left)
      addLayoutOpportunity(left, top, exclusion.Left() - left,
                           opportunityHeight);
  }

  nonCopyingSort(current_opportunities_.begin(), current_opportunities_.end(),
                 DescendingWidthCompare);
}

// For the given 2D range (opportunity), this will return a height which makes
// it bounded by the highest exclusion in the filtered exclusion list within the
// range. Returns 0-height for an invalid opportunity (which has zero area).
LayoutUnit NGLayoutOpportunityIterator::heightForOpportunity(
    LayoutUnit left,
    LayoutUnit top,
    LayoutUnit right,
    LayoutUnit bottom) {
  LayoutUnit lowestBottom = bottom;
  for (const NGExclusion& exclusion : filtered_exclusions_) {
    if (exclusion.Left() < right && exclusion.Right() > left &&
        exclusion.Bottom() > top && exclusion.Top() <= lowestBottom)
      lowestBottom = exclusion.Top();
  }
  return std::max(lowestBottom - top, LayoutUnit());
}

void NGLayoutOpportunityIterator::addLayoutOpportunity(LayoutUnit left,
                                                       LayoutUnit top,
                                                       LayoutUnit right,
                                                       LayoutUnit bottom) {
  current_opportunities_.append(
      new NGConstraintSpace(*constraint_space_, NGLogicalOffset(left, top),
                            NGLogicalSize(right - left, bottom - top)));
}

}  // namespace blink
