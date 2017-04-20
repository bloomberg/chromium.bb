// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatsUtils_h
#define NGFloatsUtils_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

// Positions {@code floating_object} into {@code new_parent_space}.
// @returns Logical offset of the positioned float.
CORE_EXPORT NGLogicalOffset PositionFloat(NGFloatingObject*,
                                          NGConstraintSpace* new_parent_space);

// Positions the list of floats by updating their {@code logical_offset}.
void PositionFloats(LayoutUnit origin_block_offset,
                    LayoutUnit from_block_offset,
                    const Vector<RefPtr<NGFloatingObject>>&,
                    NGConstraintSpace*);
}  // namespace blink

#endif  // NGFloatsUtils_h
