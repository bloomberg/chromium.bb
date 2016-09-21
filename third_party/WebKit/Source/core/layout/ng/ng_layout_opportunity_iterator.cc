// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_opportunity_iterator.h"

#include "core/layout/ng/ng_physical_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/NonCopyingSort.h"

namespace blink {

NGLayoutOpportunityIterator::NGLayoutOpportunityIterator(
    NGConstraintSpace* space,
    unsigned clear,
    bool for_inline_or_bfc)
    : constraint_space_(space),
      clear_(clear),
      for_inline_or_bfc_(for_inline_or_bfc),
      current_x_(0),
      current_y_(0) {
  FilterExclusions();
  if (!IsValidPosition())
    NextPosition();
  ComputeOpportunitiesForPosition();
}

static inline bool AscendingTopLeftCompare(const NGExclusion& a,
                                           const NGExclusion& b) {
  if (a.Top() < b.Top())
    return true;
  if (a.Top() > b.Top())
    return false;
  return a.Left() < b.Left();
}

void NGLayoutOpportunityIterator::FilterExclusions() {
  filtered_exclusions_.clear();
  for (const auto& item : constraint_space_->PhysicalSpace()->Exclusions())
    filtered_exclusions_.append(item);
  nonCopyingSort(filtered_exclusions_.begin(), filtered_exclusions_.end(),
                 AscendingTopLeftCompare);

  // TODO(eae): Writing modes.
  LayoutUnit left = constraint_space_->Offset().inline_offset;
  LayoutUnit top = constraint_space_->Offset().block_offset;
  LayoutUnit right = left + constraint_space_->Size().inline_size;
  LayoutUnit bottom = top + constraint_space_->Size().block_size;

  size_t i = filtered_exclusions_.size();
  while (i--) {
    const NGExclusion& exclusion = filtered_exclusions_[i];

    // Remove items fully outside the view of the current constraint space.
    if (exclusion.Right() < left || exclusion.Bottom() < top ||
        exclusion.Left() > right || exclusion.Top() > bottom)
      filtered_exclusions_.remove(i);
  }
}

NGConstraintSpace* NGLayoutOpportunityIterator::Next() {
  if (current_opportunities_.isEmpty() && NextPosition())
    ComputeOpportunitiesForPosition();

  if (!current_opportunities_.isEmpty()) {
    NGConstraintSpace* opportunity = current_opportunities_.last();
    current_opportunities_.removeLast();
    return opportunity;
  }

  return nullptr;
}

static inline bool DescendingWidthCompare(const NGConstraintSpace* a,
                                          const NGConstraintSpace* b) {
  return a->Size().inline_size < b->Size().inline_size;
}

bool NGLayoutOpportunityIterator::NextPosition() {
  LayoutUnit right = constraint_space_->Size().inline_size;
  LayoutUnit bottom = constraint_space_->Size().block_size;

  while (current_y_ < bottom) {
    // Try to find a start position to the right of the current position by
    // identifying the leftmost applicable right edge.
    LayoutUnit minRight = right;
    for (const NGExclusion& exclusion : filtered_exclusions_) {
      if (exclusion.Top() <= current_y_ && exclusion.Bottom() > current_y_ &&
          exclusion.Right() > current_x_ && exclusion.Right() < minRight)
        minRight = exclusion.Right();
    }
    current_x_ = minRight;

    // If no valid position was found to the right move down to the next "line".
    // Reset current_x_ and set current_y_ to the top of the next exclusion down
    // or the bottom of the closest exclusion, whichever is closest.
    if (current_x_ == right) {
      LayoutUnit minBottom = bottom;
      for (const NGExclusion& exclusion : filtered_exclusions_) {
        if (exclusion.Top() > current_y_ && exclusion.Top() < minBottom)
          minBottom = exclusion.Top();
        if (exclusion.Bottom() > current_y_ && exclusion.Bottom() < minBottom)
          minBottom = exclusion.Bottom();
      }
      current_x_ = LayoutUnit();
      current_y_ = minBottom;
    }
    if (IsValidPosition())
      return true;
  }

  return false;
}

// Checks whether the current position is within an exclusion.
bool NGLayoutOpportunityIterator::IsValidPosition() {
  for (const NGExclusion& exclusion : filtered_exclusions_) {
    if (exclusion.Top() >= current_y_ && exclusion.Bottom() < current_y_ &&
        exclusion.Left() >= current_x_ && exclusion.Right() < current_x_) {
      return false;
    }
  }

  return true;
}

void NGLayoutOpportunityIterator::FilterForPosition(Vector<NGExclusion>& out) {
  out.clear();
  for (const auto& item : filtered_exclusions_)
    out.append(item);
  nonCopyingSort(out.begin(), out.end(), AscendingTopLeftCompare);

  size_t len = out.size();
  for (size_t i = 0; i < len; i++) {
    const NGExclusion& exclusion = out[i];

    // Remove items above OR to the left of the start offset as they have no
    // effect on layout opportunities within this view.
    if (exclusion.Right() <= current_x_ || exclusion.Bottom() <= current_y_) {
      out.remove(i);
      len--;
      continue;
    }

    // Remove items below AND to the right of the current exclusions as they're
    // occluded and won't affect the layout opportunities.
    for (size_t j = out.size() - 1; j > i; j--) {
      const NGExclusion& item = out[j];
      if (item.Top() > exclusion.Top() && item.Left() >= exclusion.Left()) {
        out.remove(j);
        len--;
      }
    }
  }
}

void NGLayoutOpportunityIterator::ComputeOpportunitiesForPosition() {
  current_opportunities_.clear();

  Vector<NGExclusion> exclusions_for_position;
  FilterForPosition(exclusions_for_position);

  // TODO(eae): Filter based on clear_ and for_inline_or_bfc_. Return early for
  // now to make it clear neither are supported yet.
  if (clear_ != NGClearNone || !for_inline_or_bfc_) {
    return;
  }

  // TODO(eae): Writing modes.
  LayoutUnit left = current_x_;
  LayoutUnit top = current_y_;
  LayoutUnit right = constraint_space_->Size().inline_size;
  LayoutUnit bottom = constraint_space_->Size().block_size;

  // Compute opportunity for the full width from the start position to the right
  // edge of the NGConstraintSpace.
  LayoutUnit opportunityHeight =
      heightForOpportunity(exclusions_for_position, left, top, right, bottom);
  if (opportunityHeight && right > left)
    addLayoutOpportunity(left, top, right, opportunityHeight + top);

  // Compute the maximum available height between the current position and the
  // left edge of each exclusion. The distance between the current horizontal
  // position and the left edge of the exclusion determines the width of the
  // opportunity.
  for (const NGExclusion& exclusion : exclusions_for_position) {
    if (exclusion.Right() > current_x_ && exclusion.Bottom() > current_y_) {
      opportunityHeight = heightForOpportunity(exclusions_for_position, left,
                                               top, exclusion.Left(), bottom);
      if (opportunityHeight && exclusion.Left() > left)
        addLayoutOpportunity(left, top, exclusion.Left(),
                             opportunityHeight + top);
    }
  }

  nonCopyingSort(current_opportunities_.begin(), current_opportunities_.end(),
                 DescendingWidthCompare);
}

// For the given 2D range (opportunity), this will return a height which makes
// it bounded by the highest exclusion in the filtered exclusion list within the
// range. Returns 0-height for an invalid opportunity (which has zero area).
LayoutUnit NGLayoutOpportunityIterator::heightForOpportunity(
    const Vector<NGExclusion>& exclusions_for_position,
    LayoutUnit left,
    LayoutUnit top,
    LayoutUnit right,
    LayoutUnit bottom) {
  LayoutUnit lowestBottom = bottom;
  for (const NGExclusion& exclusion : exclusions_for_position) {
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
