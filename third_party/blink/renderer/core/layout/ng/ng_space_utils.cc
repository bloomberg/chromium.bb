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

NGConstraintSpace CreateExtrinsicConstraintSpaceForChild(
    const NGConstraintSpace& container_constraint_space,
    const ComputedStyle& container_style,
    LayoutUnit container_extrinsic_block_size,
    NGLayoutInputNode child) {
  NGLogicalSize extrinsic_size(NGSizeIndefinite,
                               container_extrinsic_block_size);
  LayoutUnit fallback_inline_size = CalculateOrthogonalFallbackInlineSize(
      container_style, container_constraint_space.InitialContainingBlockSize());

  return NGConstraintSpaceBuilder(container_constraint_space,
                                  child.Style().GetWritingMode(),
                                  child.CreatesNewFormattingContext())
      .SetOrthogonalFallbackInlineSize(fallback_inline_size)
      .SetAvailableSize(extrinsic_size)
      .SetPercentageResolutionSize(extrinsic_size)
      .SetIsIntermediateLayout(true)
      .SetFloatsBfcBlockOffset(LayoutUnit())
      .ToConstraintSpace();
}

}  // namespace blink
