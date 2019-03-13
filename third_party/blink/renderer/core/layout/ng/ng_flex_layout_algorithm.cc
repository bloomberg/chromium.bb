// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_flex_layout_algorithm.h"

#include <memory>
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

NGFlexLayoutAlgorithm::NGFlexLayoutAlgorithm(NGBlockNode node,
                                             const NGConstraintSpace& space,
                                             const NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)),
      border_scrollbar_padding_(
          CalculateBorderScrollbarPadding(ConstraintSpace(), Node())),
      borders_(ComputeBorders(ConstraintSpace(), Node())),
      padding_(ComputePadding(ConstraintSpace(), Style())),
      border_padding_(borders_ + padding_),
      is_column_(Style().IsColumnFlexDirection()) {
  container_builder_.SetIsNewFormattingContext(space.IsNewFormattingContext());
}

bool NGFlexLayoutAlgorithm::MainAxisIsInlineAxis(NGBlockNode child) {
  return child.Style().IsHorizontalWritingMode() ==
         FlexLayoutAlgorithm::IsHorizontalFlow(Style());
}

LayoutUnit NGFlexLayoutAlgorithm::MainAxisContentExtent(
    LayoutUnit sum_hypothetical_main_size) {
  if (Style().IsColumnFlexDirection()) {
    return ComputeBlockSizeForFragment(
               ConstraintSpace(), Style(), border_padding_,
               sum_hypothetical_main_size + (border_padding_).BlockSum()) -
           border_scrollbar_padding_.BlockSum();
  }
  return content_box_size_.inline_size;
}

void NGFlexLayoutAlgorithm::HandleOutOfFlowPositioned(NGBlockNode child) {
  // TODO(dgrogan): There's stuff from
  // https://www.w3.org/TR/css-flexbox-1/#abspos-items that isn't done here.
  // Specifically, neither rtl nor alignment is handled here, at least.
  // Look at LayoutFlexibleBox::PrepareChildForPositionedLayout and
  // SetStaticPositionForPositionedLayout to see how to statically position
  // this.
  container_builder_.AddOutOfFlowChildCandidate(
      child, {border_scrollbar_padding_.inline_start,
              border_scrollbar_padding_.block_start});
}

void NGFlexLayoutAlgorithm::ConstructAndAppendFlexItems() {
  const bool is_horizontal_flow = algorithm_->IsHorizontalFlow();
  for (NGLayoutInputNode generic_child = Node().FirstChild(); generic_child;
       generic_child = generic_child.NextSibling()) {
    NGBlockNode child = ToNGBlockNode(generic_child);
    if (child.IsOutOfFlowPositioned()) {
      HandleOutOfFlowPositioned(child);
      continue;
    }

    const ComputedStyle& child_style = child.Style();
    NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                           child_style.GetWritingMode(),
                                           /* is_new_fc */ true);
    SetOrthogonalFallbackInlineSizeIfNeeded(Style(), child, &space_builder);

    // TODO(dgrogan): Set IsShrinkToFit here when cross axis size is auto, at
    // least for correctness. For perf, don't set it if the item will later be
    // stretched or we won't hit the cache later.
    NGConstraintSpace child_space =
        space_builder.SetAvailableSize(content_box_size_)
            .SetPercentageResolutionSize(content_box_size_)
            .ToConstraintSpace();

    NGBoxStrut border_padding_in_child_writing_mode =
        ComputeBorders(child_space, child) +
        ComputePadding(child_space, child_style);
    NGPhysicalBoxStrut physical_border_padding(
        border_padding_in_child_writing_mode.ConvertToPhysical(
            child_style.GetWritingMode(), child_style.Direction()));
    LayoutUnit main_axis_border_and_padding =
        is_horizontal_flow ? physical_border_padding.HorizontalSum()
                           : physical_border_padding.VerticalSum();

    // We want the child's min/max size in its writing mode, not ours. We'll
    // only ever use it if the child's inline axis is our main axis.
    MinMaxSizeInput input(
        /* percentage_resolution_block_size */ content_box_size_.block_size);
    MinMaxSize intrinsic_sizes_border_box = child.ComputeMinMaxSize(
        child_style.GetWritingMode(), input, &child_space);
    // TODO(dgrogan): Don't layout every time, just when you need to.
    scoped_refptr<const NGLayoutResult> layout_result =
        child.Layout(child_space, nullptr /*break token*/);
    NGFragment fragment_in_child_writing_mode(
        child_style.GetWritingMode(), *layout_result->PhysicalFragment());

    LayoutUnit flex_base_border_box;
    const Length& length_in_main_axis =
        is_horizontal_flow ? child_style.Width() : child_style.Height();
    const Length& flex_basis = child_style.FlexBasis();
    if (flex_basis.IsAuto() && length_in_main_axis.IsAuto()) {
      if (MainAxisIsInlineAxis(child))
        flex_base_border_box = intrinsic_sizes_border_box.max_size;
      else
        flex_base_border_box = fragment_in_child_writing_mode.BlockSize();
    } else {
      const Length& length_to_resolve =
          flex_basis.IsAuto() ? length_in_main_axis : flex_basis;
      DCHECK(!length_to_resolve.IsAuto());

      if (MainAxisIsInlineAxis(child)) {
        flex_base_border_box = ResolveInlineLength(
            child_space, child_style, border_padding_in_child_writing_mode,
            intrinsic_sizes_border_box, length_to_resolve,
            LengthResolveType::kContentSize, LengthResolvePhase::kLayout);
      } else {
        // Flex container's main axis is in child's block direction. Child's
        // flex basis is in child's block direction.
        flex_base_border_box = ResolveBlockLength(
            child_space, child_style, border_padding_in_child_writing_mode,
            length_to_resolve, fragment_in_child_writing_mode.BlockSize(),
            LengthResolveType::kContentSize, LengthResolvePhase::kLayout);
      }
    }

    // Spec calls this "flex base size"
    // https://www.w3.org/TR/css-flexbox-1/#algo-main-item
    // Blink's FlexibleBoxAlgorithm expects it to be content + scrollbar widths,
    // but no padding or border.
    LayoutUnit flex_base_content_size =
        flex_base_border_box - main_axis_border_and_padding;

    NGPhysicalBoxStrut physical_child_margins =
        ComputePhysicalMargins(child_space, child_style);
    LayoutUnit main_axis_margin = is_horizontal_flow
                                      ? physical_child_margins.HorizontalSum()
                                      : physical_child_margins.VerticalSum();

    MinMaxSize min_max_sizes_in_main_axis_direction{LayoutUnit(),
                                                    LayoutUnit::Max()};
    const Length& max = is_horizontal_flow ? child.Style().MaxWidth()
                                           : child.Style().MaxHeight();
    if (MainAxisIsInlineAxis(child)) {
      min_max_sizes_in_main_axis_direction.max_size = ResolveInlineLength(
          child_space, child_style, border_padding_in_child_writing_mode,
          intrinsic_sizes_border_box, max, LengthResolveType::kMaxSize,
          LengthResolvePhase::kLayout);
    } else {
      min_max_sizes_in_main_axis_direction.max_size = ResolveBlockLength(
          child_space, child_style, border_padding_in_child_writing_mode, max,
          fragment_in_child_writing_mode.BlockSize(),
          LengthResolveType::kMaxSize, LengthResolvePhase::kLayout);
    }

    const Length& min = is_horizontal_flow ? child.Style().MinWidth()
                                           : child.Style().MinHeight();
    if (min.IsAuto()) {
      if (algorithm_->ShouldApplyMinSizeAutoForChild(*child.GetLayoutBox())) {
        // TODO(dgrogan): Port logic from
        // https://www.w3.org/TR/css-flexbox-1/#min-size-auto and
        // LayoutFlexibleBox::ComputeMinAndMaxSizesForChild
      }
    } else if (MainAxisIsInlineAxis(child)) {
      min_max_sizes_in_main_axis_direction.min_size = ResolveInlineLength(
          child_space, child_style, border_padding_in_child_writing_mode,
          intrinsic_sizes_border_box, min, LengthResolveType::kMinSize,
          LengthResolvePhase::kLayout);
    } else {
      min_max_sizes_in_main_axis_direction.min_size = ResolveBlockLength(
          child_space, child_style, border_padding_in_child_writing_mode, min,
          fragment_in_child_writing_mode.BlockSize(),
          LengthResolveType::kMinSize, LengthResolvePhase::kLayout);
    }

    algorithm_
        ->emplace_back(child.GetLayoutBox(), flex_base_content_size,
                       min_max_sizes_in_main_axis_direction,
                       main_axis_border_and_padding, main_axis_margin)
        .ng_input_node = child;
  }
}

scoped_refptr<const NGLayoutResult> NGFlexLayoutAlgorithm::Layout() {
  border_box_size_ =
      CalculateBorderBoxSize(ConstraintSpace(), Node(), border_padding_);
  content_box_size_ =
      ShrinkAvailableSize(border_box_size_, border_scrollbar_padding_);

  const LayoutUnit line_break_length = MainAxisContentExtent(LayoutUnit::Max());
  algorithm_.emplace(&Style(), line_break_length);
  const bool is_horizontal_flow = algorithm_->IsHorizontalFlow();

  ConstructAndAppendFlexItems();

  LayoutUnit main_axis_offset = border_scrollbar_padding_.inline_start;
  LayoutUnit cross_axis_offset = border_scrollbar_padding_.block_start;
  if (is_column_) {
    main_axis_offset = border_scrollbar_padding_.block_start;
    cross_axis_offset = border_scrollbar_padding_.inline_start;
  }
  FlexLine* line;
  LayoutUnit max_main_axis_extent;
  while (
      (line = algorithm_->ComputeNextFlexLine(border_box_size_.inline_size))) {
    line->SetContainerMainInnerSize(
        MainAxisContentExtent(line->sum_hypothetical_main_size));
    line->FreezeInflexibleItems();
    while (!line->ResolveFlexibleLengths()) {
      continue;
    }
    for (wtf_size_t i = 0; i < line->line_items.size(); ++i) {
      FlexItem& flex_item = line->line_items[i];

      WritingMode child_writing_mode =
          flex_item.ng_input_node.Style().GetWritingMode();
      NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                             child_writing_mode,
                                             /* is_new_fc */ true);
      SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item.ng_input_node,
                                              &space_builder);

      NGLogicalSize available_size;
      if (is_column_) {
        available_size.inline_size = content_box_size_.inline_size;
        available_size.block_size = flex_item.flexed_content_size +
                                    flex_item.main_axis_border_and_padding;
        space_builder.SetIsFixedSizeBlock(true);
      } else {
        available_size.inline_size = flex_item.flexed_content_size +
                                     flex_item.main_axis_border_and_padding;
        available_size.block_size = content_box_size_.block_size;
        space_builder.SetIsFixedSizeInline(true);
      }
      space_builder.SetAvailableSize(available_size);
      space_builder.SetPercentageResolutionSize(content_box_size_);
      NGConstraintSpace child_space = space_builder.ToConstraintSpace();
      flex_item.layout_result =
          flex_item.ng_input_node.Layout(child_space, nullptr /*break token*/);
      flex_item.cross_axis_size =
          is_horizontal_flow
              ? flex_item.layout_result->PhysicalFragment()->Size().height
              : flex_item.layout_result->PhysicalFragment()->Size().width;
    }
    // cross_axis_offset is updated in each iteration of the loop, for passing
    // in to the next iteration.
    line->ComputeLineItemsPosition(main_axis_offset, cross_axis_offset);
    max_main_axis_extent =
        std::max(max_main_axis_extent, line->main_axis_extent);
  }
  LayoutUnit intrinsic_block_content_size =
      is_column_ ? max_main_axis_extent
                 : cross_axis_offset - border_scrollbar_padding_.block_start;
  LayoutUnit intrinsic_block_size =
      intrinsic_block_content_size + border_scrollbar_padding_.BlockSum();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_padding_, intrinsic_block_size);

  container_builder_.SetBlockSize(block_size);
  container_builder_.SetInlineSize(border_box_size_.inline_size);
  container_builder_.SetBorders(borders_);
  container_builder_.SetPadding(padding_);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  GiveLinesAndItemsFinalPositionAndSize();

  NGOutOfFlowLayoutPart(&container_builder_, Node().IsAbsoluteContainer(),
                        Node().IsFixedContainer(),
                        borders_ + Node().GetScrollbarSizes(),
                        ConstraintSpace(), Style())
      .Run();

  return container_builder_.ToBoxFragment();
}

void NGFlexLayoutAlgorithm::GiveLinesAndItemsFinalPositionAndSize() {
  // TODO(dgrogan): This needs to eventually encompass all of the behavior in
  // LayoutFlexibleBox::RepositionLogicalHeightDependentFlexItems, but for now
  // it only does stretch alignment.
  LayoutUnit final_content_cross_size =
      container_builder_.BlockSize() - border_scrollbar_padding_.BlockSum();
  if (is_column_) {
    final_content_cross_size =
        border_box_size_.inline_size - border_scrollbar_padding_.InlineSum();
  }
  if (!algorithm_->IsMultiline() && !algorithm_->FlexLines().IsEmpty())
    algorithm_->FlexLines()[0].cross_axis_extent = final_content_cross_size;

  for (FlexLine& line_context : algorithm_->FlexLines()) {
    for (wtf_size_t child_number = 0;
         child_number < line_context.line_items.size(); ++child_number) {
      FlexItem& flex_item = line_context.line_items[child_number];
      if (flex_item.Alignment() == ItemPosition::kStretch) {
        flex_item.ComputeStretchedSize();

        WritingMode child_writing_mode =
            flex_item.ng_input_node.Style().GetWritingMode();
        NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                               child_writing_mode,
                                               /* is_new_fc */ true);
        SetOrthogonalFallbackInlineSizeIfNeeded(
            Style(), flex_item.ng_input_node, &space_builder);

        NGLogicalSize available_size(flex_item.flexed_content_size +
                                         flex_item.main_axis_border_and_padding,
                                     flex_item.cross_axis_size);
        if (is_column_)
          available_size.Flip();
        space_builder.SetAvailableSize(available_size);
        space_builder.SetPercentageResolutionSize(content_box_size_);
        space_builder.SetIsFixedSizeInline(true);
        space_builder.SetIsFixedSizeBlock(true);
        NGConstraintSpace child_space = space_builder.ToConstraintSpace();
        flex_item.layout_result = flex_item.ng_input_node.Layout(
            child_space, /* break_token */ nullptr);
      }
      // TODO(dgrogan): Add an extra pass for kColumnReverse containers like
      // legacy does in LayoutColumnReverse.

      // AddChild treats location parameter as logical offset from parent rect.
      // TODO(dgrogan): Does this need to transpose the location for
      // non-horizontal flexboxes, like
      // LayoutFlexibleBox::SetFlowAwareLocationForChild does?
      container_builder_.AddChild(
          *flex_item.layout_result,
          {flex_item.desired_location.X(), flex_item.desired_location.Y()});
    }
  }
}

base::Optional<MinMaxSize> NGFlexLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  MinMaxSize sizes;
  if (Node().ShouldApplySizeContainment()) {
    // TODO(dgrogan): When this code was written it didn't make any more tests
    // pass, so it may be wrong or untested.
    if (input.size_type == NGMinMaxSizeType::kBorderBoxSize)
      sizes = border_scrollbar_padding_.InlineSum();
    return sizes;
  }

  LayoutUnit child_percentage_resolution_block_size =
      CalculateChildPercentageBlockSizeForMinMax(
          ConstraintSpace(), Node(), borders_ + padding_,
          input.percentage_resolution_block_size);

  // Use default MinMaxSizeInput:
  //   - Children of flexbox ignore any specified float properties, so children
  //     never have to take floated siblings into account, and external floats
  //     don't make it through the new formatting context that flexbox
  //     establishes.
  //   - We want the child's border box MinMaxSize, which is the default.
  MinMaxSizeInput child_input(child_percentage_resolution_block_size);

  for (NGLayoutInputNode generic_child = Node().FirstChild(); generic_child;
       generic_child = generic_child.NextSibling()) {
    NGBlockNode child = ToNGBlockNode(generic_child);
    if (child.IsOutOfFlowPositioned())
      continue;

    MinMaxSize child_min_max_sizes =
        ComputeMinAndMaxContentContribution(Style(), child, child_input);
    NGBoxStrut child_margins = ComputeMinMaxMargins(Style(), child);
    child_min_max_sizes += child_margins.InlineSum();
    if (is_column_) {
      sizes.min_size = std::max(sizes.min_size, child_min_max_sizes.min_size);
      sizes.max_size = std::max(sizes.max_size, child_min_max_sizes.max_size);
    } else {
      sizes.max_size += child_min_max_sizes.max_size;
      if (IsMultiline())
        sizes.min_size = std::max(sizes.min_size, child_min_max_sizes.min_size);
      else
        sizes.min_size += child_min_max_sizes.min_size;
    }
  }
  sizes.max_size = std::max(sizes.max_size, sizes.min_size);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());

  if (input.size_type == NGMinMaxSizeType::kBorderBoxSize)
    sizes += border_scrollbar_padding_.InlineSum();

  return sizes;
}

bool NGFlexLayoutAlgorithm::IsMultiline() const {
  return Style().FlexWrap() != EFlexWrap::kNowrap;
}

}  // namespace blink
