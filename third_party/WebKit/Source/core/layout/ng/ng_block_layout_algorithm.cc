// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_margin_strut.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBoxIterator boxIterator)
    : m_style(style), m_boxIterator(boxIterator) {}

NGFragment* NGBlockLayoutAlgorithm::layout(
    const NGConstraintSpace& constraintSpace) {
  LayoutUnit inlineSize =
      computeInlineSizeForFragment(constraintSpace, *m_style);

  HeapVector<Member<const NGFragmentBase>> childFragments;
  LayoutUnit contentSize;
  for (NGBox box : m_boxIterator) {
    NGBoxMargins childMargins = computeMargins(constraintSpace, *box.style());
    NGFragment* fragment = box.layout(constraintSpace);
    // TODO(layout-ng): Support auto margins
    fragment->setOffset(childMargins.inlineStart,
                        contentSize + childMargins.blockStart);
    box.positionUpdated(*fragment);
    contentSize += fragment->blockSize() + childMargins.blockSum();
    childFragments.append(fragment);
  }

  LayoutUnit blockSize =
      computeBlockSizeForFragment(constraintSpace, *m_style, contentSize);
  NGFragment* returnFragment =
      new NGFragment(inlineSize, blockSize, inlineSize, blockSize,
                     HorizontalTopBottom, LeftToRight);
  returnFragment->swapChildren(childFragments);
  return returnFragment;
}

}  // namespace blink
