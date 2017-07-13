// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_child_iterator.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "platform/wtf/Optional.h"

namespace blink {
namespace {

// Returns if a child may be affected by its clear property. I.e. it will
// actually clear a float.
bool ClearanceMayAffectLayout(
    const NGConstraintSpace& space,
    const Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
    const ComputedStyle& child_style) {
  const NGExclusions& exclusions = *space.Exclusions();
  EClear clear = child_style.Clear();
  bool should_clear_left = (clear == EClear::kBoth || clear == EClear::kLeft);
  bool should_clear_right = (clear == EClear::kBoth || clear == EClear::kRight);

  if (exclusions.last_left_float && should_clear_left)
    return true;

  if (exclusions.last_right_float && should_clear_right)
    return true;

  auto should_clear_pred =
      [&](const RefPtr<const NGUnpositionedFloat>& unpositioned_float) {
        return (unpositioned_float->IsLeft() && should_clear_left) ||
               (unpositioned_float->IsRight() && should_clear_right);
      };

  if (std::any_of(unpositioned_floats.begin(), unpositioned_floats.end(),
                  should_clear_pred))
    return true;

  return false;
}

// Whether we've run out of space in this flow. If so, there will be no work
// left to do for this block in this fragmentainer.
bool IsOutOfSpace(const NGConstraintSpace& space, LayoutUnit content_size) {
  return space.HasBlockFragmentation() &&
         content_size >= space.FragmentainerSpaceAvailable();
}

bool IsEmptyFragment(NGWritingMode writing_mode,
                     const NGLayoutResult& layout_result) {
  if (!layout_result.PhysicalFragment())
    return true;

  // TODO(ikilpatrick): This is kinda wrong, we need a bit on the
  // NGLayoutResult to properly determine if something is empty.
  NGFragment fragment(writing_mode, layout_result.PhysicalFragment().Get());
  if (!fragment.BlockSize())
    return true;

  return false;
}

}  // namespace

bool MaybeUpdateFragmentBfcOffset(const NGConstraintSpace& space,
                                  LayoutUnit bfc_block_offset,
                                  NGFragmentBuilder* builder) {
  DCHECK(builder);
  if (!builder->BfcOffset()) {
    NGLogicalOffset bfc_offset = {space.BfcOffset().inline_offset,
                                  bfc_block_offset};
    AdjustToClearance(space.ClearanceOffset(), &bfc_offset);
    builder->SetBfcOffset(bfc_offset);
    return true;
  }

  return false;
}

void PositionPendingFloats(
    LayoutUnit origin_block_offset,
    NGFragmentBuilder* container_builder,
    Vector<RefPtr<NGUnpositionedFloat>>* unpositioned_floats,
    NGConstraintSpace* space) {
  DCHECK(container_builder->BfcOffset() || space->FloatsBfcOffset())
      << "Parent BFC offset should be known here";
  LayoutUnit from_block_offset =
      container_builder->BfcOffset()
          ? container_builder->BfcOffset().value().block_offset
          : space->FloatsBfcOffset().value().block_offset;

  const auto positioned_floats = PositionFloats(
      origin_block_offset, from_block_offset, *unpositioned_floats, space);

  // TODO(ikilpatrick): Add DCHECK that any positioned floats are children.

  for (const auto& positioned_float : positioned_floats)
    container_builder->AddPositionedFloat(positioned_float);

  unpositioned_floats->clear();
}

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(NGBlockNode node,
                                               NGConstraintSpace* space,
                                               NGBlockBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, break_token) {}

Optional<MinMaxContentSize> NGBlockLayoutAlgorithm::ComputeMinMaxContentSize()
    const {
  MinMaxContentSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().ContainsSize())
    return sizes;

  // TODO: handle floats & orthogonal children.
  for (NGLayoutInputNode node = Node().FirstChild(); node;
       node = node.NextSibling()) {
    if (node.IsOutOfFlowPositioned())
      continue;
    MinMaxContentSize child_sizes;
    if (node.IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |node| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes = node.ComputeMinMaxContentSize();
    } else {
      Optional<MinMaxContentSize> child_minmax;
      if (NeedMinMaxContentSizeForContentContribution(node.Style())) {
        child_minmax = node.ComputeMinMaxContentSize();
      }

      child_sizes =
          ComputeMinAndMaxContentContribution(node.Style(), child_minmax);
    }

    sizes.min_content = std::max(sizes.min_content, child_sizes.min_content);
    sizes.max_content = std::max(sizes.max_content, child_sizes.max_content);
  }

  sizes.max_content = std::max(sizes.min_content, sizes.max_content);
  return sizes;
}

NGLogicalOffset NGBlockLayoutAlgorithm::CalculateLogicalOffset(
    const NGBoxStrut& child_margins,
    const WTF::Optional<NGLogicalOffset>& known_fragment_offset) {
  if (known_fragment_offset)
    return known_fragment_offset.value() - ContainerBfcOffset();

  LayoutUnit inline_offset =
      border_scrollbar_padding_.inline_start + child_margins.inline_start;

  // If we've reached here, both the child and the current layout don't have a
  // BFC offset yet. Children in this situation are always placed at a logical
  // block offset of 0.
  DCHECK(!container_builder_.BfcOffset());
  return {inline_offset, LayoutUnit()};
}

RefPtr<NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  WTF::Optional<MinMaxContentSize> min_max_size;
  if (NeedMinMaxContentSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxContentSize();

  border_scrollbar_padding_ = ComputeBorders(ConstraintSpace(), Style()) +
                              ComputePadding(ConstraintSpace(), Style()) +
                              GetScrollbarSizes(Node().GetLayoutObject());
  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of -1?
  NGLogicalSize size(
      ComputeInlineSizeForFragment(ConstraintSpace(), Style(), min_max_size),
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(),
                                  NGSizeIndefinite));

  // Our calculated block-axis size may be indefinite at this point.
  // If so, just leave the size as NGSizeIndefinite instead of subtracting
  // borders and padding.
  NGLogicalSize adjusted_size(size);
  if (size.block_size == NGSizeIndefinite) {
    adjusted_size.inline_size -= border_scrollbar_padding_.InlineSum();
  } else {
    adjusted_size -= border_scrollbar_padding_;
    adjusted_size.block_size = std::max(adjusted_size.block_size, LayoutUnit());
  }
  adjusted_size.inline_size = std::max(adjusted_size.inline_size, LayoutUnit());

  child_available_size_ = adjusted_size;
  child_percentage_size_ = adjusted_size;

  container_builder_.SetDirection(constraint_space_->Direction());
  container_builder_.SetWritingMode(constraint_space_->WritingMode());
  container_builder_.SetSize(size);

  // If we have a list of unpositioned floats as input to this layout, we'll
  // need to abort once our BFC offset is resolved. Additionally the
  // FloatsBfcOffset() must not be present in this case.
  unpositioned_floats_ = constraint_space_->UnpositionedFloats();
  abort_when_bfc_resolved_ = !unpositioned_floats_.IsEmpty();
  if (abort_when_bfc_resolved_)
    DCHECK(!constraint_space_->FloatsBfcOffset());

  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken());
  NGBlockChildIterator::Entry entry = child_iterator.NextChild();
  NGLayoutInputNode child = entry.node;
  NGBreakToken* child_break_token = entry.token;

  // If we are resuming from a break token our start border and padding is
  // within a previous fragment.
  content_size_ =
      BreakToken() ? LayoutUnit() : border_scrollbar_padding_.block_start;

  NGMarginStrut input_margin_strut = ConstraintSpace().MarginStrut();
  LayoutUnit input_bfc_block_offset =
      ConstraintSpace().BfcOffset().block_offset;

  // Margins collapsing:
  //   Do not collapse margins between parent and its child if there is
  //   border/padding between them.
  if (border_scrollbar_padding_.block_start) {
    input_bfc_block_offset += input_margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(
        ConstraintSpace(), input_bfc_block_offset, &container_builder_);

    if (updated && abort_when_bfc_resolved_) {
      container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
      return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
    }

    // We reset the block offset here as it may have been effected by clearance.
    input_bfc_block_offset = ContainerBfcOffset().block_offset;
    input_margin_strut = NGMarginStrut();
  }

  // If a new formatting context hits the margin collapsing if-branch above
  // then the BFC offset is still {} as the margin strut from the constraint
  // space must also be empty.
  // If we are resuming layout from a break token the same rule applies. Margin
  // struts cannot pass through break tokens.
  if (ConstraintSpace().IsNewFormattingContext() || BreakToken()) {
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), input_bfc_block_offset,
                                 &container_builder_);
    DCHECK_EQ(input_margin_strut, NGMarginStrut());
    DCHECK_EQ(container_builder_.BfcOffset().value(), NGLogicalOffset());
  }

  input_bfc_block_offset += content_size_;

  WTF::Optional<NGPreviousInflowPosition> previous_inflow_position =
      NGPreviousInflowPosition{input_bfc_block_offset, content_size_,
                               input_margin_strut};

  while (child) {
    if (child.IsOutOfFlowPositioned()) {
      DCHECK(!child_break_token);
      HandleOutOfFlowPositioned(previous_inflow_position.value(),
                                ToNGBlockNode(child));
    } else if (child.IsFloating()) {
      HandleFloat(previous_inflow_position.value(), ToNGBlockNode(child),
                  ToNGBlockBreakToken(child_break_token));
    } else {
      previous_inflow_position = HandleInflow(previous_inflow_position.value(),
                                              child, child_break_token);

      // If we didn't receive a NGPreviousInflowPosition from HandleInflow, we
      // need to abort the layout as our BFC offset was resolved.
      if (!previous_inflow_position) {
        container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
        return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
      }
    }

    entry = child_iterator.NextChild();
    child = entry.node;
    child_break_token = entry.token;

    if (IsOutOfSpace(ConstraintSpace(), content_size_))
      break;
  }

  NGMarginStrut end_margin_strut = previous_inflow_position->margin_strut;
  LayoutUnit end_bfc_block_offset = previous_inflow_position->bfc_block_offset;

  // Margins collapsing:
  //   Bottom margins of an in-flow block box doesn't collapse with its last
  //   in-flow block-level child's bottom margin if the box has bottom
  //   border/padding.
  if (border_scrollbar_padding_.block_end ||
      ConstraintSpace().IsNewFormattingContext()) {
    content_size_ =
        std::max(content_size_, previous_inflow_position->logical_block_offset +
                                    end_margin_strut.Sum());
    end_margin_strut = NGMarginStrut();
  }

  // If the current layout is a new formatting context, we need to encapsulate
  // all of our floats.
  if (ConstraintSpace().IsNewFormattingContext()) {
    // We can use the BFC coordinates, as we are a new formatting context.
    DCHECK_EQ(container_builder_.BfcOffset().value(), NGLogicalOffset());

    WTF::Optional<LayoutUnit> float_end_offset =
        GetClearanceOffset(ConstraintSpace().Exclusions(), EClear::kBoth);
    if (float_end_offset)
      content_size_ = std::max(content_size_, float_end_offset.value());
  }

  content_size_ += border_scrollbar_padding_.block_end;

  // Recompute the block-axis size now that we know our content size.
  size.block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), content_size_);
  container_builder_.SetBlockSize(size.block_size);

  // Non-empty blocks always know their position in space.
  // TODO(ikilpatrick): This check for a break token seems error prone.
  if (size.block_size || BreakToken()) {
    end_bfc_block_offset += end_margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(
        ConstraintSpace(), end_bfc_block_offset, &container_builder_);

    if (updated && abort_when_bfc_resolved_) {
      container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
      return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
    }

    PositionPendingFloats(end_bfc_block_offset, &container_builder_,
                          &unpositioned_floats_, MutableConstraintSpace());
  }

  // Margins collapsing:
  //   Do not collapse margins between the last in-flow child and bottom margin
  //   of its parent if the parent has height != auto()
  if (!Style().LogicalHeight().IsAuto()) {
    // TODO(glebl): handle minLogicalHeight, maxLogicalHeight.
    end_margin_strut = NGMarginStrut();
  }
  container_builder_.SetEndMarginStrut(end_margin_strut);
  container_builder_.SetOverflowSize(
      NGLogicalSize(max_inline_size_, content_size_));

  // We only finalize for fragmentation if the fragment has a BFC offset. This
  // may occur with a zero block size fragment. We need to know the BFC offset
  // to determine where the fragmentation line is relative to us.
  if (container_builder_.BfcOffset() &&
      ConstraintSpace().HasBlockFragmentation())
    FinalizeForFragmentation();

  // Only layout absolute and fixed children if we aren't going to revisit this
  // layout.
  if (unpositioned_floats_.IsEmpty()) {
    NGOutOfFlowLayoutPart(ConstraintSpace(), Style(), &container_builder_)
        .Run();
  }

  // If we have any unpositioned floats at this stage, need to tell our parent
  // about this, so that we get relayout with a forced BFC offset.
  if (!unpositioned_floats_.IsEmpty()) {
    DCHECK(!container_builder_.BfcOffset());
    container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
  }

  PropagateBaselinesFromChildren();

  return container_builder_.ToBoxFragment();
}

void NGBlockLayoutAlgorithm::HandleOutOfFlowPositioned(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGBlockNode child) {
  // TODO(ikilpatrick): Determine which of the child's margins need to be
  // included for the static position.
  NGLogicalOffset offset = {border_scrollbar_padding_.inline_start,
                            previous_inflow_position.logical_block_offset};

  // We only include the margin strut in the OOF static-position if we know we
  // aren't going to be a zero-block-size fragment.
  if (container_builder_.BfcOffset())
    offset.block_offset += previous_inflow_position.margin_strut.Sum();

  container_builder_.AddOutOfFlowChildCandidate(child, offset);
}

void NGBlockLayoutAlgorithm::HandleFloat(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGBlockNode child,
    NGBlockBreakToken* token) {
  // Calculate margins in the BFC's writing mode.
  NGBoxStrut margins = CalculateMargins(child);

  LayoutUnit origin_inline_offset =
      constraint_space_->BfcOffset().inline_offset +
      border_scrollbar_padding_.inline_start;

  RefPtr<NGUnpositionedFloat> unpositioned_float = NGUnpositionedFloat::Create(
      child_available_size_, child_percentage_size_, origin_inline_offset,
      constraint_space_->BfcOffset().inline_offset, margins, child, token);
  unpositioned_floats_.push_back(std::move(unpositioned_float));

  // If there is a break token for a float we must be resuming layout, we must
  // always know our position in the BFC.
  DCHECK(!token || container_builder_.BfcOffset());

  // No need to postpone the positioning if we know the correct offset.
  if (container_builder_.BfcOffset() || ConstraintSpace().FloatsBfcOffset()) {
    // Adjust origin point to the margins of the last child.
    // Example: <div style="margin-bottom: 20px"><float></div>
    //          <div style="margin-bottom: 30px"></div>
    LayoutUnit origin_block_offset =
        container_builder_.BfcOffset()
            ? previous_inflow_position.bfc_block_offset +
                  previous_inflow_position.margin_strut.Sum()
            : ConstraintSpace().FloatsBfcOffset().value().block_offset;
    PositionPendingFloats(origin_block_offset, &container_builder_,
                          &unpositioned_floats_, MutableConstraintSpace());
  }
}

WTF::Optional<NGPreviousInflowPosition> NGBlockLayoutAlgorithm::HandleInflow(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGLayoutInputNode child,
    NGBreakToken* child_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());

  bool should_position_pending_floats =
      !child.CreatesNewFormattingContext() &&
      ClearanceMayAffectLayout(ConstraintSpace(), unpositioned_floats_,
                               child.Style());

  // Children which may clear a float need to force all the pending floats to
  // be positioned before layout. This also resolves our BFC offset.
  if (should_position_pending_floats) {
    LayoutUnit origin_point_block_offset =
        previous_inflow_position.bfc_block_offset +
        previous_inflow_position.margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(
        ConstraintSpace(), origin_point_block_offset, &container_builder_);

    if (updated && abort_when_bfc_resolved_)
      return WTF::nullopt;

    // TODO(ikilpatrick): Check if origin_point_block_offset is correct -
    // MaybeUpdateFragmentBfcOffset might have changed it due to clearance.
    PositionPendingFloats(origin_point_block_offset, &container_builder_,
                          &unpositioned_floats_, MutableConstraintSpace());
  }

  // Perform layout on the child.
  NGInflowChildData child_data =
      ComputeChildData(previous_inflow_position, child);
  RefPtr<NGConstraintSpace> child_space =
      CreateConstraintSpaceForChild(child, child_data);
  RefPtr<NGLayoutResult> layout_result =
      child.Layout(child_space.Get(), child_break_token);

  // If we don't know our BFC offset yet, we need to copy the list of
  // unpositioned floats from the child's layout result.
  //
  // If the child had any unpositioned floats, we need to abort our layout if
  // we resolve our BFC offset.
  //
  // If we are a new formatting context, the child will get re-laid out once it
  // has been positioned.
  //
  // TODO(ikilpatrick): a more optimal version of this is to set
  // abort_when_bfc_resolved_, if the child tree _added_ any floats.
  if (!container_builder_.BfcOffset() && !child.CreatesNewFormattingContext()) {
    unpositioned_floats_ = layout_result->UnpositionedFloats();
    abort_when_bfc_resolved_ |= !layout_result->UnpositionedFloats().IsEmpty();
    if (child_space->FloatsBfcOffset())
      DCHECK(layout_result->UnpositionedFloats().IsEmpty());
  }

  // A child may have aborted its layout if it resolved its BFC offset. If
  // we don't have a BFC offset yet, we need to propagate the abortion up
  // to our parent.
  if (layout_result->Status() == NGLayoutResult::kBfcOffsetResolved &&
      !container_builder_.BfcOffset()) {
    DCHECK(!child.CreatesNewFormattingContext());

    MaybeUpdateFragmentBfcOffset(
        ConstraintSpace(), layout_result->BfcOffset().value().block_offset,
        &container_builder_);

    // NOTE: Unlike other aborts, we don't try check if we *should* abort with
    // abort_when_bfc_resolved_, this is simply propagating an abort up to a
    // node which is able to restart the layout (a node that has resolved its
    // BFC offset).
    return WTF::nullopt;
  }

  // We try and position the child within the block formatting context. This
  // may cause our BFC offset to be resolved, in which case we should abort our
  // layout if needed.
  WTF::Optional<NGLogicalOffset> child_bfc_offset;
  if (child.CreatesNewFormattingContext()) {
    if (!PositionNewFc(child, previous_inflow_position, *layout_result,
                       child_data, *child_space, &child_bfc_offset))
      return WTF::nullopt;
  } else if (layout_result->BfcOffset()) {
    if (!PositionWithBfcOffset(layout_result->BfcOffset().value(),
                               &child_bfc_offset))
      return WTF::nullopt;
  } else if (container_builder_.BfcOffset()) {
    child_bfc_offset =
        PositionWithParentBfc(*child_space, child_data, *layout_result);
  } else
    DCHECK(IsEmptyFragment(ConstraintSpace().WritingMode(), *layout_result));

  // We need to layout a child if we know its BFC offset and:
  //  - It aborted its layout as it resolved its BFC offset.
  //  - It has some unpositioned floats.
  if ((layout_result->Status() == NGLayoutResult::kBfcOffsetResolved ||
       !layout_result->UnpositionedFloats().IsEmpty()) &&
      child_bfc_offset) {
    RefPtr<NGConstraintSpace> new_child_space =
        CreateConstraintSpaceForChild(child, child_data, child_bfc_offset);
    layout_result = child.Layout(new_child_space.Get(), child_break_token);

    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);
  }

  // We must have an actual fragment at this stage.
  DCHECK(layout_result->PhysicalFragment().Get());

  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get()));

  NGLogicalOffset logical_offset =
      CalculateLogicalOffset(child_data.margins, child_bfc_offset);

  // Only modify content_size_ if the fragment's BlockSize is not empty. This is
  // needed to prevent the situation when logical_offset is included in
  // content_size_ for empty blocks. Example:
  //   <div style="overflow:hidden">
  //     <div style="margin-top: 8px"></div>
  //     <div style="margin-top: 10px"></div>
  //   </div>
  if (fragment.BlockSize()) {
    content_size_ = std::max(
        content_size_, logical_offset.block_offset + fragment.BlockSize());
  }
  max_inline_size_ = std::max(
      max_inline_size_, fragment.InlineSize() + child_data.margins.InlineSum() +
                            border_scrollbar_padding_.InlineSum());

  container_builder_.AddChild(layout_result, logical_offset);

  return ComputeInflowPosition(previous_inflow_position, child_data,
                               child_bfc_offset, logical_offset, *layout_result,
                               fragment);
}

NGInflowChildData NGBlockLayoutAlgorithm::ComputeChildData(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGLayoutInputNode child) {
  DCHECK(child);
  DCHECK(!child.IsFloating());

  // Calculate margins in parent's writing mode.
  NGBoxStrut margins = CalculateMargins(child);

  // Append the current margin strut with child's block start margin.
  // Non empty border/padding, and new FC use cases are handled inside of the
  // child's layout
  NGMarginStrut margin_strut = previous_inflow_position.margin_strut;
  margin_strut.Append(margins.block_start);

  NGLogicalOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().inline_offset +
          border_scrollbar_padding_.inline_start + margins.inline_start,
      previous_inflow_position.bfc_block_offset};

  return {child_bfc_offset, margin_strut, margins};
}

NGPreviousInflowPosition NGBlockLayoutAlgorithm::ComputeInflowPosition(
    const NGPreviousInflowPosition& previous_inflow_position,
    const NGInflowChildData& child_data,
    const WTF::Optional<NGLogicalOffset>& child_bfc_offset,
    const NGLogicalOffset& logical_offset,
    const NGLayoutResult& layout_result,
    const NGFragment& fragment) {
  // Determine the child's end BFC block offset and logical offset, for the
  // next child to use.
  LayoutUnit child_end_bfc_block_offset;
  LayoutUnit logical_block_offset;

  if (child_bfc_offset) {
    // TODO(crbug.com/716930): I think the layout_result->BfcOffset() condition
    // here can be removed once we've removed inline splitting.
    if (fragment.BlockSize() || layout_result.BfcOffset()) {
      child_end_bfc_block_offset =
          child_bfc_offset.value().block_offset + fragment.BlockSize();
      logical_block_offset = logical_offset.block_offset + fragment.BlockSize();
    } else {
      DCHECK(IsEmptyFragment(ConstraintSpace().WritingMode(), layout_result));

      child_end_bfc_block_offset = previous_inflow_position.bfc_block_offset;
      logical_block_offset = previous_inflow_position.logical_block_offset;
    }
  } else {
    child_end_bfc_block_offset = ConstraintSpace().BfcOffset().block_offset;
    logical_block_offset = LayoutUnit();
  }

  NGMarginStrut margin_strut = layout_result.EndMarginStrut();
  margin_strut.Append(child_data.margins.block_end);

  return {child_end_bfc_block_offset, logical_block_offset, margin_strut};
}

bool NGBlockLayoutAlgorithm::PositionNewFc(
    const NGLayoutInputNode& child,
    const NGPreviousInflowPosition& previous_inflow_position,
    const NGLayoutResult& layout_result,
    const NGInflowChildData& child_data,
    const NGConstraintSpace& child_space,
    WTF::Optional<NGLogicalOffset>* child_bfc_offset) {
  const ComputedStyle& child_style = child.Style();

  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(layout_result.PhysicalFragment().Get()));

  LayoutUnit child_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset;

  // 1. Position all pending floats to a temporary space.
  RefPtr<NGConstraintSpace> tmp_space =
      NGConstraintSpaceBuilder(&child_space)
          .SetIsNewFormattingContext(false)
          .ToConstraintSpace(child_space.WritingMode());
  PositionFloats(child_bfc_offset_estimate, child_bfc_offset_estimate,
                 unpositioned_floats_, tmp_space.Get());

  NGLogicalOffset origin_offset = {ConstraintSpace().BfcOffset().inline_offset +
                                       border_scrollbar_padding_.inline_start,
                                   child_bfc_offset_estimate};
  AdjustToClearance(
      GetClearanceOffset(ConstraintSpace().Exclusions(), child_style.Clear()),
      &origin_offset);

  // 2. Find an estimated layout opportunity for our fragment.
  NGLayoutOpportunity opportunity = FindLayoutOpportunityForFragment(
      tmp_space->Exclusions().get(), child_space.AvailableSize(), origin_offset,
      child_data.margins, fragment.Size());

  NGMarginStrut margin_strut = previous_inflow_position.margin_strut;

  // 3. If the found opportunity lies on the same line with our estimated
  //    child's BFC offset then merge fragment's margins with the current
  //    MarginStrut.
  if (opportunity.offset.block_offset == child_bfc_offset_estimate)
    margin_strut.Append(child_data.margins.block_start);
  child_bfc_offset_estimate += margin_strut.Sum();

  // 4. The child's BFC block offset is known here.
  bool updated = MaybeUpdateFragmentBfcOffset(
      ConstraintSpace(), child_bfc_offset_estimate, &container_builder_);

  if (updated && abort_when_bfc_resolved_)
    return false;

  PositionPendingFloats(child_bfc_offset_estimate, &container_builder_,
                        &unpositioned_floats_, MutableConstraintSpace());

  origin_offset = {ConstraintSpace().BfcOffset().inline_offset +
                       border_scrollbar_padding_.inline_start,
                   child_bfc_offset_estimate};
  AdjustToClearance(
      GetClearanceOffset(ConstraintSpace().Exclusions(), child_style.Clear()),
      &origin_offset);

  // 5. Find the final layout opportunity for the fragment after all pending
  // floats are positioned at the correct BFC block's offset.
  opportunity = FindLayoutOpportunityForFragment(
      MutableConstraintSpace()->Exclusions().get(), child_space.AvailableSize(),
      origin_offset, child_data.margins, fragment.Size());

  *child_bfc_offset = opportunity.offset;
  return true;
}

bool NGBlockLayoutAlgorithm::PositionWithBfcOffset(
    const NGLogicalOffset& bfc_offset,
    WTF::Optional<NGLogicalOffset>* child_bfc_offset) {
  LayoutUnit bfc_block_offset = bfc_offset.block_offset;
  bool updated = MaybeUpdateFragmentBfcOffset(
      ConstraintSpace(), bfc_block_offset, &container_builder_);

  if (updated && abort_when_bfc_resolved_)
    return false;

  PositionPendingFloats(bfc_block_offset, &container_builder_,
                        &unpositioned_floats_, MutableConstraintSpace());

  *child_bfc_offset = bfc_offset;
  return true;
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionWithParentBfc(
    const NGConstraintSpace& space,
    const NGInflowChildData& child_data,
    const NGLayoutResult& layout_result) {
  // The child must be an in-flow zero-block-size fragment, use its end margin
  // strut for positioning.
  DCHECK(IsEmptyFragment(ConstraintSpace().WritingMode(), layout_result));

  NGLogicalOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().inline_offset +
          border_scrollbar_padding_.inline_start +
          child_data.margins.inline_start,
      child_data.bfc_offset_estimate.block_offset +
          layout_result.EndMarginStrut().Sum()};

  AdjustToClearance(space.ClearanceOffset(), &child_bfc_offset);
  return child_bfc_offset;
}

void NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  LayoutUnit used_block_size =
      BreakToken() ? BreakToken()->UsedBlockSize() : LayoutUnit();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), used_block_size + content_size_);

  block_size -= used_block_size;
  DCHECK_GE(block_size, LayoutUnit())
      << "Adding and subtracting the used_block_size shouldn't leave the "
         "block_size for this fragment smaller than zero.";

  LayoutUnit space_left = ConstraintSpace().FragmentainerSpaceAvailable() -
                          ContainerBfcOffset().block_offset;
  DCHECK_GE(space_left, LayoutUnit());

  if (container_builder_.DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space we
    // need to prepare a break token.
    container_builder_.SetUsedBlockSize(std::min(space_left, block_size) +
                                        used_block_size);
    container_builder_.SetBlockSize(std::min(space_left, block_size));
    container_builder_.SetBlockOverflow(space_left);
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    container_builder_.SetUsedBlockSize(space_left + used_block_size);
    container_builder_.SetBlockSize(space_left);
    container_builder_.SetBlockOverflow(space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  container_builder_.SetBlockSize(block_size);
  container_builder_.SetBlockOverflow(content_size_);
}

NGBoxStrut NGBlockLayoutAlgorithm::CalculateMargins(NGLayoutInputNode child) {
  DCHECK(child);
  if (child.IsInline())
    return {};
  const ComputedStyle& child_style = child.Style();

  RefPtr<NGConstraintSpace> space =
      NGConstraintSpaceBuilder(MutableConstraintSpace())
          .SetAvailableSize(child_available_size_)
          .SetPercentageResolutionSize(child_percentage_size_)
          .ToConstraintSpace(ConstraintSpace().WritingMode());

  NGBoxStrut margins = ComputeMargins(*space, child_style, space->WritingMode(),
                                      space->Direction());

  // TODO(ikilpatrick): Move the auto margins calculation for different writing
  // modes to post-layout.
  if (!child.IsFloating()) {
    WTF::Optional<MinMaxContentSize> sizes;
    if (NeedMinMaxContentSize(*space, child_style))
      sizes = child.ComputeMinMaxContentSize();

    LayoutUnit child_inline_size =
        ComputeInlineSizeForFragment(*space, child_style, sizes);
    ApplyAutoMargins(*space, child_style, child_inline_size, &margins);
  }
  return margins;
}

RefPtr<NGConstraintSpace> NGBlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const WTF::Optional<NGLogicalOffset> floats_bfc_offset) {
  NGConstraintSpaceBuilder space_builder(MutableConstraintSpace());
  space_builder.SetAvailableSize(child_available_size_)
      .SetPercentageResolutionSize(child_percentage_size_);

  if (NGBaseline::ShouldPropagateBaselines(child))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  bool is_new_fc = child.CreatesNewFormattingContext();
  space_builder.SetIsNewFormattingContext(is_new_fc)
      .SetBfcOffset(child_data.bfc_offset_estimate)
      .SetMarginStrut(child_data.margin_strut);

  if (!container_builder_.BfcOffset() && ConstraintSpace().FloatsBfcOffset()) {
    space_builder.SetFloatsBfcOffset(
        NGLogicalOffset{child_data.bfc_offset_estimate.inline_offset,
                        ConstraintSpace().FloatsBfcOffset()->block_offset});
  }

  if (floats_bfc_offset)
    space_builder.SetFloatsBfcOffset(floats_bfc_offset);

  if (!is_new_fc && !floats_bfc_offset) {
    space_builder.SetUnpositionedFloats(unpositioned_floats_);
  }

  if (child.IsInline()) {
    // TODO(kojii): Setup space_builder appropriately for inline child.
    space_builder.SetClearanceOffset(ConstraintSpace().ClearanceOffset());
    return space_builder.ToConstraintSpace(
        FromPlatformWritingMode(Style().GetWritingMode()));
  }

  const ComputedStyle& child_style = child.Style();
  space_builder
      .SetClearanceOffset(GetClearanceOffset(constraint_space_->Exclusions(),
                                             child_style.Clear()))
      .SetIsShrinkToFit(ShouldShrinkToFit(Style(), child_style))
      .SetTextDirection(child_style.Direction());

  LayoutUnit space_available;
  if (constraint_space_->HasBlockFragmentation()) {
    space_available = ConstraintSpace().FragmentainerSpaceAvailable();
    // If a block establishes a new formatting context we must know our
    // position in the formatting context, and are able to adjust the
    // fragmentation line.
    if (is_new_fc) {
      space_available -= child_data.bfc_offset_estimate.block_offset;
    }
  }
  space_builder.SetFragmentainerSpaceAvailable(space_available);

  return space_builder.ToConstraintSpace(
      FromPlatformWritingMode(child_style.GetWritingMode()));
}

bool NGBlockLayoutAlgorithm::AddBaseline(const NGBaselineRequest& request,
                                         unsigned child_index) {
  const NGPhysicalFragment* child =
      container_builder_.Children()[child_index].Get();
  if (!child->IsBox())
    return false;
  LayoutObject* layout_object = child->GetLayoutObject();
  if (layout_object->IsFloatingOrOutOfFlowPositioned())
    return false;

  const NGPhysicalBoxFragment* box = ToNGPhysicalBoxFragment(child);
  if (const NGBaseline* baseline = box->Baseline(request)) {
    container_builder_.AddBaseline(
        request.algorithm_type, request.baseline_type,
        baseline->offset +
            container_builder_.Offsets()[child_index].block_offset);
    return true;
  }
  return false;
}

// Propagate computed baselines from children.
// Skip children that do not produce baselines (e.g., empty blocks.)
void NGBlockLayoutAlgorithm::PropagateBaselinesFromChildren() {
  const Vector<NGBaselineRequest>& requests =
      ConstraintSpace().BaselineRequests();
  if (requests.IsEmpty())
    return;

  for (const auto& request : requests) {
    switch (request.algorithm_type) {
      case NGBaselineAlgorithmType::kAtomicInline:
      case NGBaselineAlgorithmType::kAtomicInlineForFirstLine:
        for (unsigned i = container_builder_.Children().size(); i--;) {
          if (AddBaseline(request, i))
            break;
        }
        break;
      case NGBaselineAlgorithmType::kFirstLine:
        for (unsigned i = 0; i < container_builder_.Children().size(); i++) {
          if (AddBaseline(request, i))
            break;
        }
        break;
    }
  }
}

}  // namespace blink
