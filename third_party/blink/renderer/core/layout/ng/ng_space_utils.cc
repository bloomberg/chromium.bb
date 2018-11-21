// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

bool AdjustToClearance(LayoutUnit clearance_offset, NGBfcOffset* offset) {
  DCHECK(offset);
  if (clearance_offset > offset->block_offset) {
    offset->block_offset = clearance_offset;
    return true;
  }

  return false;
}

NGConstraintSpace CreateIndefiniteConstraintSpaceForChild(
    const ComputedStyle& container_style,
    NGLayoutInputNode child) {
  NGLogicalSize indefinite_size(NGSizeIndefinite, NGSizeIndefinite);
  LayoutUnit fallback_inline_size = NGSizeIndefinite;
  WritingMode parent_writing_mode = container_style.GetWritingMode();
  WritingMode child_writing_mode = child.Style().GetWritingMode();
  if (!IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    fallback_inline_size = CalculateOrthogonalFallbackInlineSize(
        container_style, child.InitialContainingBlockSize());
  }

  return NGConstraintSpaceBuilder(parent_writing_mode, child_writing_mode,
                                  child.InitialContainingBlockSize(),
                                  child.CreatesNewFormattingContext())
      .SetOrthogonalFallbackInlineSize(fallback_inline_size)
      .SetAvailableSize(indefinite_size)
      .SetPercentageResolutionSize(indefinite_size)
      .SetReplacedPercentageResolutionSize(indefinite_size)
      .SetIsIntermediateLayout(true)
      .SetFloatsBfcBlockOffset(LayoutUnit())
      .ToConstraintSpace();
}

}  // namespace blink
