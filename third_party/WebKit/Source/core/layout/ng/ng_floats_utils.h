// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatsUtils_h
#define NGFloatsUtils_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/layout/ng/ng_unpositioned_float.h"

namespace blink {

// Returns the inline size (relative to {@code parent_space}) of the
// unpositioned float. If the float is in a different writing mode, this will
// perform a layout.
CORE_EXPORT LayoutUnit
ComputeInlineSizeForUnpositionedFloat(const NGConstraintSpace& parent_space,
                                      NGUnpositionedFloat* unpositioned_float);

// Positions {@code unpositioned_float} into {@code new_parent_space}.
// @returns A positioned float.
CORE_EXPORT NGPositionedFloat
PositionFloat(LayoutUnit origin_block_offset,
              LayoutUnit parent_bfc_block_offset,
              NGUnpositionedFloat*,
              const NGConstraintSpace& parent_space,
              NGExclusionSpace* exclusion_space);

// Positions the list of {@code unpositioned_floats}. Adds them as exclusions to
// {@code space}.
CORE_EXPORT const Vector<NGPositionedFloat> PositionFloats(
    LayoutUnit origin_block_offset,
    LayoutUnit container_block_offset,
    const Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
    const NGConstraintSpace& space,
    NGExclusionSpace* exclusion_space);

}  // namespace blink

#endif  // NGFloatsUtils_h
