// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGSpaceUtils_h
#define NGSpaceUtils_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ComputedStyle;
struct NGLogicalOffset;

// Whether child's constraint space should shrink to its intrinsic width.
// This is needed for buttons, select, input, floats and orthogonal children.
// See LayoutBox::sizesLogicalWidthToFitContent for the rationale behind this.
bool ShouldShrinkToFit(const ComputedStyle& parent_style,
                       const ComputedStyle& style);

// Adjusts {@code offset} to the clearance line.
CORE_EXPORT bool AdjustToClearance(
    const WTF::Optional<LayoutUnit>& clearance_offset,
    NGLogicalOffset* offset);

}  // namespace blink

#endif  // NGSpaceUtils_h
