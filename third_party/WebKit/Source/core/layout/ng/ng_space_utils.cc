// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_space_utils.h"

#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

bool ShouldShrinkToFit(const ComputedStyle& parent_style,
                       const ComputedStyle& style) {
  // Whether the child and the containing block are parallel to each other.
  // Example: vertical-rl and vertical-lr
  bool is_in_parallel_flow = IsParallelWritingMode(
      FromPlatformWritingMode(parent_style.GetWritingMode()),
      FromPlatformWritingMode(style.GetWritingMode()));

  return style.Display() == EDisplay::kInlineBlock || style.IsFloating() ||
         !is_in_parallel_flow;
}

bool AdjustToClearance(const WTF::Optional<LayoutUnit>& clearance_offset,
                       NGLogicalOffset* offset) {
  DCHECK(offset);
  if (clearance_offset && clearance_offset.value() > offset->block_offset) {
    offset->block_offset = clearance_offset.value();
    return true;
  }

  return false;
}

}  // namespace blink
