// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/NGBlockLayoutAlgorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/NGConstraintSpace.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm()
{
}

NGConstraintSpace NGBlockLayoutAlgorithm::createConstraintSpaceFromLayoutObject(const LayoutBox& child)
{
    bool fixedInline = false, fixedBlock = false;
    // XXX for orthogonal writing mode this is not right
    LayoutUnit containerLogicalWidth = std::max(LayoutUnit(), child.containingBlockLogicalWidthForContent());
    // XXX Make sure this height is correct
    LayoutUnit containerLogicalHeight = child.containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
    if (child.hasOverrideLogicalContentWidth()) {
        containerLogicalWidth = child.overrideLogicalContentWidth();
        fixedInline = true;
    }
    if (child.hasOverrideLogicalContentHeight()) {
        containerLogicalWidth = child.overrideLogicalContentHeight();
        fixedBlock = true;
    }
    NGConstraintSpace space(containerLogicalWidth, containerLogicalHeight);
    // XXX vertical writing mode
    space.setOverflowTriggersScrollbar(child.styleRef().overflowX() == OverflowAuto,
        child.styleRef().overflowY() == OverflowAuto);
    space.setFixedSize(fixedInline, fixedBlock);
    return space;
}

} // namespace blink
