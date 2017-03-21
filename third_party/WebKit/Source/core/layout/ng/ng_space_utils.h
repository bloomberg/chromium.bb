// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGSpaceUtils_h
#define NGSpaceUtils_h

#include "core/layout/ng/ng_constraint_space.h"
#include "core/style/ComputedStyle.h"
#include "wtf/Optional.h"

namespace blink {

// Whether an in-flow block-level child creates a new formatting context.
//
// This will *NOT* check the following cases:
//  - The child is out-of-flow, e.g. floating or abs-pos.
//  - The child is a inline-level, e.g. "display: inline-block".
//  - The child establishes a new formatting context, but should be a child of
//    another layout algorithm, e.g. "display: table-caption" or flex-item.
CORE_EXPORT bool IsNewFormattingContextForInFlowBlockLevelChild(
    const NGConstraintSpace& space,
    const ComputedStyle& style);

// Gets the clearance offset based on the provided {@code style} and list of
// exclusions that represent left/right float.
CORE_EXPORT WTF::Optional<LayoutUnit> GetClearanceOffset(
    const std::shared_ptr<NGExclusions>& exclusions,
    const ComputedStyle& style);

}  // namespace blink

#endif  // NGSpaceUtils_h
