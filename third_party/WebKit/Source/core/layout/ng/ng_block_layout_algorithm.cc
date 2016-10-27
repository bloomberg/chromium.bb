// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {
namespace {

LayoutUnit ComputeCollapsedMarginBlockStart(
    const NGMarginStrut& prev_margin_strut,
    const NGMarginStrut& curr_margin_strut) {
  return std::max(prev_margin_strut.margin_block_end,
                  curr_margin_strut.margin_block_start) -
         std::max(prev_margin_strut.negative_margin_block_end.abs(),
                  curr_margin_strut.negative_margin_block_start.abs());
}

// Creates an exclusion from the fragment that will be placed in the provided
// layout opportunity.
NGExclusion* CreateExclusion(const NGFragment& fragment,
                             const NGLayoutOpportunity& opportunity,
                             LayoutUnit float_offset,
                             NGBoxStrut margins) {
  LayoutUnit exclusion_top = opportunity.offset.block_offset;

  LayoutUnit exclusion_left = opportunity.offset.inline_offset;
  exclusion_left += float_offset;

  LayoutUnit exclusion_bottom = exclusion_top + fragment.BlockSize();
  LayoutUnit exclusion_right = exclusion_left + fragment.InlineSize();

  // Adjust to child's margin.
  exclusion_bottom += margins.BlockSum();
  exclusion_right += margins.InlineSum();

  return new NGExclusion(exclusion_top, exclusion_right, exclusion_bottom,
                         exclusion_left);
}

// Finds a layout opportunity for the fragment.
// It iterates over all layout opportunities in the constraint space and returns
// the first layout opportunity that is wider than the fragment or returns the
// last one which is always the widest.
//
// @param space Constraint space that is used to find layout opportunity for
//              the fragment.
// @param fragment Fragment that needs to be placed.
// @param margins Margins of the fragment.
// @return Layout opportunity for the fragment.
const NGLayoutOpportunity FindLayoutOpportunityForFragment(
    const Member<NGConstraintSpace>& space,
    const NGFragment& fragment,
    const NGBoxStrut& margins) {
  NGLayoutOpportunityIterator* opportunity_iter = space->LayoutOpportunities();
  NGLayoutOpportunity opportunity;
  NGLayoutOpportunity opportunity_candidate = opportunity_iter->Next();

  while (!opportunity_candidate.IsEmpty()) {
    opportunity = opportunity_candidate;
    // Checking opportunity's block size is not necessary as a float cannot be
    // positioned on top of another float inside of the same constraint space.
    auto fragment_inline_size = fragment.InlineSize() + margins.InlineSum();
    if (opportunity.size.inline_size > fragment_inline_size)
      break;

    opportunity_candidate = opportunity_iter->Next();
  }

  return opportunity;
}

// Calculates the logical offset for opportunity.
NGLogicalOffset CalculateLogicalOffsetForOpportunity(
    const NGLayoutOpportunity& opportunity,
    NGBoxStrut border_padding,
    LayoutUnit float_offset,
    NGBoxStrut margins) {
  // TODO(layout-ng): create children_constraint_space with an offset for the
  // border and padding.
  // Offset from parent's border/padding.
  LayoutUnit inline_offset = border_padding.inline_start;
  LayoutUnit block_offset = border_padding.block_start;

  // Adjust to child's margin.
  inline_offset += margins.inline_start;
  block_offset += margins.block_start;

  // Offset from the opportunity's block/inline start.
  inline_offset += opportunity.offset.inline_offset;
  block_offset += opportunity.offset.block_offset;

  inline_offset += float_offset;

  return NGLogicalOffset(inline_offset, block_offset);
}

// Whether an in-flow block-level child creates a new formatting context.
//
// This will *NOT* check the following cases:
//  - The child is out-of-flow, e.g. floating or abs-pos.
//  - The child is a inline-level, e.g. "display: inline-block".
//  - The child establishes a new formatting context, but should be a child of
//    another layout algorithm, e.g. "display: table-caption" or flex-item.
bool IsNewFormattingContextForInFlowBlockLevelChild(
    const NGConstraintSpace& space,
    const ComputedStyle& style) {
  // TODO(layout-dev): This doesn't capture a few cases which can't be computed
  // directly from style yet:
  //  - The child is a <fieldset>.
  //  - "column-span: all" is set on the child (requires knowledge that we are
  //    in a multi-col formatting context).
  //    (https://drafts.csswg.org/css-multicol-1/#valdef-column-span-all)

  if (style.specifiesColumns() || style.containsPaint() ||
      style.containsLayout())
    return true;

  if (!style.isOverflowVisible())
    return true;

  EDisplay display = style.display();
  if (display == EDisplay::Grid || display == EDisplay::Flex ||
      display == EDisplay::Box)
    return true;

  if (space.WritingMode() != FromPlatformWritingMode(style.getWritingMode()))
    return true;

  return false;
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBox* first_child)
    : style_(style),
      first_child_(first_child),
      state_(kStateInit),
      is_fragment_margin_strut_block_start_updated_(false) {
  DCHECK(style_);
}

bool NGBlockLayoutAlgorithm::Layout(const NGConstraintSpace* constraint_space,
                                    NGPhysicalFragment** out) {
  switch (state_) {
    case kStateInit: {
      border_and_padding_ =
          ComputeBorders(Style()) + ComputePadding(*constraint_space, Style());

      LayoutUnit inline_size =
          ComputeInlineSizeForFragment(*constraint_space, Style());
      LayoutUnit adjusted_inline_size =
          inline_size - border_and_padding_.InlineSum();
      // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
      // -1?
      LayoutUnit block_size = ComputeBlockSizeForFragment(
          *constraint_space, Style(), NGSizeIndefinite);
      LayoutUnit adjusted_block_size(block_size);
      // Our calculated block-axis size may be indefinite at this point.
      // If so, just leave the size as NGSizeIndefinite instead of subtracting
      // borders and padding.
      if (adjusted_block_size != NGSizeIndefinite)
        adjusted_block_size -= border_and_padding_.BlockSum();
      constraint_space_for_children_ = new NGConstraintSpace(
          FromPlatformWritingMode(Style().getWritingMode()),
          FromPlatformDirection(Style().direction()), *constraint_space,
          NGLogicalSize(adjusted_inline_size, adjusted_block_size));
      content_size_ = border_and_padding_.block_start;

      builder_ = new NGFragmentBuilder(NGPhysicalFragmentBase::FragmentBox);
      builder_->SetDirection(constraint_space->Direction());
      builder_->SetWritingMode(constraint_space->WritingMode());
      builder_->SetInlineSize(inline_size).SetBlockSize(block_size);
      current_child_ = first_child_;
      state_ = kStateChildLayout;
      return false;
    }
    case kStateChildLayout: {
      if (current_child_) {
        if (!LayoutCurrentChild(constraint_space))
          return false;
        current_child_ = current_child_->NextSibling();
        if (current_child_)
          return false;
      }
      state_ = kStateFinalize;
      return false;
    }
    case kStateFinalize: {
      content_size_ += border_and_padding_.block_end;

      // Recompute the block-axis size now that we know our content size.
      LayoutUnit block_size = ComputeBlockSizeForFragment(
          *constraint_space, Style(), content_size_);

      builder_->SetBlockSize(block_size)
          .SetInlineOverflow(max_inline_size_)
          .SetBlockOverflow(content_size_);
      *out = builder_->ToFragment();
      state_ = kStateInit;
      return true;
    }
  };
  NOTREACHED();
  *out = nullptr;
  return true;
}

bool NGBlockLayoutAlgorithm::LayoutCurrentChild(
    const NGConstraintSpace* constraint_space) {
  constraint_space_for_children_->SetIsNewFormattingContext(
      IsNewFormattingContextForInFlowBlockLevelChild(*constraint_space,
                                                     *current_child_->Style()));

  NGFragment* fragment;
  if (!current_child_->Layout(constraint_space_for_children_, &fragment))
    return false;

  NGBoxStrut child_margins =
      ComputeMargins(*constraint_space_for_children_, *current_child_->Style(),
                     constraint_space_for_children_->WritingMode(),
                     constraint_space_for_children_->Direction());

  NGLogicalOffset fragment_offset;
  if (current_child_->Style()->isFloating()) {
    fragment_offset = PositionFloatFragment(*fragment, child_margins);
  } else {
    // TODO(layout-ng): move ApplyAutoMargins to PositionFragment
    ApplyAutoMargins(*constraint_space_for_children_, *current_child_->Style(),
                     *fragment, &child_margins);
    fragment_offset =
        PositionFragment(*fragment, child_margins, *constraint_space);
  }
  builder_->AddChild(fragment, fragment_offset);
  return true;
}

NGBoxStrut NGBlockLayoutAlgorithm::CollapseMargins(
    const NGConstraintSpace& space,
    const NGBoxStrut& margins,
    const NGFragment& fragment) {
  bool is_zero_height_box = !fragment.BlockSize() && margins.IsEmpty() &&
                            fragment.MarginStrut().IsEmpty();
  // Create the current child's margin strut from its children's margin strut or
  // use margin strut from the the last non-empty child.
  NGMarginStrut curr_margin_strut =
      is_zero_height_box ? prev_child_margin_strut_ : fragment.MarginStrut();

  // Calculate borders and padding for the current child.
  NGBoxStrut border_and_padding =
      ComputeBorders(*current_child_->Style()) +
      ComputePadding(space, *current_child_->Style());

  // Collapse BLOCK-START margins if there is no padding or border between
  // parent (current child) and its first in-flow child.
  if (border_and_padding.block_start) {
    curr_margin_strut.SetMarginBlockStart(margins.block_start);
  } else {
    curr_margin_strut.AppendMarginBlockStart(margins.block_start);
  }

  // Collapse BLOCK-END margins if
  // 1) there is no padding or border between parent (current child) and its
  //    first/last in-flow child
  // 2) parent's logical height is auto.
  if (current_child_->Style()->logicalHeight().isAuto() &&
      !border_and_padding.block_end) {
    curr_margin_strut.AppendMarginBlockEnd(margins.block_end);
  } else {
    curr_margin_strut.SetMarginBlockEnd(margins.block_end);
  }

  NGBoxStrut result_margins;
  // Margins of the newly established formatting context do not participate
  // in Collapsing Margins:
  // - Compute margins block start for adjoining blocks *including* 1st block.
  // - Compute margins block end for the last block.
  // - Do not set the computed margins to the parent fragment.
  if (space.IsNewFormattingContext()) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
    bool is_last_child = !current_child_->NextSibling();
    if (is_last_child)
      result_margins.block_end = curr_margin_strut.BlockEndSum();
    return result_margins;
  }

  // Zero-height boxes are ignored and do not participate in margin collapsing.
  if (is_zero_height_box)
    return result_margins;

  // Compute the margin block start for adjoining blocks *excluding* 1st block
  if (is_fragment_margin_strut_block_start_updated_) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
  }

  // Update the parent fragment's margin strut
  UpdateMarginStrut(curr_margin_strut);

  prev_child_margin_strut_ = curr_margin_strut;
  return result_margins;
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionFragment(
    const NGFragment& fragment,
    const NGBoxStrut& child_margins,
    const NGConstraintSpace& space) {
  const NGBoxStrut collapsed_margins =
      CollapseMargins(space, child_margins, fragment);

  LayoutUnit inline_offset =
      border_and_padding_.inline_start + child_margins.inline_start;
  LayoutUnit block_offset = content_size_ + collapsed_margins.block_start;

  content_size_ += fragment.BlockSize() + collapsed_margins.BlockSum();
  max_inline_size_ = std::max(
      max_inline_size_, fragment.InlineSize() + child_margins.InlineSum() +
                            border_and_padding_.InlineSum());
  return NGLogicalOffset(inline_offset, block_offset);
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionFloatFragment(
    const NGFragment& fragment,
    const NGBoxStrut& margins) {
  // TODO(glebl@chromium.org): Support the top edge alignment rule.
  // Find a layout opportunity that will fit our float.
  const NGLayoutOpportunity opportunity = FindLayoutOpportunityForFragment(
      constraint_space_for_children_, fragment, margins);
  DCHECK(!opportunity.IsEmpty()) << "Opportunity is empty but it shouldn't be";

  // Calculate the float offset if needed.
  LayoutUnit float_offset;
  if (current_child_->Style()->floating() == EFloat::Right) {
    float_offset = opportunity.size.inline_size - fragment.InlineSize();
  }

  // Add the float as an exclusion.
  const NGExclusion* exclusion =
      CreateExclusion(fragment, opportunity, float_offset, margins);
  constraint_space_for_children_->AddExclusion(exclusion);

  return CalculateLogicalOffsetForOpportunity(opportunity, border_and_padding_,
                                              float_offset, margins);
}

void NGBlockLayoutAlgorithm::UpdateMarginStrut(const NGMarginStrut& from) {
  if (!is_fragment_margin_strut_block_start_updated_) {
    builder_->SetMarginStrutBlockStart(from);
    is_fragment_margin_strut_block_start_updated_ = true;
  }
  builder_->SetMarginStrutBlockEnd(from);
}

DEFINE_TRACE(NGBlockLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(builder_);
  visitor->trace(constraint_space_for_children_);
  visitor->trace(current_child_);
}

}  // namespace blink
