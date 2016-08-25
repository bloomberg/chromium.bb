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
    NGBoxIterator box_iterator)
    : m_style(style), m_boxIterator(box_iterator) {}

NGFragment* NGBlockLayoutAlgorithm::layout(
    const NGConstraintSpace& constraint_space) {
  LayoutUnit inline_size =
      computeInlineSizeForFragment(constraint_space, *m_style);
  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of -1?
  LayoutUnit block_size =
      computeBlockSizeForFragment(constraint_space, *m_style, LayoutUnit(-1));
  NGConstraintSpace constraint_space_for_children(
      constraint_space, NGLogicalSize(inline_size, block_size));

  NGFragmentBuilder builder(NGFragmentBase::FragmentBox);
  builder.SetInlineSize(inline_size).SetBlockSize(block_size);

  LayoutUnit content_size;
  for (NGBox box : m_boxIterator) {
    NGBoxMargins child_margins =
        computeMargins(constraint_space_for_children, *box.style());
    NGFragment* fragment = box.layout(constraint_space_for_children);
    // TODO(layout-ng): Support auto margins
    fragment->SetOffset(child_margins.inline_start,
                        content_size + child_margins.block_start);
    box.positionUpdated(*fragment);
    content_size += fragment->BlockSize() + child_margins.BlockSum();
    builder.AddChild(fragment);
  }

  // Recompute the block-axis size now that we know our content size.
  block_size =
      computeBlockSizeForFragment(constraint_space, *m_style, content_size);
  // TODO(layout-ng): Compute correct inline overflow (block overflow should be
  // correct)
  builder.SetBlockSize(block_size)
      .SetInlineOverflow(inline_size)
      .SetBlockOverflow(content_size);
  return builder.ToFragment();
}

}  // namespace blink
