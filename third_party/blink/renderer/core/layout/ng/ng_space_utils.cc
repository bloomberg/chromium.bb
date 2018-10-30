// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
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
    LayoutUnit container_extrinsic_block_size,
    NGLayoutInputNode child) {
  NGLogicalSize extrinsic_size(NGSizeIndefinite,
                               container_extrinsic_block_size);

  return NGConstraintSpaceBuilder(container_constraint_space)
      .SetAvailableSize(extrinsic_size)
      .SetPercentageResolutionSize(extrinsic_size)
      .SetIsIntermediateLayout(true)
      .SetIsNewFormattingContext(child.CreatesNewFormattingContext())
      .SetFloatsBfcBlockOffset(LayoutUnit())
      .ToConstraintSpace(child.Style().GetWritingMode());
}

}  // namespace blink
