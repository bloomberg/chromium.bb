// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatsUtils_h
#define NGFloatsUtils_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class NGConstraintSpace;
struct NGPositionedFloat;
struct NGUnpositionedFloat;

// Returns the inline size (relative to {@code parent_space}) of the
// unpositioned float. If the float is in a different writing mode, this will
// perform a layout.
CORE_EXPORT LayoutUnit
ComputeInlineSizeForUnpositionedFloat(NGConstraintSpace* parent_space,
                                      NGUnpositionedFloat* unpositioned_float);

// Positions {@code unpositioned_float} into {@code new_parent_space}.
// @returns A positioned float.
CORE_EXPORT NGPositionedFloat
PositionFloat(LayoutUnit origin_block_offset,
              LayoutUnit parent_bfc_block_offset,
              NGUnpositionedFloat*,
              NGConstraintSpace* new_parent_space);

// Positions the list of {@code unpositioned_floats}. Adds them as exclusions to
// {@code space}.
CORE_EXPORT const Vector<NGPositionedFloat> PositionFloats(
    LayoutUnit origin_block_offset,
    LayoutUnit container_block_offset,
    const Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
    NGConstraintSpace* space);

}  // namespace blink

#endif  // NGFloatsUtils_h
