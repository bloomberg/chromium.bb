// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGAbsoluteUtils_h
#define NGAbsoluteUtils_h

#include "core/CoreExport.h"
#include "core/layout/MinMaxSize.h"
#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/layout/ng/geometry/ng_static_position.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;

struct CORE_EXPORT NGAbsolutePhysicalPosition {
  NGPhysicalBoxStrut inset;
  NGPhysicalSize size;
  String ToString() const;
};

// The following routines implement absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
//
// The size is computed as NGAbsolutePhysicalPosition.
// It needs to be computed in 4 stages:
// 1. If AbsoluteNeedsChildInlineSize compute estimated inline_size using
//    MinMaxSize.ShrinkToFit.
// 2. Compute part of PhysicalPosition that depends upon child inline size
//    with ComputePartialAbsoluteWithChildInlineSize.
// 3. If AbsoluteNeedsChildBlockSize compute estimated block_size by
//    performing layout with the inline_size calculated from (2).
// 4. Compute full PhysicalPosition by filling it in with parts that depend
//    upon child's block_size.

// True if ComputePartialAbsoluteWithChildInlineSize will need
// estimated inline size.
CORE_EXPORT bool AbsoluteNeedsChildInlineSize(const ComputedStyle&);

// True if ComputeFullAbsoluteWithChildBlockSize will need
// estimated block size.
CORE_EXPORT bool AbsoluteNeedsChildBlockSize(const ComputedStyle&);

// Compute part of position that depends on child's inline_size
// returns partially filled position.
CORE_EXPORT NGAbsolutePhysicalPosition
ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition&,
    const Optional<MinMaxSize>& child_minmax);

// Compute rest of NGPhysicalRect that depends on child's block_size.
CORE_EXPORT void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition&,
    const Optional<LayoutUnit>& child_block_size,
    NGAbsolutePhysicalPosition* position);

// TODO(atotic) Absolute coordinates for replaced elements
// ComputeAbsoluteReplaced.
// https://www.w3.org/TR/css-position-3/#abs-replaced-width
}  // namespace blink

#endif  // NGAbsoluteUtils_h
