// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_constraint_space.h"

namespace blink {

NGPhysicalConstraintSpace::NGPhysicalConstraintSpace(
    NGPhysicalSize container_size,
    bool fixed_width,
    bool fixed_height,
    bool width_direction_triggers_scrollbar,
    bool height_direction_triggers_scrollbar,
    NGFragmentationType width_direction_fragmentation_type,
    NGFragmentationType height_direction_fragmentation_type,
    bool is_new_fc)
    : container_size_(container_size),
      fixed_width_(fixed_width),
      fixed_height_(fixed_height),
      width_direction_triggers_scrollbar_(width_direction_triggers_scrollbar),
      height_direction_triggers_scrollbar_(height_direction_triggers_scrollbar),
      width_direction_fragmentation_type_(width_direction_fragmentation_type),
      height_direction_fragmentation_type_(height_direction_fragmentation_type),
      is_new_fc_(is_new_fc) {}

void NGPhysicalConstraintSpace::AddExclusion(const NGLogicalRect& exclusion,
                                             unsigned options) {
  NGLogicalRect* exclusion_ptr = new NGLogicalRect(exclusion);
  exclusions_.append(WTF::wrapUnique(exclusion_ptr));
}

const Vector<std::unique_ptr<const NGLogicalRect>>&
NGPhysicalConstraintSpace::Exclusions(unsigned options) const {
  // TODO(layout-ng): Filter based on options? Perhaps layout Opportunities
  // should filter instead?
  return exclusions_;
}

}  // namespace blink
