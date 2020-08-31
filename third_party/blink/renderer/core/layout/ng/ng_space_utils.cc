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
  WritingMode parent_writing_mode = container_style.GetWritingMode();
  WritingMode child_writing_mode = child.Style().GetWritingMode();
  DCHECK(!IsParallelWritingMode(parent_writing_mode, child_writing_mode));

  LogicalSize indefinite_size(kIndefiniteSize, kIndefiniteSize);
  NGConstraintSpaceBuilder builder(parent_writing_mode, child_writing_mode,
                                   child.CreatesNewFormattingContext());
  SetOrthogonalFallbackInlineSizeIfNeeded(container_style, child, &builder);

  builder.SetCacheSlot(NGCacheSlot::kMeasure);
  builder.SetAvailableSize(indefinite_size);
  builder.SetPercentageResolutionSize(indefinite_size);
  builder.SetReplacedPercentageResolutionSize(indefinite_size);
  builder.SetIsShrinkToFit(child.Style().LogicalWidth().IsAuto());
  return builder.ToConstraintSpace();
}

void SetOrthogonalFallbackInlineSizeIfNeeded(
    const ComputedStyle& parent_style,
    const NGLayoutInputNode child,
    NGConstraintSpaceBuilder* builder) {
  if (IsParallelWritingMode(parent_style.GetWritingMode(),
                            child.Style().GetWritingMode()))
    return;

  PhysicalSize orthogonal_children_containing_block_size =
      child.InitialContainingBlockSize();

  LayoutUnit fallback_size;
  if (IsHorizontalWritingMode(parent_style.GetWritingMode()))
    fallback_size = orthogonal_children_containing_block_size.height;
  else
    fallback_size = orthogonal_children_containing_block_size.width;

  LayoutUnit size(LayoutUnit::Max());
  if (parent_style.LogicalHeight().IsFixed()) {
    // Note that during layout, fixed size will already be taken care of (and
    // set in the constraint space), but when calculating intrinsic sizes of
    // orthogonal children, that won't be the case.
    size = LayoutUnit(parent_style.LogicalHeight().GetFloatValue());
  }
  if (parent_style.LogicalMaxHeight().IsFixed()) {
    size = std::min(
        size, LayoutUnit(parent_style.LogicalMaxHeight().GetFloatValue()));
  }
  if (parent_style.LogicalMinHeight().IsFixed()) {
    size = std::max(
        size, LayoutUnit(parent_style.LogicalMinHeight().GetFloatValue()));
  }
  // Calculate the content-box size.
  if (parent_style.BoxSizing() == EBoxSizing::kBorderBox) {
    // We're unable to resolve percentages at this point, so make sure we're
    // only dealing with fixed-size values.
    if (!parent_style.PaddingBefore().IsFixed() ||
        !parent_style.PaddingAfter().IsFixed()) {
      builder->SetOrthogonalFallbackInlineSize(fallback_size);
      return;
    }

    LayoutUnit border_padding(parent_style.BorderBefore().Width() +
                              parent_style.BorderAfter().Width() +
                              parent_style.PaddingBefore().GetFloatValue() +
                              parent_style.PaddingAfter().GetFloatValue());

    size -= border_padding;
    size = size.ClampNegativeToZero();
  }

  fallback_size = std::min(fallback_size, size);
  builder->SetOrthogonalFallbackInlineSize(fallback_size);
}

}  // namespace blink
