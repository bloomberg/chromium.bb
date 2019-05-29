// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

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

NGStaticPosition LayoutBoxUtils::ComputeStaticPositionFromLegacy(
    const LayoutBox& box,
    const NGBoxFragmentBuilder* container_builder) {
  LayoutBoxModelObject* css_container = ToLayoutBoxModelObject(box.Container());
  LayoutBox* container = css_container->IsBox() ? ToLayoutBox(css_container)
                                                : box.ContainingBlock();
  const ComputedStyle* container_style = container->Style();
  const ComputedStyle* parent_style = box.Parent()->Style();
  const auto writing_mode = container_style->GetWritingMode();

  // Calculate the actual size of the containing block for this out-of-flow
  // descendant. This is what's used to size and position us.
  LayoutUnit containing_block_logical_width =
      box.ContainingBlockLogicalWidthForPositioned(css_container);
  LayoutUnit containing_block_logical_height =
      box.ContainingBlockLogicalHeightForPositioned(css_container);

  // Determine static position.

  // static_inline and static_block are inline/block direction offsets from
  // physical origin. This is an unexpected blend of logical and physical in a
  // single variable.
  LayoutUnit static_inline;
  LayoutUnit static_block;

  Length logical_left;
  Length logical_right;
  Length logical_top;
  Length logical_bottom;

  box.ComputeInlineStaticDistance(logical_left, logical_right, &box,
                                  css_container, containing_block_logical_width,
                                  container_builder);
  box.ComputeBlockStaticDistance(logical_top, logical_bottom, &box,
                                 css_container, container_builder);

  if (parent_style->IsLeftToRightDirection()) {
    if (!logical_left.IsAuto()) {
      static_inline =
          MinimumValueForLength(logical_left, containing_block_logical_width);
    }
  } else {
    if (!logical_right.IsAuto()) {
      static_inline =
          MinimumValueForLength(logical_right, containing_block_logical_width);
    }
  }
  if (!logical_top.IsAuto()) {
    static_block =
        MinimumValueForLength(logical_top, containing_block_logical_height);
  }

  // Legacy static position is relative to padding box. Convert to border box.
  // Also flip offsets as necessary to make them relative to to the left/top
  // edges.

  // First convert the static inline offset to a line-left offset, i.e.
  // physical left or top.
  LayoutUnit inline_left_or_top =
      parent_style->IsLeftToRightDirection()
          ? static_inline
          : containing_block_logical_width - static_inline;

  // inline_left_or_top is now relative to the padding box.
  // Make it relative to border box by adding border + scrollbar.
  NGBlockNode container_node(container);
  NGConstraintSpace non_anonymous_space =
      NGConstraintSpaceBuilder(writing_mode, writing_mode,
                               /* is_new_fc */ false)
          .ToConstraintSpace();
  NGBoxStrut border_scrollbar =
      ComputeBorders(non_anonymous_space, container_node) +
      ComputeScrollbars(non_anonymous_space, container_node);

  // Now make it relative to the left or top border edge of the containing
  // block.
  inline_left_or_top += border_scrollbar.LineLeft(container_style->Direction());

  // Make the static block offset relative to the start border edge of the
  // containing block.
  static_block += border_scrollbar.block_start;

  // Calculate the border-box size of the object that's the containing block of
  // this out-of-flow positioned descendant. This is needed to correct
  // flipped-blocks coordinates.
  LayoutUnit container_border_box_logical_height;
  if (box.HasOverrideContainingBlockContentLogicalHeight()) {
    container_border_box_logical_height =
        box.OverrideContainingBlockContentLogicalHeight() +
        border_scrollbar.BlockSum();
  } else {
    container_border_box_logical_height = container_builder
                                              ? container_builder->BlockSize()
                                              : container->LogicalHeight();
  }

  // Then convert it to a physical top or left offset. Since we're already
  // border-box relative, flip it around the size of the border box, rather
  // than the size of the containing block (padding box).
  LayoutUnit block_top_or_left =
      container_style->IsFlippedBlocksWritingMode()
          ? container_border_box_logical_height - static_block
          : static_block;

  PhysicalOffset static_location =
      container_style->IsHorizontalWritingMode()
          ? PhysicalOffset(inline_left_or_top, block_top_or_left)
          : PhysicalOffset(block_top_or_left, inline_left_or_top);

  return NGStaticPosition::Create(writing_mode, parent_style->Direction(),
                                  static_location);
}

bool LayoutBoxUtils::SkipContainingBlockForPercentHeightCalculation(
    const LayoutBlock* cb) {
  return LayoutBox::SkipContainingBlockForPercentHeightCalculation(cb);
}

}  // namespace blink
