// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_constraint_space.h"

namespace blink {

NGPhysicalConstraintSpace::NGPhysicalConstraintSpace(
    NGPhysicalSize container_size)
    : container_size_(container_size),
      fixed_width_(0),
      fixed_height_(0),
      width_direction_triggers_scrollbar_(0),
      height_direction_triggers_scrollbar_(0),
      width_direction_fragmentation_type_(FragmentNone),
      height_direction_fragmentation_type_(FragmentNone) {}

NGPhysicalConstraintSpace::NGPhysicalConstraintSpace()
    : fixed_width_(0),
      fixed_height_(0),
      width_direction_triggers_scrollbar_(0),
      height_direction_triggers_scrollbar_(0),
      width_direction_fragmentation_type_(FragmentNone),
      height_direction_fragmentation_type_(FragmentNone) {}

NGPhysicalConstraintSpace::NGPhysicalConstraintSpace(
    const NGPhysicalConstraintSpace& other)
    : fixed_width_(other.fixed_width_),
      fixed_height_(other.fixed_height_),
      width_direction_triggers_scrollbar_(
          other.width_direction_triggers_scrollbar_),
      height_direction_triggers_scrollbar_(
          other.height_direction_triggers_scrollbar_),
      width_direction_fragmentation_type_(
          other.width_direction_fragmentation_type_),
      height_direction_fragmentation_type_(
          other.height_direction_fragmentation_type_) {}

void NGPhysicalConstraintSpace::AddExclusion(const NGExclusion exclusion,
                                             unsigned options) {
  exclusions_.append(exclusion);
}

const Vector<NGExclusion>& NGPhysicalConstraintSpace::Exclusions(
    unsigned options) const {
  // TODO(layout-ng): Filter based on options? Perhaps layout Opportunities
  // should filter instead?
  return exclusions_;
}

}  // namespace blink
