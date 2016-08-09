// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/NGBlockLayoutAlgorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/NGConstraintSpace.h"
#include "core/layout/ng/NGFragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(const ComputedStyle* style)
    : m_style(style)
{
}

NGFragment* NGBlockLayoutAlgorithm::layout(
    const NGConstraintSpace& constraintSpace)
{
    LayoutUnit inlineSize = valueForLength(m_style->logicalWidth(),
        constraintSpace.inlineContainerSize());
    LayoutUnit blockSize = valueForLength(m_style->logicalHeight(),
        constraintSpace.blockContainerSize());
    return new NGFragment(inlineSize, blockSize, inlineSize, blockSize);
}

} // namespace blink
