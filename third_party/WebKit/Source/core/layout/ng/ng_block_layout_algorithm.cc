// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
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
  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of -1?
  LayoutUnit blockSize =
      computeBlockSizeForFragment(constraintSpace, *m_style, LayoutUnit(-1));
  NGConstraintSpace constraint_space_for_children(
      constraintSpace, NGLogicalSize(inlineSize, blockSize));

  NGFragmentBuilder builder(NGFragmentBase::FragmentBox);
  builder.SetInlineSize(inlineSize).SetBlockSize(blockSize);

  LayoutUnit contentSize;
  for (NGBox box : m_boxIterator) {
    NGBoxMargins childMargins =
        computeMargins(constraint_space_for_children, *box.style());
    NGFragment* fragment = box.layout(constraint_space_for_children);
    // TODO(layout-ng): Support auto margins
    fragment->SetOffset(childMargins.inlineStart,
                        contentSize + childMargins.blockStart);
    box.positionUpdated(*fragment);
    contentSize += fragment->BlockSize() + childMargins.blockSum();
    builder.AddChild(fragment);
  }

  // Recompute the block-axis size now that we know our content size.
  blockSize =
      computeBlockSizeForFragment(constraintSpace, *m_style, contentSize);
  // TODO(layout-ng): Compute correct inline overflow (block overflow should be
  // correct)
  builder.SetBlockSize(blockSize)
      .SetInlineOverflow(inlineSize)
      .SetBlockOverflow(contentSize);
  return builder.ToFragment();
}

}  // namespace blink
