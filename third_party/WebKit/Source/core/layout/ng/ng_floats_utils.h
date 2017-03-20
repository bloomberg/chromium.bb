// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatsUtils_h
#define NGFloatsUtils_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_floating_object.h"

namespace blink {

// Calculates the relative position from {@code from_offset} of the
// floating object that is requested to be positioned from {@code origin_point}.
CORE_EXPORT NGLogicalOffset PositionFloat(const NGLogicalOffset& origin_point,
                                          const NGLogicalOffset& from_offset,
                                          NGFloatingObject*,
                                          NGConstraintSpace* new_parent_space);

}  // namespace blink

#endif  // NGFloatsUtils_h
