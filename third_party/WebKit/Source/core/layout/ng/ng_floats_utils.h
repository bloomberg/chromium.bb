// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatsUtils_h
#define NGFloatsUtils_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_positioned_float.h"

namespace blink {

// Positions {@code floating_object} into {@code new_parent_space}.
// @returns A positioned float.
CORE_EXPORT NGPositionedFloat
PositionFloat(NGFloatingObject*, NGConstraintSpace* new_parent_space);

// Positions the list of {@code floating_objects}. Adds them as exclusions to
// {@code space}.
CORE_EXPORT const Vector<NGPositionedFloat> PositionFloats(
    LayoutUnit origin_block_offset,
    LayoutUnit from_block_offset,
    LayoutUnit container_block_offset,
    const Vector<RefPtr<NGFloatingObject>>& floating_objects,
    NGConstraintSpace* space);

}  // namespace blink

#endif  // NGFloatsUtils_h
