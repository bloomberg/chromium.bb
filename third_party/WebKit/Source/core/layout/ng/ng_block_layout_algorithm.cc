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
#include "core/layout/ng/ng_unpositioned_float.h"
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

// Returns if the resulting fragment should be considered an "empty block".
// There is special casing for fragments like this, e.g. margins "collapse
// through", etc.
bool IsEmptyBlock(const NGLayoutInputNode child,
                  const NGLayoutResult& layout_result) {
  if (child.CreatesNewFormattingContext())
    return false;

  if (layout_result.BfcOffset())
    return false;

#if DCHECK_IS_ON()
  // This just checks that the fragments block size is actually zero. We can
  // assume that its in the same writing mode as its parent, as a different
  // writing mode child will be caught by the CreatesNewFormattingContext check.
  NGFragment fragment(FromPlatformWritingMode(child.Style().GetWritingMode()),
                      layout_result.PhysicalFragment().Get());
  DCHECK_EQ(LayoutUnit(), fragment.BlockSize());
#endif

  return true;
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
    container_builder->AddChild(positioned_float.layout_result,
                                positioned_float.logical_offset);

  unpositioned_floats->clear();
}

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(NGBlockNode node,
                                               NGConstraintSpace* space,
                                               NGBlockBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, break_token) {}

Optional<MinMaxSize> NGBlockLayoutAlgorithm::ComputeMinMaxSize() const {
  MinMaxSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().ContainsSize())
    return sizes;

  // TODO: handle floats & orthogonal children.
  for (NGLayoutInputNode node = Node().FirstChild(); node;
       node = node.NextSibling()) {
    if (node.IsOutOfFlowPositioned())
      continue;
    MinMaxSize child_sizes;
    if (node.IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |node| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes = node.ComputeMinMaxSize();
    } else {
      Optional<MinMaxSize> child_minmax;
      if (NeedMinMaxSizeForContentContribution(node.Style())) {
        child_minmax = node.ComputeMinMaxSize();
      }

      child_sizes =
          ComputeMinAndMaxContentContribution(node.Style(), child_minmax);
    }

    sizes.min_size = std::max(sizes.min_size, child_sizes.min_size);
    sizes.max_size = std::max(sizes.max_size, child_sizes.max_size);
  }

  sizes.max_size = std::max(sizes.min_size, sizes.max_size);
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
  WTF::Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxSize();

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

  container_builder_.SetSize(size);

  // If we have a list of unpositioned floats as input to this layout, we'll
  // need to abort once our BFC offset is resolved. Additionally the
  // FloatsBfcOffset() must not be present in this case.
  unpositioned_floats_ = constraint_space_->UnpositionedFloats();
  abort_when_bfc_resolved_ = !unpositioned_floats_.IsEmpty();
  if (abort_when_bfc_resolved_)
    DCHECK(!constraint_space_->FloatsBfcOffset());

  // If we are resuming from a break token our start border and padding is
  // within a previous fragment.
  content_size_ =
      BreakToken() ? LayoutUnit() : border_scrollbar_padding_.block_start;

  NGMarginStrut input_margin_strut = ConstraintSpace().MarginStrut();

  // If this node is a quirky container, (we are in quirks mode and either a
  // table cell or body), we set our margin strut to a mode where it only
  // considers non-quirky margins. E.g.
  // <body>
  //   <p></p>
  //   <div style="margin-top: 10px"></div>
  //   <h1>Hello</h1>
  // </body>
  // In the above example <p>'s & <h1>'s margins are ignored as they are
  // quirky, and we only consider <div>'s 10px margin.
  if (node_.IsQuirkyContainer())
    input_margin_strut.is_quirky_container_start = true;

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
    DCHECK_EQ(input_margin_strut.positive_margin, LayoutUnit());
    DCHECK_EQ(input_margin_strut.negative_margin, LayoutUnit());
    DCHECK_EQ(container_builder_.BfcOffset().value(), NGLogicalOffset());
  }

  input_bfc_block_offset += content_size_;

  NGPreviousInflowPosition previous_inflow_position = {
      input_bfc_block_offset, content_size_, input_margin_strut,
      /* empty_block_affected_by_clearance */ false};

  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken());
  for (auto entry = child_iterator.NextChild();
       NGLayoutInputNode child = entry.node;
       entry = child_iterator.NextChild()) {
    NGBreakToken* child_break_token = entry.token;

    if (child.IsOutOfFlowPositioned()) {
      DCHECK(!child_break_token);
      HandleOutOfFlowPositioned(previous_inflow_position, ToNGBlockNode(child));
    } else if (child.IsFloating()) {
      HandleFloat(previous_inflow_position, ToNGBlockNode(child),
                  ToNGBlockBreakToken(child_break_token));
    } else {
      if (!HandleInflow(child, child_break_token, &previous_inflow_position)) {
        // We need to abort the layout, as our BFC offset was resolved.
        container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
        return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
      }
    }

    if (IsOutOfSpace(ConstraintSpace(), content_size_))
      break;
  }

  NGMarginStrut end_margin_strut = previous_inflow_position.margin_strut;
  LayoutUnit end_bfc_block_offset = previous_inflow_position.bfc_block_offset;

  // The end margin strut of an in-flow fragment contributes to the size of the
  // current fragment if:
  //  - There is block-end border/scrollbar/padding.
  //  - There was empty block(s) affected by clearance.
  //  - We are a new formatting context.
  // Additionally this fragment produces no end margin strut.
  if (border_scrollbar_padding_.block_end ||
      previous_inflow_position.empty_block_affected_by_clearance ||
      ConstraintSpace().IsNewFormattingContext()) {
    // TODO(ikilpatrick): If we are a quirky container and our last child had a
    // quirky block end margin, we need to use the margin strut without the
    // quirky margin appended. - http://jsbin.com/yizinagupo/edit?html,output
    content_size_ =
        std::max(content_size_, previous_inflow_position.logical_block_offset +
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
    // TODO(ikilpatrick): This looks wrong with end_margin_strut above?
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

bool NGBlockLayoutAlgorithm::HandleInflow(
    NGLayoutInputNode child,
    NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());

  // TODO(ikilpatrick): We may only want to position pending floats if there is
  // something that we *might* clear in the unpositioned list. E.g. we may
  // clear an already placed left float, but the unpositioned list may only have
  // right floats.
  bool should_position_pending_floats =
      !child.CreatesNewFormattingContext() && child.IsBlock() &&
      ClearanceMayAffectLayout(ConstraintSpace(), unpositioned_floats_,
                               child.Style());

  // Children which may clear a float need to force all the pending floats to
  // be positioned before layout. This also resolves our BFC offset.
  if (should_position_pending_floats) {
    LayoutUnit origin_point_block_offset =
        previous_inflow_position->bfc_block_offset +
        previous_inflow_position->margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(
        ConstraintSpace(), origin_point_block_offset, &container_builder_);

    if (updated && abort_when_bfc_resolved_)
      return false;

    bool positioned_direct_child_floats = !unpositioned_floats_.IsEmpty();

    // TODO(ikilpatrick): Check if origin_point_block_offset is correct -
    // MaybeUpdateFragmentBfcOffset might have changed it due to clearance.
    PositionPendingFloats(origin_point_block_offset, &container_builder_,
                          &unpositioned_floats_, MutableConstraintSpace());

    // When we have resolved our BFC (as a child is going to clear some floats),
    // *and* we positioned floats which are direct children, we need to
    // artificially "reset" the previous inflow position, e.g. we clear the
    // margin strut, and set the offset to our block-start border edge.
    //
    // This behaviour is similar to if we had block-start border or padding.
    if (positioned_direct_child_floats && updated) {
      // We must have no border/scrollbar/padding here otherwise our BFC offset
      // would already be resolved.
      DCHECK_EQ(border_scrollbar_padding_.block_start, LayoutUnit());

      previous_inflow_position->bfc_block_offset =
          container_builder_.BfcOffset()->block_offset;
      previous_inflow_position->margin_strut = NGMarginStrut();
      previous_inflow_position->logical_block_offset = LayoutUnit();
    }
  }

  // Perform layout on the child.
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child);
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
    return false;
  }

  // We have special behaviour for an empty block which gets pushed down due to
  // clearance, see comment inside ComputeInflowPosition.
  bool empty_block_affected_by_clearance = false;

  // We try and position the child within the block formatting context. This
  // may cause our BFC offset to be resolved, in which case we should abort our
  // layout if needed.
  WTF::Optional<NGLogicalOffset> child_bfc_offset;
  if (child.CreatesNewFormattingContext()) {
    if (!PositionNewFc(child, *previous_inflow_position, *layout_result,
                       child_data, *child_space, &child_bfc_offset))
      return false;
  } else if (layout_result->BfcOffset()) {
    if (!PositionWithBfcOffset(layout_result->BfcOffset().value(),
                               &child_bfc_offset))
      return false;
  } else if (container_builder_.BfcOffset()) {
    child_bfc_offset =
        PositionWithParentBfc(child, *child_space, child_data, *layout_result,
                              &empty_block_affected_by_clearance);
  } else
    DCHECK(IsEmptyBlock(child, *layout_result));

  // We need to re-layout a child if it was affected by clearance in order to
  // produce a new margin strut. For example:
  // <div style="margin-bottom: 50px;"></div>
  // <div id="float" style="height: 50px;"></div>
  // <div id="zero" style="clear: left; margin-top: -20px;">
  //   <div id="zero-inner" style="margin-top: 40px; margin-bottom: -30px;">
  // </div>
  //
  // The end margin strut for #zero will be {50, -30}. #zero will be affected
  // by clearance (as 50 > {50, -30}).
  //
  // As #zero doesn't touch the incoming margin strut now we need to perform a
  // relayout with an empty incoming margin strut.
  //
  // The resulting margin strut in the above example will be {40, -30}. See
  // ComputeInflowPosition for how this end margin strut is used.
  bool empty_block_affected_by_clearance_needs_relayout = false;
  if (empty_block_affected_by_clearance) {
    NGMarginStrut margin_strut;
    margin_strut.Append(child_data.margins.block_start,
                        child.Style().HasMarginBeforeQuirk());

    // We only need to relayout if the new margin strut is different to the
    // previous one.
    if (child_data.margin_strut != margin_strut) {
      child_data.margin_strut = margin_strut;
      empty_block_affected_by_clearance_needs_relayout = true;
    }
  }

  // We need to layout a child if we know its BFC offset and:
  //  - It aborted its layout as it resolved its BFC offset.
  //  - It has some unpositioned floats.
  //  - It was affected by clearance.
  if ((layout_result->Status() == NGLayoutResult::kBfcOffsetResolved ||
       !layout_result->UnpositionedFloats().IsEmpty() ||
       empty_block_affected_by_clearance_needs_relayout) &&
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

  // Only modify content_size_ if the fragment is non-empty block.
  //
  // Empty blocks don't immediately contribute to our size, instead we wait to
  // see what the final margin produced, e.g.
  // <div style="display: flow-root">
  //   <div style="margin-top: -8px"></div>
  //   <div style="margin-top: 10px"></div>
  // </div>
  if (!IsEmptyBlock(child, *layout_result)) {
    DCHECK(container_builder_.BfcOffset());
    content_size_ = std::max(
        content_size_, logical_offset.block_offset + fragment.BlockSize());
  }
  max_inline_size_ = std::max(
      max_inline_size_, fragment.InlineSize() + child_data.margins.InlineSum() +
                            border_scrollbar_padding_.InlineSum());

  container_builder_.AddChild(layout_result, logical_offset);

  *previous_inflow_position =
      ComputeInflowPosition(*previous_inflow_position, child, child_data,
                            child_bfc_offset, logical_offset, *layout_result,
                            fragment, empty_block_affected_by_clearance);
  return true;
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
  margin_strut.Append(margins.block_start,
                      child.Style().HasMarginBeforeQuirk());

  NGLogicalOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().inline_offset +
          border_scrollbar_padding_.inline_start + margins.inline_start,
      previous_inflow_position.bfc_block_offset};

  return {child_bfc_offset, margin_strut, margins};
}

NGPreviousInflowPosition NGBlockLayoutAlgorithm::ComputeInflowPosition(
    const NGPreviousInflowPosition& previous_inflow_position,
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const WTF::Optional<NGLogicalOffset>& child_bfc_offset,
    const NGLogicalOffset& logical_offset,
    const NGLayoutResult& layout_result,
    const NGFragment& fragment,
    bool empty_block_affected_by_clearance) {
  // Determine the child's end BFC block offset and logical offset, for the
  // next child to use.
  LayoutUnit child_end_bfc_block_offset;
  LayoutUnit logical_block_offset;

  bool is_empty_block = IsEmptyBlock(child, layout_result);
  if (is_empty_block) {
    if (empty_block_affected_by_clearance) {
      // If an empty block was affected by clearance (that is it got pushed
      // down past a float), we need to do something slightly bizarre.
      //
      // Instead of just passing through the previous inflow position, we make
      // the inflow position our new position (which was affected by the
      // float), minus what the margin strut which the empty block produced.
      //
      // Another way of thinking about this is that when you *add* back the
      // margin strut, you end up with the same position as you started with.
      //
      // This behaviour isn't known to be in any CSS specification.
      child_end_bfc_block_offset = child_bfc_offset.value().block_offset -
                                   layout_result.EndMarginStrut().Sum();
      logical_block_offset =
          logical_offset.block_offset - layout_result.EndMarginStrut().Sum();
    } else {
      // The default behaviour for empty blocks is they just pass through the
      // previous inflow position.
      child_end_bfc_block_offset = previous_inflow_position.bfc_block_offset;
      logical_block_offset = previous_inflow_position.logical_block_offset;
    }

    if (!container_builder_.BfcOffset()) {
      DCHECK_EQ(child_end_bfc_block_offset,
                ConstraintSpace().BfcOffset().block_offset);
      DCHECK_EQ(logical_block_offset, LayoutUnit());
    }
  } else {
    child_end_bfc_block_offset =
        child_bfc_offset.value().block_offset + fragment.BlockSize();
    logical_block_offset = logical_offset.block_offset + fragment.BlockSize();
  }

  NGMarginStrut margin_strut = layout_result.EndMarginStrut();
  margin_strut.Append(child_data.margins.block_end,
                      child.Style().HasMarginAfterQuirk());

  // This flag is subtle, but in order to determine our size correctly we need
  // to check if our last child is an empty block, and it was affected by
  // clearance *or* an adjoining empty sibling was affected by clearance. E.g.
  // <div id="container">
  //   <div id="float"></div>
  //   <div id="zero-with-clearance"></div>
  //   <div id="another-zero"></div>
  // </div>
  // In the above case #container's size will depend on the end margin strut of
  // #another-zero, even though usually it wouldn't.
  bool empty_or_sibling_empty_affected_by_clearance =
      empty_block_affected_by_clearance ||
      (previous_inflow_position.empty_block_affected_by_clearance &&
       is_empty_block);

  return {child_end_bfc_block_offset, logical_block_offset, margin_strut,
          empty_or_sibling_empty_affected_by_clearance};
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
    margin_strut.Append(child_data.margins.block_start,
                        child.Style().HasMarginBeforeQuirk());
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
    const NGLayoutInputNode& child,
    const NGConstraintSpace& space,
    const NGInflowChildData& child_data,
    const NGLayoutResult& layout_result,
    bool* empty_block_affected_by_clearance) {
  DCHECK(IsEmptyBlock(child, layout_result));

  // The child must be an in-flow zero-block-size fragment, use its end margin
  // strut for positioning.
  NGLogicalOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().inline_offset +
          border_scrollbar_padding_.inline_start +
          child_data.margins.inline_start,
      child_data.bfc_offset_estimate.block_offset +
          layout_result.EndMarginStrut().Sum()};

  *empty_block_affected_by_clearance =
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
          .ToConstraintSpace(
              FromPlatformWritingMode(child_style.GetWritingMode()));

  NGBoxStrut margins =
      ComputeMargins(*space, child_style, ConstraintSpace().WritingMode(),
                     ConstraintSpace().Direction());

  // TODO(ikilpatrick): Move the auto margins calculation for different writing
  // modes to post-layout.
  if (!child.IsFloating()) {
    WTF::Optional<MinMaxSize> sizes;
    if (NeedMinMaxSize(*space, child_style))
      sizes = child.ComputeMinMaxSize();

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

// Add a baseline from a child box fragment.
// @return false if the specified child is not a box or is OOF.
bool NGBlockLayoutAlgorithm::AddBaseline(const NGBaselineRequest& request,
                                         const NGPhysicalFragment* child,
                                         LayoutUnit child_offset) {
  if (!child->IsBox())
    return false;
  LayoutObject* layout_object = child->GetLayoutObject();
  if (layout_object->IsFloatingOrOutOfFlowPositioned())
    return false;

  const NGPhysicalBoxFragment* box = ToNGPhysicalBoxFragment(child);
  if (const NGBaseline* baseline = box->Baseline(request)) {
    container_builder_.AddBaseline(request, baseline->offset + child_offset);
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
          if (AddBaseline(request, container_builder_.Children()[i].Get(),
                          container_builder_.Offsets()[i].block_offset))
            break;
        }
        break;
      case NGBaselineAlgorithmType::kFirstLine:
        for (unsigned i = 0; i < container_builder_.Children().size(); i++) {
          if (AddBaseline(request, container_builder_.Children()[i].Get(),
                          container_builder_.Offsets()[i].block_offset))
            break;
        }
        break;
    }
  }
}

}  // namespace blink
