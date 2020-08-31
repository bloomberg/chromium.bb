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
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

NGFieldsetLayoutAlgorithm::NGFieldsetLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      writing_mode_(ConstraintSpace().GetWritingMode()),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding),
      consumed_block_size_(BreakToken() ? BreakToken()->ConsumedBlockSize()
                                        : LayoutUnit()) {
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);

  borders_ = container_builder_.Borders();
  padding_ = container_builder_.Padding();
  border_box_size_ = container_builder_.InitialBorderBoxSize();

  // Leading border and padding should only apply to the first fragment. We
  // don't adjust the value of border_padding_ itself so that it can be used
  // when calculating the block size of the last fragment.
  adjusted_border_padding_ = border_padding_;
  AdjustForFragmentation(BreakToken(), &adjusted_border_padding_);
}

scoped_refptr<const NGLayoutResult> NGFieldsetLayoutAlgorithm::Layout() {
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

  // Calculate the amount of the border block-start that was consumed in
  // previous fragments.
  consumed_border_block_start_ =
      std::min(consumed_block_size_, borders_.block_start);
  intrinsic_block_size_ = borders_.block_start - consumed_border_block_start_;

  NGBreakStatus break_status = LayoutChildren();
  if (break_status == NGBreakStatus::kNeedsEarlierBreak) {
    // We need to abort the layout. No fragment will be generated.
    return container_builder_.Abort(NGLayoutResult::kNeedsEarlierBreak);
  }

  intrinsic_block_size_ = ClampIntrinsicBlockSize(
      ConstraintSpace(), Node(), adjusted_border_padding_,
      intrinsic_block_size_ + borders_.block_end);

  // Recompute the block-axis size now that we know our content size.
  border_box_size_.block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), border_padding_,
                                  intrinsic_block_size_ + consumed_block_size_,
                                  border_box_size_.inline_size);

  // The above computation utility knows nothing about fieldset weirdness. The
  // legend may eat from the available content box block size. Make room for
  // that if necessary.
  // Note that in size containment, we have to consider sizing as if we have no
  // contents, with the conjecture being that legend is part of the contents.
  // Thus, only do this adjustment if we do not contain size.
  if (!Node().ShouldApplySizeContainment()) {
    // Similar to how we add the consumed block size to the intrinsic
    // block size when calculating border_box_size_.block_size, we also need to
    // do so when the fieldset is adjusted to encompass the legend.
    border_box_size_.block_size =
        std::max(border_box_size_.block_size,
                 minimum_border_box_block_size_ + consumed_block_size_);
  }

  // TODO(almaher): end border and padding may overflow the parent
  // fragmentainer, and we should avoid that.
  LayoutUnit block_size = border_box_size_.block_size - consumed_block_size_;

  container_builder_.SetIsFieldsetContainer();
  if (ConstraintSpace().HasKnownFragmentainerBlockSize()) {
    FinishFragmentation(
        ConstraintSpace(), BreakToken(), block_size, intrinsic_block_size_,
        FragmentainerSpaceAtBfcStart(ConstraintSpace()), &container_builder_);
  } else {
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
    container_builder_.SetBlockSize(block_size);
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), borders_,
                        &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

NGBreakStatus NGFieldsetLayoutAlgorithm::LayoutChildren() {
  scoped_refptr<const NGBlockBreakToken> legend_break_token;
  scoped_refptr<const NGBlockBreakToken> content_break_token;
  bool has_seen_all_children = false;
  if (const auto* token = BreakToken()) {
    const auto child_tokens = token->ChildBreakTokens();
    if (wtf_size_t break_token_count = child_tokens.size()) {
      DCHECK_LE(break_token_count, 2u);
      for (wtf_size_t break_token_idx = 0; break_token_idx < break_token_count;
           break_token_idx++) {
        scoped_refptr<const NGBlockBreakToken> child_token =
            To<NGBlockBreakToken>(child_tokens[break_token_idx]);
        if (child_token && child_token->InputNode().IsRenderedLegend()) {
          DCHECK_EQ(break_token_idx, 0u);
          legend_break_token = child_token;
        } else {
          content_break_token = child_token;
        }
      }
    }
    if (token->HasSeenAllChildren()) {
      container_builder_.SetHasSeenAllChildren();
      has_seen_all_children = true;
    }
  }

  NGBlockNode legend = Node().GetRenderedLegend();
  bool legend_needs_layout =
      legend && (legend_break_token || !IsResumingLayout(BreakToken()));

  if (legend_needs_layout) {
    NGBreakStatus break_status = LayoutLegend(legend, legend_break_token);
    if (break_status != NGBreakStatus::kContinue)
      return break_status;

    // The legend may eat from the available content box block size. Calculate
    // the minimum block size needed to encompass the legend.
    if (!Node().ShouldApplySizeContainment()) {
      minimum_border_box_block_size_ =
          intrinsic_block_size_ + padding_.BlockSum() + borders_.block_end;
    }
  }

  NGBoxStrut borders_with_legend = borders_;
  borders_with_legend.block_start = intrinsic_block_size_;
  LogicalSize adjusted_padding_box_size =
      ShrinkAvailableSize(border_box_size_, borders_with_legend);

  if (adjusted_padding_box_size.block_size != kIndefiniteSize) {
    // If intrinsic_block_size_ does not include the border block-start that was
    // consumed in previous fragments, exclude consumed_border_block_start_ from
    // adjusted_padding_box_size, as well.
    if (consumed_border_block_start_ > LayoutUnit())
      adjusted_padding_box_size.block_size -= consumed_border_block_start_;

    // If the legend has been laid out in previous fragments,
    // adjusted_padding_box_size will need to be adjusted further to account for
    // block size taken up by the legend.
    if (legend) {
      LayoutUnit content_consumed_block_size =
          content_break_token ? content_break_token->ConsumedBlockSize()
                              : LayoutUnit();

      // Calculate the amount of the border block-end that was consumed in
      // previous fragments.
      DCHECK_NE(border_box_size_.block_size, kIndefiniteSize);
      LayoutUnit consumed_border_block_end =
          std::max(consumed_block_size_ -
                       (border_box_size_.block_size - borders_.block_end),
                   LayoutUnit());

      LayoutUnit legend_block_size =
          consumed_block_size_ - content_consumed_block_size -
          consumed_border_block_start_ - consumed_border_block_end;
      DCHECK_GE(legend_block_size, LayoutUnit());

      adjusted_padding_box_size.block_size =
          std::max(padding_.BlockSum(),
                   adjusted_padding_box_size.block_size - legend_block_size);
    }
  }

  // Proceed with normal fieldset children (excluding the rendered legend). They
  // all live inside an anonymous child box of the fieldset container.
  auto fieldset_content = Node().GetFieldsetContent();
  if (fieldset_content && (content_break_token || !has_seen_all_children)) {
    if (ConstraintSpace().HasBlockFragmentation() && legend_broke_ &&
        IsFragmentainerOutOfSpace(ConstraintSpace().FragmentainerOffsetAtBfc() +
                                  intrinsic_block_size_))
      return NGBreakStatus::kContinue;

    NGBreakStatus break_status =
        LayoutFieldsetContent(fieldset_content, content_break_token,
                              adjusted_padding_box_size, !!legend);
    if (break_status == NGBreakStatus::kNeedsEarlierBreak)
      return break_status;
  }

  if (!fieldset_content) {
    container_builder_.SetHasSeenAllChildren();
    // There was no anonymous child to provide the padding, so we have to add it
    // ourselves.
    intrinsic_block_size_ += padding_.BlockSum();
  }

  return NGBreakStatus::kContinue;
}

NGBreakStatus NGFieldsetLayoutAlgorithm::LayoutLegend(
    NGBlockNode& legend,
    scoped_refptr<const NGBlockBreakToken> legend_break_token) {
  // Lay out the legend. While the fieldset container normally ignores its
  // padding, the legend is laid out within what would have been the content
  // box had the fieldset been a regular block with no weirdness.
  LogicalSize content_box_size =
      ShrinkAvailableSize(border_box_size_, adjusted_border_padding_);
  LogicalSize percentage_size =
      CalculateChildPercentageSize(ConstraintSpace(), Node(), content_box_size);
  NGBoxStrut legend_margins = ComputeMarginsFor(
      legend.Style(), percentage_size.inline_size,
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction());

  if (legend_break_token)
    legend_margins.block_start = LayoutUnit();

  LogicalOffset legend_offset;
  scoped_refptr<const NGLayoutResult> result;
  scoped_refptr<const NGLayoutResult> previous_result;
  LayoutUnit block_offset = legend_margins.block_start;
  do {
    auto legend_space = CreateConstraintSpaceForLegend(
        legend, content_box_size, percentage_size, block_offset);
    result = legend.Layout(legend_space, legend_break_token.get());

    // TODO(layout-dev): Handle abortions caused by block fragmentation.
    DCHECK_EQ(result->Status(), NGLayoutResult::kSuccess);

    if (ConstraintSpace().HasBlockFragmentation()) {
      NGBreakStatus break_status = BreakBeforeChildIfNeeded(
          ConstraintSpace(), legend, *result.get(),
          ConstraintSpace().FragmentainerOffsetAtBfc() + block_offset,
          /*has_container_separation*/ false, &container_builder_);
      if (break_status != NGBreakStatus::kContinue)
        return break_status;
      EBreakBetween break_after = JoinFragmentainerBreakValues(
          result->FinalBreakAfter(), legend.Style().BreakAfter());
      container_builder_.SetPreviousBreakAfter(break_after);
    }

    const auto& physical_fragment = result->PhysicalFragment();
    legend_broke_ = physical_fragment.BreakToken();

    // We have already adjusted the legend block offset, no need to adjust
    // again.
    if (block_offset != legend_margins.block_start) {
      // If adjusting the block_offset caused the legend to break, revert back
      // to the previous result.
      if (legend_broke_) {
        result = std::move(previous_result);
        block_offset = legend_margins.block_start;
      }
      break;
    }

    LayoutUnit legend_margin_box_block_size =
        NGFragment(writing_mode_, physical_fragment).BlockSize() +
        legend_margins.BlockSum();
    LayoutUnit space_left = borders_.block_start - legend_margin_box_block_size;

    if (space_left > LayoutUnit()) {
      // Don't adjust the block-start offset of the legend if the legend broke.
      if (legend_break_token || legend_broke_)
        break;

      // If the border is the larger one, though, it will stay put at the
      // border-box block-start edge of the fieldset. Then it's the legend
      // that needs to be pushed. We'll center the margin box in this case, to
      // make sure that both margins remain within the area occupied by the
      // border also after adjustment.
      block_offset += space_left / 2;
      if (ConstraintSpace().HasBlockFragmentation()) {
        // Save the previous result in case adjusting the block_offset causes
        // the legend to break.
        previous_result = std::move(result);
        continue;
      }
    } else {
      // If the border is smaller, intrinsic_block_size_ should now be based on
      // the size of the legend instead of the border.
      intrinsic_block_size_ = legend_margin_box_block_size;

      // Don't adjust the block-start offset of the fragment border if it broke.
      if (BreakToken() || (ConstraintSpace().HasKnownFragmentainerBlockSize() &&
                           legend_margin_box_block_size >
                               ConstraintSpace().FragmentainerBlockSize()))
        break;
      // If the legend is larger than the width of the fieldset block-start
      // border, the actual padding edge of the fieldset will be moved
      // accordingly. This will be the block-start offset for the fieldset
      // contents anonymous box.
      borders_.block_start = legend_margin_box_block_size;
    }
    break;
  } while (true);

  // If the margin box of the legend is at least as tall as the fieldset
  // block-start border width, it will start at the block-start border edge
  // of the fieldset. As a paint effect, the block-start border will be
  // pushed so that the center of the border will be flush with the center
  // of the border-box of the legend.
  // TODO(mstensho): inline alignment
  legend_offset = LogicalOffset(
      adjusted_border_padding_.inline_start + legend_margins.inline_start,
      block_offset);

  container_builder_.AddResult(*result, legend_offset);
  return NGBreakStatus::kContinue;
}

NGBreakStatus NGFieldsetLayoutAlgorithm::LayoutFieldsetContent(
    NGBlockNode& fieldset_content,
    scoped_refptr<const NGBlockBreakToken> content_break_token,
    LogicalSize adjusted_padding_box_size,
    bool has_legend) {
  auto child_space = CreateConstraintSpaceForFieldsetContent(
      fieldset_content, adjusted_padding_box_size, intrinsic_block_size_);
  auto result = fieldset_content.Layout(child_space, content_break_token.get());

  // TODO(layout-dev): Handle abortions caused by block fragmentation.
  DCHECK_EQ(result->Status(), NGLayoutResult::kSuccess);

  NGBreakStatus break_status = NGBreakStatus::kContinue;
  if (ConstraintSpace().HasBlockFragmentation()) {
    // TODO(almaher): The legend should be treated as out-of-flow.
    break_status = BreakBeforeChildIfNeeded(
        ConstraintSpace(), fieldset_content, *result.get(),
        ConstraintSpace().FragmentainerOffsetAtBfc() + intrinsic_block_size_,
        /*has_container_separation*/ has_legend, &container_builder_);
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        result->FinalBreakAfter(), fieldset_content.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);
  }

  if (break_status == NGBreakStatus::kContinue) {
    LogicalOffset offset(borders_.inline_start, intrinsic_block_size_);
    container_builder_.AddResult(*result, offset);
    intrinsic_block_size_ +=
        NGFragment(writing_mode_, result->PhysicalFragment()).BlockSize();
    container_builder_.SetHasSeenAllChildren();
  }

  return break_status;
}

bool NGFieldsetLayoutAlgorithm::IsFragmentainerOutOfSpace(
    LayoutUnit block_offset) const {
  if (!ConstraintSpace().HasKnownFragmentainerBlockSize())
    return false;
  return block_offset >= FragmentainerSpaceAtBfcStart(ConstraintSpace());
}

MinMaxSizesResult NGFieldsetLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  MinMaxSizesResult result;

  // TODO(crbug.com/1011842): Need to consider content-size here.
  bool apply_size_containment = Node().ShouldApplySizeContainment();

  // Size containment does not consider the legend for sizing.
  if (!apply_size_containment) {
    if (NGBlockNode legend = Node().GetRenderedLegend()) {
      result = ComputeMinAndMaxContentContribution(Style(), legend, input);
      result.sizes += ComputeMinMaxMargins(Style(), legend).InlineSum();
    }
  }

  // The fieldset content includes the fieldset padding (and any scrollbars),
  // while the legend is a regular child and doesn't. We may have a fieldset
  // without any content or legend, so add the padding here, on the outside.
  result.sizes += ComputePadding(ConstraintSpace(), Style()).InlineSum();

  // Size containment does not consider the content for sizing.
  if (!apply_size_containment) {
    if (NGBlockNode content = Node().GetFieldsetContent()) {
      MinMaxSizesResult content_result =
          ComputeMinAndMaxContentContribution(Style(), content, input);
      content_result.sizes +=
          ComputeMinMaxMargins(Style(), content).InlineSum();
      result.sizes.Encompass(content_result.sizes);
      result.depends_on_percentage_block_size |=
          content_result.depends_on_percentage_block_size;
    }
  }

  result.sizes += ComputeBorders(ConstraintSpace(), Style()).InlineSum();
  return result;
}

const NGConstraintSpace
NGFieldsetLayoutAlgorithm::CreateConstraintSpaceForLegend(
    NGBlockNode legend,
    LogicalSize available_size,
    LogicalSize percentage_size,
    LayoutUnit block_offset) {
  NGConstraintSpaceBuilder builder(
      ConstraintSpace(), legend.Style().GetWritingMode(), /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), legend, &builder);

  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetIsShrinkToFit(legend.Style().LogicalWidth().IsAuto());
  builder.SetTextDirection(legend.Style().Direction());

  if (ConstraintSpace().HasBlockFragmentation()) {
    SetupSpaceBuilderForFragmentation(ConstraintSpace(), legend, block_offset,
                                      &builder, /* is_new_fc */ true);
    builder.SetEarlyBreakAppeal(container_builder_.BreakAppeal());
  }
  return builder.ToConstraintSpace();
}

const NGConstraintSpace
NGFieldsetLayoutAlgorithm::CreateConstraintSpaceForFieldsetContent(
    NGBlockNode fieldset_content,
    LogicalSize padding_box_size,
    LayoutUnit block_offset) {
  DCHECK(fieldset_content.CreatesNewFormattingContext());
  NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                   ConstraintSpace().GetWritingMode(),
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(padding_box_size);
  builder.SetPercentageResolutionSize(
      ConstraintSpace().PercentageResolutionSize());
  builder.SetIsFixedBlockSize(padding_box_size.block_size != kIndefiniteSize);

  if (ConstraintSpace().HasBlockFragmentation()) {
    SetupSpaceBuilderForFragmentation(ConstraintSpace(), fieldset_content,
                                      block_offset, &builder,
                                      /* is_new_fc */ true);
    builder.SetEarlyBreakAppeal(container_builder_.BreakAppeal());
  }
  return builder.ToConstraintSpace();
}

}  // namespace blink
