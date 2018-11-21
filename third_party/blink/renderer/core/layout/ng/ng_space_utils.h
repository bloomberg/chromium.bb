// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGSpaceUtils_h
#define NGSpaceUtils_h

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
struct NGBfcOffset;

// Adjusts {@code offset} to the clearance line.
CORE_EXPORT bool AdjustToClearance(LayoutUnit clearance_offset,
                                   NGBfcOffset* offset);

// Create a child constraint space with no sizing data, except for fallback
// inline sizing for orthongonal flow roots. This will not and can not be used
// for final layout, but is needed in an intermediate measure pass that
// calculates the min/max size contribution from a child that establishes an
// orthogonal flow root.
NGConstraintSpace CreateIndefiniteConstraintSpaceForChild(
    const ComputedStyle& container_style,
    NGLayoutInputNode child);

}  // namespace blink

#endif  // NGSpaceUtils_h
