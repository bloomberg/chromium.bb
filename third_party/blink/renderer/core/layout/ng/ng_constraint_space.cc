// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

namespace {

struct SameSizeAsNGConstraintSpace {
  NGLogicalSize available_size;
  union {
    NGBfcOffset bfc_offset;
    void* rare_data;
  };
  NGExclusionSpace exclusion_space;
  unsigned bitfields[1];
};

static_assert(sizeof(NGConstraintSpace) == sizeof(SameSizeAsNGConstraintSpace),
              "NGConstraintSpace should stay small.");

}  // namespace

NGConstraintSpace NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBlock& block) {
  // We should only ever create a constraint space from legacy layout if the
  // object is a new formatting context.
  DCHECK(block.CreatesNewFormattingContext());

  const LayoutBlock* cb = block.ContainingBlock();
  LayoutUnit available_logical_width =
      LayoutBoxUtils::AvailableLogicalWidth(block, cb);
  LayoutUnit available_logical_height =
      LayoutBoxUtils::AvailableLogicalHeight(block, cb);
  NGLogicalSize percentage_size = {available_logical_width,
                                   available_logical_height};
  NGLogicalSize available_size = percentage_size;

  bool fixed_inline = false, fixed_block = false;
  bool fixed_block_is_definite = true;
  if (block.HasOverrideLogicalWidth()) {
    available_size.inline_size = block.OverrideLogicalWidth();
    fixed_inline = true;
  }
  if (block.HasOverrideLogicalHeight()) {
    available_size.block_size = block.OverrideLogicalHeight();
    fixed_block = true;
  }
  if (block.IsFlexItem() && fixed_block) {
    // The flexbox-specific behavior is in addition to regular definite-ness, so
    // if the flex item would normally have a definite height it should keep it.
    fixed_block_is_definite =
        ToLayoutFlexibleBox(block.Parent())
            ->UseOverrideLogicalHeightForPerentageResolution(block) ||
        block.HasDefiniteLogicalHeight();
  }

  const ComputedStyle& style = block.StyleRef();
  auto writing_mode = style.GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);
  NGConstraintSpaceBuilder builder(writing_mode, writing_mode,
                                   /* is_new_fc */ true,
                                   !parallel_containing_block);

  if (!block.IsWritingModeRoot() || block.IsGridItem()) {
    // Add all types because we don't know which baselines will be requested.
    FontBaseline baseline_type = style.GetFontBaseline();
    bool synthesize_inline_block_baseline =
        block.UseLogicalBottomMarginEdgeForInlineBlockBaseline();
    if (!synthesize_inline_block_baseline) {
      builder.AddBaselineRequest(
          {NGBaselineAlgorithmType::kAtomicInline, baseline_type});
    }
    builder.AddBaselineRequest(
        {NGBaselineAlgorithmType::kFirstLine, baseline_type});
  }

  return builder.SetAvailableSize(available_size)
      .SetPercentageResolutionSize(percentage_size)
      .SetIsFixedSizeInline(fixed_inline)
      .SetIsFixedSizeBlock(fixed_block)
      .SetFixedSizeBlockIsDefinite(fixed_block_is_definite)
      .SetIsShrinkToFit(
          style.LogicalWidth().IsAuto() &&
          block.SizesLogicalWidthToFitContent(style.LogicalWidth()))
      .SetTextDirection(style.Direction())
      .ToConstraintSpace();
}

String NGConstraintSpace::ToString() const {
  return String::Format("Offset: %s,%s Size: %sx%s Clearance: %s",
                        bfc_offset_.line_offset.ToString().Ascii().data(),
                        bfc_offset_.block_offset.ToString().Ascii().data(),
                        AvailableSize().inline_size.ToString().Ascii().data(),
                        AvailableSize().block_size.ToString().Ascii().data(),
                        HasClearanceOffset()
                            ? ClearanceOffset().ToString().Ascii().data()
                            : "none");
}

}  // namespace blink
