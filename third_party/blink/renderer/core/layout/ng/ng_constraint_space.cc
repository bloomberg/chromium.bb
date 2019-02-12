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
    const LayoutBox& box) {
  const LayoutBlock* cb = box.ContainingBlock();

  LayoutUnit available_logical_width =
      LayoutBoxUtils::AvailableLogicalWidth(box, cb);
  LayoutUnit available_logical_height =
      LayoutBoxUtils::AvailableLogicalHeight(box, cb);
  NGLogicalSize percentage_size = {available_logical_width,
                                   available_logical_height};
  NGLogicalSize available_size = percentage_size;

  bool fixed_inline = false, fixed_block = false;
  bool fixed_block_is_definite = true;
  if (box.HasOverrideLogicalWidth()) {
    available_size.inline_size = box.OverrideLogicalWidth();
    fixed_inline = true;
  }
  if (box.HasOverrideLogicalHeight()) {
    available_size.block_size = box.OverrideLogicalHeight();
    fixed_block = true;
  }
  if (box.IsFlexItem() && fixed_block) {
    // The flexbox-specific behavior is in addition to regular definite-ness, so
    // if the flex item would normally have a definite height it should keep it.
    fixed_block_is_definite =
        ToLayoutFlexibleBox(box.Parent())
            ->UseOverrideLogicalHeightForPerentageResolution(box) ||
        (box.IsLayoutBlock() && ToLayoutBlock(box).HasDefiniteLogicalHeight());
  }

  bool is_new_fc = true;
  // TODO(ikilpatrick): This DCHECK needs to be enabled once we've switched
  // LayoutTableCell, etc over to LayoutNG.
  //
  // We currently need to "force" LayoutNG roots to be formatting contexts so
  // that floats have layout performed on them.
  //
  // DCHECK(is_new_fc,
  //  box.IsLayoutBlock() && ToLayoutBlock(box).CreatesNewFormattingContext());

  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);
  NGConstraintSpaceBuilder builder(writing_mode, writing_mode, is_new_fc,
                                   !parallel_containing_block);

  if (!box.IsWritingModeRoot() || box.IsGridItem()) {
    // Add all types because we don't know which baselines will be requested.
    FontBaseline baseline_type = box.StyleRef().GetFontBaseline();
    bool synthesize_inline_block_baseline =
        box.IsLayoutBlock() &&
        ToLayoutBlock(box).UseLogicalBottomMarginEdgeForInlineBlockBaseline();
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
          box.SizesLogicalWidthToFitContent(box.StyleRef().LogicalWidth()))
      .SetTextDirection(box.StyleRef().Direction())
      .ToConstraintSpace();
}

bool NGConstraintSpace::operator==(const NGConstraintSpace& other) const {
  if (!AreSizesEqual(other))
    return false;

  if (!MaySkipLayout(other))
    return false;

  if (!HasRareData() && !other.HasRareData() &&
      bfc_offset_.block_offset != other.bfc_offset_.block_offset)
    return false;

  return true;
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
