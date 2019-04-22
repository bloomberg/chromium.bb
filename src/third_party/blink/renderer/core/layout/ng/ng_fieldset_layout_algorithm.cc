// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

NGFieldsetLayoutAlgorithm::NGFieldsetLayoutAlgorithm(
    NGBlockNode node,
    const NGConstraintSpace& space,
    const NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, To<NGBlockBreakToken>(break_token)) {
  container_builder_.SetIsNewFormattingContext(space.IsNewFormattingContext());
}

scoped_refptr<const NGLayoutResult> NGFieldsetLayoutAlgorithm::Layout() {
  // TODO(mstensho): Support block fragmentation.
  DCHECK(!BreakToken());

  // Layout of a fieldset container consists of two parts: Create a child
  // fragment for the rendered legend (if any), and create a child fragment for
  // the fieldset contents anonymous box (if any). Fieldset scrollbars and
  // padding will not be applied to the fieldset container itself, but rather to
  // the fieldset contents anonymous child box. The reason for this is that the
  // rendered legend shouldn't be part of the scrollport; the legend is
  // essentially a part of the block-start border, and should not scroll along
  // with the actual fieldset contents. Since scrollbars are handled by the
  // anonymous child box, and since padding is inside the scrollport, padding
  // also needs to be handled by the anonymous child.
  NGBoxStrut borders = ComputeBorders(ConstraintSpace(), Node());
  NGBoxStrut padding = ComputePadding(ConstraintSpace(), Style());
  NGBoxStrut border_padding = borders + padding;
  NGLogicalSize border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Node(), border_padding);
  const auto writing_mode = ConstraintSpace().GetWritingMode();
  LayoutUnit block_start_padding_edge = borders.block_start;

  if (NGBlockNode legend = Node().GetRenderedLegend()) {
    // Lay out the legend. While the fieldset container normally ignores its
    // padding, the legend is laid out within what would have been the content
    // box had the fieldset been a regular block with no weirdness.
    NGLogicalSize content_box_size =
        ShrinkAvailableSize(border_box_size, border_padding);
    auto legend_space =
        CreateConstraintSpaceForLegend(legend, content_box_size);
    auto result = legend.Layout(legend_space, BreakToken());
    NGBoxStrut legend_margins =
        ComputeMarginsFor(legend_space, legend.Style(), ConstraintSpace());
    NGFragment logical_fragment(writing_mode, *result->PhysicalFragment());
    // If the margin box of the legend is at least as tall as the fieldset
    // block-start border width, it will start at the block-start border edge of
    // the fieldset. As a paint effect, the block-start border will be pushed so
    // that the center of the border will be flush with the center of the
    // border-box of the legend.
    // TODO(mstensho): inline alignment
    NGLogicalOffset legend_offset = NGLogicalOffset(
        border_padding.inline_start + legend_margins.inline_start,
        legend_margins.block_start);
    LayoutUnit legend_margin_box_block_size =
        logical_fragment.BlockSize() + legend_margins.BlockSum();
    LayoutUnit space_left = borders.block_start - legend_margin_box_block_size;
    if (space_left > LayoutUnit()) {
      // If the border is the larger one, though, it will stay put at the
      // border-box block-start edge of the fieldset. Then it's the legend that
      // needs to be pushed. We'll center the margin box in this case, to make
      // sure that both margins remain within the area occupied by the border
      // also after adjustment.
      legend_offset.block_offset += space_left / 2;
    } else {
      // If the legend is larger than the width of the fieldset block-start
      // border, the actual padding edge of the fieldset will be moved
      // accordingly. This will be the block-start offset for the fieldset
      // contents anonymous box.
      block_start_padding_edge = legend_margin_box_block_size;
    }

    container_builder_.AddChild(*result, legend_offset);
  }

  NGBoxStrut borders_with_legend = borders;
  borders_with_legend.block_start = block_start_padding_edge;
  LayoutUnit intrinsic_block_size = borders_with_legend.BlockSum();

  // Proceed with normal fieldset children (excluding the rendered legend). They
  // all live inside an anonymous child box of the fieldset container.
  if (auto fieldset_content = Node().GetFieldsetContent()) {
    NGLogicalSize adjusted_padding_box_size =
        ShrinkAvailableSize(border_box_size, borders_with_legend);
    auto child_space =
        CreateConstraintSpaceForFieldsetContent(adjusted_padding_box_size);
    auto result = fieldset_content.Layout(child_space, BreakToken());
    container_builder_.AddChild(*result, borders_with_legend.StartOffset());

    NGFragment logical_fragment(writing_mode, *result->PhysicalFragment());
    intrinsic_block_size += logical_fragment.BlockSize();
  } else {
    // There was no anonymous child to provide the padding, so we have to add it
    // ourselves.
    intrinsic_block_size += padding.BlockSum();
  }

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_padding, intrinsic_block_size);

  // The above computation utility knows nothing about fieldset weirdness. The
  // legend may eat from the available content box block size. Make room for
  // that if necessary.
  LayoutUnit minimum_border_box_block_size =
      borders_with_legend.BlockSum() + padding.BlockSum();
  border_box_size.block_size =
      std::max(border_box_size.block_size, minimum_border_box_block_size);

  container_builder_.SetIsFieldsetContainer();
  container_builder_.SetInlineSize(border_box_size.inline_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetBlockSize(border_box_size.block_size);
  container_builder_.SetBorders(borders);
  container_builder_.SetPadding(padding);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), borders_with_legend,
                        &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSize> NGFieldsetLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  MinMaxSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (node_.ShouldApplySizeContainment())
    return sizes;

  if (NGBlockNode legend = Node().GetRenderedLegend()) {
    sizes = ComputeMinAndMaxContentContribution(Style(), legend, input);
    sizes += ComputeMinMaxMargins(Style(), legend).InlineSum();
  }
  // The fieldset content includes the fieldset padding (and any scrollbars),
  // while the legend is a regular child and doesn't. We may have a fieldset
  // without any content or legend, so add the padding here, on the outside.
  sizes += ComputePadding(ConstraintSpace(), node_.Style()).InlineSum();

  if (NGBlockNode content = Node().GetFieldsetContent()) {
    MinMaxSize content_minmax =
        ComputeMinAndMaxContentContribution(Style(), content, input);
    content_minmax += ComputeMinMaxMargins(Style(), content).InlineSum();
    sizes.Encompass(content_minmax);
  }

  sizes += ComputeBorders(ConstraintSpace(), node_).InlineSum();
  return sizes;
}

const NGConstraintSpace
NGFieldsetLayoutAlgorithm::CreateConstraintSpaceForLegend(
    NGBlockNode legend,
    NGLogicalSize available_size) {
  NGConstraintSpaceBuilder builder(
      ConstraintSpace(), legend.Style().GetWritingMode(), /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), legend, &builder);

  builder.SetAvailableSize(available_size);
  NGLogicalSize percentage_size =
      CalculateChildPercentageSize(ConstraintSpace(), Node(), available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetIsShrinkToFit(legend.Style().LogicalWidth().IsAuto());
  builder.SetTextDirection(legend.Style().Direction());
  return builder.ToConstraintSpace();
}

const NGConstraintSpace
NGFieldsetLayoutAlgorithm::CreateConstraintSpaceForFieldsetContent(
    NGLogicalSize padding_box_size) {
  NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                   ConstraintSpace().GetWritingMode(),
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(padding_box_size);
  builder.SetPercentageResolutionSize(
      ConstraintSpace().PercentageResolutionSize());
  builder.SetIsFixedSizeBlock(padding_box_size.block_size != NGSizeIndefinite);
  return builder.ToConstraintSpace();
}

}  // namespace blink
