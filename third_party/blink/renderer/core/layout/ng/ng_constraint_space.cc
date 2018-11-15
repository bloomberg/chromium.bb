// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

namespace {

struct SameSizeAsNGConstraintSpace {
  NGLogicalSize logical_sizes[3];
  NGPhysicalSize physical_sizes[1];
  NGMarginStrut margin_strut;
  NGBfcOffset bfc_offset;
  NGExclusionSpace exclusion_space;
  base::Optional<LayoutUnit> optional_layout_unit;
  LayoutUnit layout_units[3];
  unsigned flags[1];
};

static_assert(sizeof(NGConstraintSpace) == sizeof(SameSizeAsNGConstraintSpace),
              "NGConstraintSpace should stay small.");

}  // namespace

NGConstraintSpace::NGConstraintSpace(WritingMode out_writing_mode,
                                     NGPhysicalSize icb_size)
    : initial_containing_block_size_(icb_size),
      block_direction_fragmentation_type_(static_cast<unsigned>(kFragmentNone)),
      table_cell_child_layout_phase_(static_cast<unsigned>(kNotTableCellChild)),
      adjoining_floats_(static_cast<unsigned>(kFloatTypeNone)),
      writing_mode_(static_cast<unsigned>(out_writing_mode)),
      direction_(static_cast<unsigned>(TextDirection::kLtr)),
      flags_(kFixedSizeBlockIsDefinite) {}

NGConstraintSpace NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      box.ContainingBlock()->StyleRef().GetWritingMode(), writing_mode);
  bool fixed_inline = false, fixed_block = false;
  bool fixed_block_is_definite = true;

  LayoutUnit available_logical_width;
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth()) {
    // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
    available_logical_width = box.OverrideContainingBlockContentLogicalWidth();
  } else if (!parallel_containing_block &&
             box.HasOverrideContainingBlockContentLogicalHeight()) {
    available_logical_width = box.OverrideContainingBlockContentLogicalHeight();
  } else {
    if (parallel_containing_block)
      available_logical_width = box.ContainingBlockLogicalWidthForContent();
    else
      available_logical_width = box.PerpendicularContainingBlockLogicalHeight();
  }
  available_logical_width = std::max(LayoutUnit(), available_logical_width);

  LayoutUnit available_logical_height;
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalHeight()) {
    // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
    available_logical_height =
        box.OverrideContainingBlockContentLogicalHeight();
  } else if (!parallel_containing_block &&
             box.HasOverrideContainingBlockContentLogicalWidth()) {
    available_logical_height = box.OverrideContainingBlockContentLogicalWidth();
  } else {
    if (!box.Parent()) {
      available_logical_height = box.View()->ViewLogicalHeightForPercentages();
    } else if (box.ContainingBlock()) {
      if (parallel_containing_block) {
        available_logical_height =
            box.ContainingBlockLogicalHeightForPercentageResolution();
      } else {
        available_logical_height = box.ContainingBlockLogicalWidthForContent();
      }
    }
  }
  NGLogicalSize percentage_size = {available_logical_width,
                                   available_logical_height};
  NGLogicalSize available_size = percentage_size;
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

  IntSize int_icb_size = box.View()->GetLayoutSize(kExcludeScrollbars);
  NGPhysicalSize icb_size{LayoutUnit(int_icb_size.Width()),
                          LayoutUnit(int_icb_size.Height())};

  // The initial containing block size cannot be indefinite.
  DCHECK_GE(icb_size.width, LayoutUnit());
  DCHECK_GE(icb_size.height, LayoutUnit());

  NGConstraintSpaceBuilder builder(writing_mode, writing_mode, icb_size,
                                   is_new_fc, !parallel_containing_block);

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
  return AreSizesEqual(other) &&
         bfc_offset_.block_offset == other.bfc_offset_.block_offset &&
         initial_containing_block_size_ ==
             other.initial_containing_block_size_ &&
         MaySkipLayout(other);
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
