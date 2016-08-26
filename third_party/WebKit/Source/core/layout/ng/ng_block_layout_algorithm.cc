// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBoxIterator box_iterator)
    : style_(style), box_iterator_(box_iterator) {}

bool NGBlockLayoutAlgorithm::Layout(const NGConstraintSpace* constraint_space,
                                    NGFragment** out) {
  LayoutUnit inline_size =
      computeInlineSizeForFragment(*constraint_space, *style_);
  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of -1?
  LayoutUnit block_size =
      computeBlockSizeForFragment(*constraint_space, *style_, LayoutUnit(-1));
  NGConstraintSpace* constraint_space_for_children = new NGConstraintSpace(
      *constraint_space, NGLogicalSize(inline_size, block_size));

  NGFragmentBuilder builder(NGFragmentBase::FragmentBox);
  builder.SetInlineSize(inline_size).SetBlockSize(block_size);

  LayoutUnit content_size;
  for (NGBox box : box_iterator_) {
    NGBoxStrut child_margins =
        computeMargins(*constraint_space_for_children, *box.style());
    NGFragment* fragment;
    // TODO(layout-ng): Actually make this async
    while (!box.Layout(constraint_space_for_children, &fragment))
      ;
    // TODO(layout-ng): Support auto margins
    fragment->SetOffset(child_margins.inline_start,
                        content_size + child_margins.block_start);
    box.positionUpdated(*fragment);
    content_size += fragment->BlockSize() + child_margins.BlockSum();
    builder.AddChild(fragment);
  }

  // Recompute the block-axis size now that we know our content size.
  block_size =
      computeBlockSizeForFragment(*constraint_space, *style_, content_size);

  // TODO(layout-ng): Compute correct inline overflow (block overflow should be
  // correct)
  builder.SetBlockSize(block_size)
      .SetInlineOverflow(inline_size)
      .SetBlockOverflow(content_size);
  *out = builder.ToFragment();
  return true;
}

}  // namespace blink
