// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBox firstChild)
    : m_style(style), m_firstChild(firstChild) {}

NGFragment* NGBlockLayoutAlgorithm::layout(
    const NGConstraintSpace& constraintSpace) {
  LayoutUnit inlineSize = valueForLength(m_style->logicalWidth(),
                                         constraintSpace.inlineContainerSize());
  LayoutUnit blockSize = valueForLength(m_style->logicalHeight(),
                                        constraintSpace.blockContainerSize());

  for (NGBox curr = m_firstChild; curr; curr.nextSibling()) {
    curr.layout(constraintSpace);
  }

  return new NGFragment(inlineSize, blockSize, inlineSize, blockSize);
}

}  // namespace blink
