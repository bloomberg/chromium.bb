// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

LayoutUnit LayoutBoxUtils::AvailableLogicalWidth(const LayoutBox& box,
                                                 const LayoutBlock* cb) {
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);

  // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth()) {
    return box.OverrideContainingBlockContentLogicalWidth()
        .ClampNegativeToZero();
  }

  if (!parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalHeight()) {
    return box.OverrideContainingBlockContentLogicalHeight()
        .ClampNegativeToZero();
  }

  if (parallel_containing_block)
    return box.ContainingBlockLogicalWidthForContent().ClampNegativeToZero();

  return box.PerpendicularContainingBlockLogicalHeight().ClampNegativeToZero();
}

LayoutUnit LayoutBoxUtils::AvailableLogicalHeight(const LayoutBox& box,
                                                  const LayoutBlock* cb) {
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);

  // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalHeight())
    return box.OverrideContainingBlockContentLogicalHeight();

  if (!parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth())
    return box.OverrideContainingBlockContentLogicalWidth();

  if (!box.Parent())
    return box.View()->ViewLogicalHeightForPercentages();

  DCHECK(cb);
  if (parallel_containing_block)
    return box.ContainingBlockLogicalHeightForPercentageResolution();

  return box.ContainingBlockLogicalWidthForContent();
}

}  // namespace blink
