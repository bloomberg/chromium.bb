// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/ng_block_child_iterator.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_fragmentation_utils.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/Optional.h"

namespace blink {
namespace {

// Returns if a child may be affected by its clear property. I.e. it will
// actually clear a float.
bool ClearanceMayAffectLayout(
    const NGExclusionSpace& exclusion_space,
    const Vector<scoped_refptr<NGUnpositionedFloat>>& unpositioned_floats,
    const ComputedStyle& child_style) {
  EClear clear = child_style.Clear();
  bool should_clear_left = (clear == EClear::kBoth || clear == EClear::kLeft);
  bool should_clear_right = (clear == EClear::kBoth || clear == EClear::kRight);

  if (exclusion_space.HasLeftFloat() && should_clear_left)
    return true;

  if (exclusion_space.HasRightFloat() && should_clear_right)
    return true;

  auto should_clear_pred =
      [&](const scoped_refptr<const NGUnpositionedFloat>& unpositioned_float) {
        return (unpositioned_float->IsLeft() && should_clear_left) ||
               (unpositioned_float->IsRight() && should_clear_right);
      };

  if (std::any_of(unpositioned_floats.begin(), unpositioned_floats.end(),
                  should_clear_pred))
    return true;

  return false;
}

// Returns if the resulting fragment should be considered an "empty block".
// There is special casing for fragments like this, e.g. margins "collapse
// through", etc.
bool IsEmptyBlock(const NGLayoutInputNode child,
                  const NGLayoutResult& layout_result) {
  // TODO(ikilpatrick): This should be a DCHECK.
  if (child.CreatesNewFormattingContext())
    return false;

  if (layout_result.BfcOffset())
    return false;

#if DCHECK_IS_ON()
  // This just checks that the fragments block size is actually zero. We can
  // assume that its in the same writing mode as its parent, as a different
  // writing mode child will be caught by the CreatesNewFormattingContext check.
  NGFragment fragment(child.Style().GetWritingMode(),
                      *layout_result.PhysicalFragment());
  DCHECK_EQ(LayoutUnit(), fragment.BlockSize());
#endif

  return true;
}

NGLogicalOffset LogicalFromBfcOffsets(const NGFragment& fragment,
                                      const NGBfcOffset& child_bfc_offset,
                                      const NGBfcOffset& parent_bfc_offset,
                                      LayoutUnit parent_inline_size,
                                      TextDirection direction) {
  // We need to respect the current text direction to calculate the logical
  // offset correctly.
  LayoutUnit relative_line_offset =
      child_bfc_offset.line_offset - parent_bfc_offset.line_offset;

  LayoutUnit inline_offset =
      direction == TextDirection::kLtr
          ? relative_line_offset
          : parent_inline_size - relative_line_offset - fragment.InlineSize();

  return {inline_offset,
          child_bfc_offset.block_offset - parent_bfc_offset.block_offset};
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(NGBlockNode node,
                                               const NGConstraintSpace& space,
                                               NGBlockBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, break_token),
      is_resuming_(break_token && !break_token->IsBreakBefore()),
      exclusion_space_(new NGExclusionSpace(space.ExclusionSpace())) {}

Optional<MinMaxSize> NGBlockLayoutAlgorithm::ComputeMinMaxSize() const {
  MinMaxSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().ContainsSize())
    return sizes;

  const TextDirection direction = Style().Direction();
  LayoutUnit float_left_inline_size;
  LayoutUnit float_right_inline_size;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned() || child.IsColumnSpanAll())
      continue;

    const ComputedStyle& child_style = child.Style();
    const EClear child_clear = child_style.Clear();

    // Conceptually floats and a single new-FC would just get positioned on a
    // single "line". If there is a float/new-FC with clearance, this creates a
    // new "line", resetting the appropriate float size trackers.
    //
    // Both of the float size trackers get reset for anything that isn't a float
    // (inflow and new-FC) at the end of the loop, as this creates a new "line".
    if (child.IsFloating() || child.CreatesNewFormattingContext()) {
      LayoutUnit float_inline_size =
          float_left_inline_size + float_right_inline_size;

      if (child_clear != EClear::kNone)
        sizes.max_size = std::max(sizes.max_size, float_inline_size);

      if (child_clear == EClear::kBoth || child_clear == EClear::kLeft)
        float_left_inline_size = LayoutUnit();

      if (child_clear == EClear::kBoth || child_clear == EClear::kRight)
        float_right_inline_size = LayoutUnit();
    }

    MinMaxSize child_sizes;
    if (child.IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |child| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes = child.ComputeMinMaxSize();
    } else {
      Optional<MinMaxSize> child_minmax;
      if (NeedMinMaxSizeForContentContribution(child_style)) {
        child_minmax = child.ComputeMinMaxSize();
      }

      child_sizes =
          ComputeMinAndMaxContentContribution(child_style, child_minmax);
    }

    // Determine the max inline contribution of the child.
    NGBoxStrut margins = ComputeMinMaxMargins(Style(), child);
    LayoutUnit max_inline_contribution;

    if (child.IsFloating()) {
      // A float adds to its inline size to the current "line". The new max
      // inline contribution is just the sum of all the floats on that "line".
      LayoutUnit float_inline_size = child_sizes.max_size + margins.InlineSum();
      if (child_style.Floating() == EFloat::kLeft)
        float_left_inline_size += float_inline_size;
      else
        float_right_inline_size += float_inline_size;

      max_inline_contribution =
          float_left_inline_size + float_right_inline_size;
    } else if (child.CreatesNewFormattingContext()) {
      // As floats are line relative, we perform the margin calculations in the
      // line relative coordinate system as well.
      LayoutUnit margin_line_left = margins.LineLeft(direction);
      LayoutUnit margin_line_right = margins.LineRight(direction);

      // line_left_inset and line_right_inset are the "distance" from their
      // respective edges of the parent that the new-FC would take. If the
      // margin is positive the inset is just whichever of the floats inline
      // size and margin is larger, and if negative it just subtracts from the
      // float inline size.
      LayoutUnit line_left_inset =
          margin_line_left > LayoutUnit()
              ? std::max(float_left_inline_size, margin_line_left)
              : float_left_inline_size + margin_line_left;

      LayoutUnit line_right_inset =
          margin_line_right > LayoutUnit()
              ? std::max(float_right_inline_size, margin_line_right)
              : float_right_inline_size + margin_line_right;

      max_inline_contribution =
          child_sizes.max_size + line_left_inset + line_right_inset;
    } else {
      // This is just a standard inflow child.
      max_inline_contribution = child_sizes.max_size + margins.InlineSum();
    }
    sizes.max_size = std::max(sizes.max_size, max_inline_contribution);

    // The min inline contribution just assumes that floats are all on their own
    // "line".
    LayoutUnit min_inline_contribution =
        child_sizes.min_size + margins.InlineSum();
    sizes.min_size = std::max(sizes.min_size, min_inline_contribution);

    // Anything that isn't a float will create a new "line" resetting the float
    // size trackers.
    if (!child.IsFloating()) {
      float_left_inline_size = LayoutUnit();
      float_right_inline_size = LayoutUnit();
    }
  }

  DCHECK_GE(sizes.min_size, LayoutUnit());
  DCHECK_GE(sizes.max_size, sizes.min_size);

  return sizes;
}

NGLogicalOffset NGBlockLayoutAlgorithm::CalculateLogicalOffset(
    const NGFragment& fragment,
    const NGBoxStrut& child_margins,
    const WTF::Optional<NGBfcOffset>& known_fragment_offset) {
  if (known_fragment_offset) {
    return LogicalFromBfcOffsets(
        fragment, known_fragment_offset.value(), ContainerBfcOffset(),
        container_builder_.Size().inline_size, ConstraintSpace().Direction());
  }

  LayoutUnit inline_offset =
      border_scrollbar_padding_.inline_start + child_margins.inline_start;

  // If we've reached here, both the child and the current layout don't have a
  // BFC offset yet. Children in this situation are always placed at a logical
  // block offset of 0.
  DCHECK(!container_builder_.BfcOffset());
  return {inline_offset, LayoutUnit()};
}

scoped_refptr<NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  WTF::Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxSize();

  border_scrollbar_padding_ =
      CalculateBorderScrollbarPadding(ConstraintSpace(), Style(), Node());

  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
  // NGSizeIndefinite?
  NGLogicalSize size = CalculateBorderBoxSize(ConstraintSpace(), Style(),
                                              min_max_size, NGSizeIndefinite);

  // Our calculated block-axis size may be indefinite at this point.
  // If so, just leave the size as NGSizeIndefinite instead of subtracting
  // borders and padding.
  NGLogicalSize adjusted_size =
      CalculateContentBoxSize(size, border_scrollbar_padding_);

  child_available_size_ = adjusted_size;

  // Anonymous constraint spaces are auto-sized. Don't let that affect
  // block-axis percentage resolution.
  if (ConstraintSpace().IsAnonymous())
    child_percentage_size_ = ConstraintSpace().PercentageResolutionSize();
  else
    child_percentage_size_ = adjusted_size;

  container_builder_.SetInlineSize(size.inline_size);

  // If we have a list of unpositioned floats as input to this layout, we'll
  // need to abort once our BFC offset is resolved. Additionally the
  // FloatsBfcOffset() must not be present in this case.
  unpositioned_floats_ = ConstraintSpace().UnpositionedFloats();
  abort_when_bfc_resolved_ = !unpositioned_floats_.IsEmpty();
  if (abort_when_bfc_resolved_)
    DCHECK(!ConstraintSpace().FloatsBfcOffset());

  // If we are resuming from a break token our start border and padding is
  // within a previous fragment.
  intrinsic_block_size_ =
      is_resuming_ ? LayoutUnit() : border_scrollbar_padding_.block_start;

  NGMarginStrut input_margin_strut = ConstraintSpace().MarginStrut();

  LayoutUnit input_bfc_block_offset =
      ConstraintSpace().BfcOffset().block_offset;

  // Margins collapsing:
  //   Do not collapse margins between parent and its child if there is
  //   border/padding between them.
  if (border_scrollbar_padding_.block_start) {
    input_bfc_block_offset += input_margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(input_bfc_block_offset);

    if (updated && abort_when_bfc_resolved_) {
      container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
      return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
    }

    // We reset the block offset here as it may have been effected by clearance.
    input_bfc_block_offset = ContainerBfcOffset().block_offset;
    input_margin_strut = NGMarginStrut();
  }

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

  // If a new formatting context hits the margin collapsing if-branch above
  // then the BFC offset is still {} as the margin strut from the constraint
  // space must also be empty.
  // If we are resuming layout from a break token the same rule applies. Margin
  // struts cannot pass through break tokens (unless it's a break token before
  // the first fragment (the one we're about to create)).
  if (ConstraintSpace().IsNewFormattingContext() || is_resuming_) {
    MaybeUpdateFragmentBfcOffset(input_bfc_block_offset);
    DCHECK(input_margin_strut.IsEmpty());
#if DCHECK_IS_ON()
    // If this is a new formatting context, we should definitely be at the
    // origin here. If we're resuming at a fragmented block (that doesn't
    // establish a new formatting context), that may not be the case,
    // though. There may e.g. be clearance involved, or inline-start margins.
    if (ConstraintSpace().IsNewFormattingContext())
      DCHECK_EQ(container_builder_.BfcOffset().value(), NGBfcOffset());
#endif
  }

  input_bfc_block_offset += intrinsic_block_size_;

  NGPreviousInflowPosition previous_inflow_position = {
      input_bfc_block_offset, intrinsic_block_size_, input_margin_strut,
      /* empty_block_affected_by_clearance */ false};
  scoped_refptr<NGBreakToken> previous_inline_break_token;

  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken());
  for (auto entry = child_iterator.NextChild();
       NGLayoutInputNode child = entry.node;
       entry = child_iterator.NextChild(previous_inline_break_token.get())) {
    NGBreakToken* child_break_token = entry.token;

    if (child.IsOutOfFlowPositioned()) {
      DCHECK(!child_break_token);
      HandleOutOfFlowPositioned(previous_inflow_position, ToNGBlockNode(child));
    } else if (child.IsFloating()) {
      HandleFloat(previous_inflow_position, ToNGBlockNode(child),
                  ToNGBlockBreakToken(child_break_token));
    } else {
      // We need to propagate the initial break-before value up our container
      // chain, until we reach a container that's not a first child. If we get
      // all the way to the root of the fragmentation context without finding
      // any such container, we have no valid class A break point, and if a
      // forced break was requested, none will be inserted.
      container_builder_.SetInitialBreakBefore(child.Style().BreakBefore());

      bool success =
          child.CreatesNewFormattingContext()
              ? HandleNewFormattingContext(child, child_break_token,
                                           &previous_inflow_position,
                                           &previous_inline_break_token)
              : HandleInflow(child, child_break_token,
                             &previous_inflow_position,
                             &previous_inline_break_token);

      if (!success) {
        // We need to abort the layout, as our BFC offset was resolved.
        container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
        return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
      }
      if (container_builder_.DidBreak() && IsFragmentainerOutOfSpace())
        break;
      has_processed_first_child_ = true;
    }
  }

  NGMarginStrut end_margin_strut = previous_inflow_position.margin_strut;
  LayoutUnit end_bfc_block_offset = previous_inflow_position.bfc_block_offset;

  // If the current layout is a new formatting context, we need to encapsulate
  // all of our floats.
  if (ConstraintSpace().IsNewFormattingContext()) {
    // We can use the BFC coordinates, as we are a new formatting context.
    DCHECK_EQ(container_builder_.BfcOffset().value(), NGBfcOffset());

    WTF::Optional<LayoutUnit> float_end_offset =
        exclusion_space_->ClearanceOffset(EClear::kBoth);

    // We only update the size of this fragment if we need to grow to
    // encapsulate the floats.
    if (float_end_offset && float_end_offset.value() > end_bfc_block_offset) {
      end_margin_strut = NGMarginStrut();
      end_bfc_block_offset = float_end_offset.value();
      intrinsic_block_size_ =
          std::max(intrinsic_block_size_, float_end_offset.value());
    }
  }

  // The end margin strut of an in-flow fragment contributes to the size of the
  // current fragment if:
  //  - There is block-end border/scrollbar/padding.
  //  - There was empty block(s) affected by clearance.
  //  - We are a new formatting context.
  // Additionally this fragment produces no end margin strut.
  if (border_scrollbar_padding_.block_end ||
      previous_inflow_position.empty_block_affected_by_clearance ||
      ConstraintSpace().IsNewFormattingContext()) {
    // If we are a quirky container, we ignore any quirky margins and
    // just consider normal margins to extend our size.  Other UAs
    // perform this calculation differently, e.g. by just ignoring the
    // *last* quirky margin.
    // TODO: revisit previous implementation to avoid changing behavior and
    // https://html.spec.whatwg.org/multipage/rendering.html#margin-collapsing-quirks
    LayoutUnit margin_strut_sum = node_.IsQuirkyContainer()
                                      ? end_margin_strut.QuirkyContainerSum()
                                      : end_margin_strut.Sum();
    end_bfc_block_offset += margin_strut_sum;
    bool updated = MaybeUpdateFragmentBfcOffset(end_bfc_block_offset);

    if (updated && abort_when_bfc_resolved_) {
      // New formatting contexts, and where we have an empty block affected by
      // clearance should already have their BFC offset resolved, and shouldn't
      // enter this branch.
      DCHECK(!previous_inflow_position.empty_block_affected_by_clearance);
      DCHECK(!ConstraintSpace().IsNewFormattingContext());

      container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
      return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
    }

    PositionPendingFloats(end_bfc_block_offset);

    // We only grow the intrinsic block size if we have content (if we didn't
    // update our BFC offset). E.g.
    // <div style="margin-bottom: solid 20px"></div>
    // In the above example the current layout won't have resolved its BFC
    // offset yet, and shouldn't include the margin strut in its size.
    if (!updated) {
      intrinsic_block_size_ = std::max(
          intrinsic_block_size_,
          previous_inflow_position.logical_block_offset + margin_strut_sum);
    }

    intrinsic_block_size_ += border_scrollbar_padding_.block_end;
    end_margin_strut = NGMarginStrut();
  }

  // Recompute the block-axis size now that we know our content size.
  size.block_size = ComputeBlockSizeForFragment(ConstraintSpace(), Style(),
                                                intrinsic_block_size_);
  container_builder_.SetBlockSize(size.block_size);

  // Non-empty blocks always know their position in space.
  // TODO(ikilpatrick): This check for a break token seems error prone.
  if (size.block_size || BreakToken()) {
    // TODO(ikilpatrick): This looks wrong with end_margin_strut above?
    end_bfc_block_offset += end_margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(end_bfc_block_offset);

    if (updated && abort_when_bfc_resolved_) {
      container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
      return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
    }

    PositionPendingFloats(end_bfc_block_offset);
  }

  // Margins collapsing:
  //   Do not collapse margins between the last in-flow child and bottom margin
  //   of its parent if the parent has height != auto()
  if (!Style().LogicalHeight().IsAuto()) {
    // TODO(glebl): handle minLogicalHeight, maxLogicalHeight.
    end_margin_strut = NGMarginStrut();
  }

  container_builder_.SetEndMarginStrut(end_margin_strut);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);

  // We only finalize for fragmentation if the fragment has a BFC offset. This
  // may occur with a zero block size fragment. We need to know the BFC offset
  // to determine where the fragmentation line is relative to us.
  if (container_builder_.BfcOffset() &&
      ConstraintSpace().HasBlockFragmentation())
    FinalizeForFragmentation();

  // Only layout absolute and fixed children if we aren't going to revisit this
  // layout.
  if (unpositioned_floats_.IsEmpty()) {
    NGOutOfFlowLayoutPart(&container_builder_, Node().IsAbsoluteContainer(),
                          Node().IsFixedContainer(), Node().GetScrollbarSizes(),
                          ConstraintSpace(), Style())
        .Run();
  }

  // If we have any unpositioned floats at this stage, need to tell our parent
  // about this, so that we get relayout with a forced BFC offset.
  if (!unpositioned_floats_.IsEmpty()) {
    DCHECK(!container_builder_.BfcOffset());
    container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
  }

  PropagateBaselinesFromChildren();

  DCHECK(exclusion_space_);
  container_builder_.SetExclusionSpace(std::move(exclusion_space_));

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
    NGBlockBreakToken* child_break_token) {
  // Calculate margins in the BFC's writing mode.
  NGBoxStrut margins = CalculateMargins(child, child_break_token);

  LayoutUnit origin_inline_offset =
      ConstraintSpace().BfcOffset().line_offset +
      border_scrollbar_padding_.LineLeft(ConstraintSpace().Direction());

  scoped_refptr<NGUnpositionedFloat> unpositioned_float =
      NGUnpositionedFloat::Create(child_available_size_, child_percentage_size_,
                                  origin_inline_offset,
                                  ConstraintSpace().BfcOffset().line_offset,
                                  margins, child, child_break_token);
  unpositioned_floats_.push_back(std::move(unpositioned_float));

  // If there is a break token for a float we must be resuming layout, we must
  // always know our position in the BFC.
  DCHECK(!child_break_token || child_break_token->IsBreakBefore() ||
         container_builder_.BfcOffset());

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
    PositionPendingFloats(origin_block_offset);
  }
}

bool NGBlockLayoutAlgorithm::HandleNewFormattingContext(
    NGLayoutInputNode child,
    NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position,
    scoped_refptr<NGBreakToken>* previous_inline_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(child.CreatesNewFormattingContext());

  const ComputedStyle& child_style = child.Style();
  const TextDirection direction = ConstraintSpace().Direction();

  bool is_fixed_inline_size =
      (IsHorizontalWritingMode(ConstraintSpace().GetWritingMode())
           ? child_style.Width().IsAuto()
           : child_style.Height().IsAuto()) &&
      IsParallelWritingMode(ConstraintSpace().GetWritingMode(),
                            child_style.GetWritingMode()) &&
      !child.ShouldBeConsideredAsReplaced();

  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token);

  NGMarginStrut adjoining_margin_strut(previous_inflow_position->margin_strut);
  adjoining_margin_strut.Append(child_data.margins.block_start,
                                child_style.HasMarginBeforeQuirk());

  LayoutUnit initial_child_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset +
      adjoining_margin_strut.Sum();
  LayoutUnit child_bfc_offset_estimate = initial_child_bfc_offset_estimate;

  NGLayoutOpportunity opportunity;
  scoped_refptr<NGLayoutResult> layout_result;
  std::tie(layout_result, opportunity) =
      LayoutNewFormattingContext(child, child_break_token, is_fixed_inline_size,
                                 child_data, child_bfc_offset_estimate);

  DCHECK(layout_result->PhysicalFragment());
  LayoutUnit fragment_block_size =
      NGFragment(ConstraintSpace().GetWritingMode(),
                 *layout_result->PhysicalFragment())
          .BlockSize();

  // We allow a single re-layout for new formatting contexts. If the first
  // layout doesn't produce which fragment which sends up at the top of the
  // exclusion space, or it just doesn't fit, we relayout with a
  // *non*-adjoining margin strut.
  //
  // This re-layout *must* produce a fragment and opportunity which fits within
  // the exclusion space.
  if (opportunity.rect.start_offset.block_offset != child_bfc_offset_estimate ||
      (is_fixed_inline_size &&
       fragment_block_size > opportunity.rect.BlockSize())) {
    NGMarginStrut non_adjoining_margin_strut(
        previous_inflow_position->margin_strut);
    child_bfc_offset_estimate = child_data.bfc_offset_estimate.block_offset +
                                non_adjoining_margin_strut.Sum();

    std::tie(layout_result, opportunity) = LayoutNewFormattingContext(
        child, child_break_token, is_fixed_inline_size, child_data,
        child_bfc_offset_estimate);
  }

  // We now know the childs BFC offset, try and update our own if needed.
  bool updated = MaybeUpdateFragmentBfcOffset(child_bfc_offset_estimate);

  if (updated && abort_when_bfc_resolved_)
    return false;

  // Position any pending floats if we've just updated our BFC offset.
  PositionPendingFloats(child_bfc_offset_estimate);

  DCHECK(layout_result->PhysicalFragment());
  const auto& physical_fragment = *layout_result->PhysicalFragment();
  NGFragment fragment(ConstraintSpace().GetWritingMode(), physical_fragment);

  // Auto-margins are applied within the layout opportunity which fits.
  NGBoxStrut auto_margins;
  ApplyAutoMargins(child_style, Style(), opportunity.rect.InlineSize(),
                   fragment.InlineSize(), &auto_margins);

  NGBfcOffset child_bfc_offset(opportunity.rect.start_offset.line_offset +
                                   auto_margins.LineLeft(direction),
                               opportunity.rect.start_offset.block_offset);

  NGLogicalOffset logical_offset = LogicalFromBfcOffsets(
      fragment, child_bfc_offset, ContainerBfcOffset(),
      container_builder_.Size().inline_size, ConstraintSpace().Direction());

  if (ConstraintSpace().HasBlockFragmentation()) {
    bool is_pushed_by_floats =
        layout_result->IsPushedByFloats() ||
        child_bfc_offset.block_offset > initial_child_bfc_offset_estimate;
    if (BreakBeforeChild(child, *layout_result, logical_offset.block_offset,
                         is_pushed_by_floats))
      return true;
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result->FinalBreakAfter(), child.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);
  }

  intrinsic_block_size_ =
      std::max(intrinsic_block_size_,
               logical_offset.block_offset + fragment.BlockSize());

  container_builder_.AddChild(layout_result, logical_offset);
  container_builder_.PropagateBreak(layout_result);

  *previous_inflow_position = ComputeInflowPosition(
      *previous_inflow_position, child, child_data, child_bfc_offset,
      logical_offset, *layout_result, fragment,
      /* empty_block_affected_by_clearance */ false);
  *previous_inline_break_token = nullptr;

  return true;
}

std::pair<scoped_refptr<NGLayoutResult>, NGLayoutOpportunity>
NGBlockLayoutAlgorithm::LayoutNewFormattingContext(
    NGLayoutInputNode child,
    NGBreakToken* child_break_token,
    bool is_fixed_inline_size,
    const NGInflowChildData& child_data,
    LayoutUnit child_origin_block_offset) {
  const ComputedStyle& child_style = child.Style();
  const EClear child_clear = child_style.Clear();
  const TextDirection direction = ConstraintSpace().Direction();

  LayoutUnit child_bfc_line_offset =
      ConstraintSpace().BfcOffset().line_offset +
      border_scrollbar_padding_.LineLeft(direction) +
      child_data.margins.LineLeft(direction);

  // Position all the pending floats into a temporary exclusion space. This
  // *doesn't* place them into our output exclusion space yet, as we don't know
  // where the child will be positioned, and hence what *out* BFC offset is.
  // If we already know our BFC offset, this won't have any affect.
  NGExclusionSpace tmp_exclusion_space(*exclusion_space_);
  PositionFloats(child_origin_block_offset, child_origin_block_offset,
                 unpositioned_floats_, ConstraintSpace(), &tmp_exclusion_space);

  // The origin offset is where we should start looking for layout
  // opportunities. It needs to be adjusted by the child's clearance, in
  // addition to the parent's (if we don't know our BFC offset yet).
  NGBfcOffset origin_offset = {child_bfc_line_offset,
                               child_origin_block_offset};
  AdjustToClearance(tmp_exclusion_space.ClearanceOffset(child_clear),
                    &origin_offset);
  if (!container_builder_.BfcOffset())
    AdjustToClearance(ConstraintSpace().ClearanceOffset(), &origin_offset);

  // TODO(ikilpatrick): min_max_size, max_inline_size, min_inline_size,
  // is_fixed_inline_size, should be computed in ComputeChildData. This will
  // allow us to handle 'auto' in the parent by assigning a fixed size to our
  // children. This will require us to remove the requirement that
  // ResolveInlineLength to take a constraint space, instead taking a
  // {writing_mode, available_size, percentage_size} struct.
  scoped_refptr<NGConstraintSpace> child_space =
      CreateConstraintSpaceForChild(child, child_data);

  WTF::Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(*child_space, child_style))
    min_max_size = child.ComputeMinMaxSize();

  Optional<LayoutUnit> max_inline_size;
  if (!child_style.LogicalMaxWidth().IsMaxSizeNone()) {
    max_inline_size = ResolveInlineLength(
        *child_space, child_style, min_max_size, child_style.LogicalMaxWidth(),
        LengthResolveType::kMaxSize);
  }
  Optional<LayoutUnit> min_inline_size = ResolveInlineLength(
      *child_space, child_style, min_max_size, child_style.LogicalMinWidth(),
      LengthResolveType::kMinSize);

  // Adjust the area we search for layout opportunities by the child's margins.
  LayoutUnit child_available_inline_size =
      std::max(LayoutUnit(), child_available_size_.inline_size -
                                 child_data.margins.InlineSum());
  NGLogicalSize child_available_size(child_available_inline_size,
                                     LayoutUnit(-1));

  // If we have should have a fixed inline size, we find a layout opportunity
  // before layout, then fixed our child to that size.
  WTF::Optional<NGLayoutOpportunity> opportunity;
  WTF::Optional<LayoutUnit> fixed_inline_size;
  if (is_fixed_inline_size) {
    // TODO(ikilpatrick): This actually needs to take into account the border
    // and padding of the child for the minimum layout opportunity size.
    // TODO(ikilpatrick): min_inline_size should be applied before finding the
    // layout opportunity.
    // TODO(ikilpatrick): During the second layout pass of new formatting
    // contexts, we actually need to find the first "open" layout opportunity.
    // TODO(ikilpatrick): Investigate tables 'auto' size as this is subtly
    // different to normal 'auto' sizing behaviour.
    opportunity = tmp_exclusion_space.FindLayoutOpportunity(
        origin_offset, child_available_size.inline_size, NGLogicalSize());
    fixed_inline_size = ConstrainByMinMax(opportunity->rect.InlineSize(),
                                          min_inline_size, max_inline_size);
  }

  child_space = CreateConstraintSpaceForChild(child, child_data, WTF::nullopt,
                                              fixed_inline_size);
  scoped_refptr<NGLayoutResult> layout_result =
      child.Layout(*child_space, child_break_token);
  DCHECK(layout_result->PhysicalFragment());

  // If we didn't have a fixed inline size, we now search for the layout
  // opportunity we fit into.
  if (!opportunity) {
    NGFragment fragment(ConstraintSpace().GetWritingMode(),
                        *layout_result->PhysicalFragment());

    // TODO(ikilpatrick): child_available_size is probably wrong as the area we
    // need to search shrinks by the origin_offset and LineRight margin.
    opportunity = tmp_exclusion_space.FindLayoutOpportunity(
        origin_offset, child_available_size.inline_size,
        NGLogicalSize{fragment.InlineSize(), fragment.BlockSize()});
  }

  return std::make_pair(std::move(layout_result), opportunity.value());
}

bool NGBlockLayoutAlgorithm::HandleInflow(
    NGLayoutInputNode child,
    NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position,
    scoped_refptr<NGBreakToken>* previous_inline_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(!child.CreatesNewFormattingContext());

  bool is_non_empty_inline =
      child.IsInline() && !ToNGInlineNode(child).IsEmptyInline();

  // TODO(ikilpatrick): We may only want to position pending floats if there is
  // something that we *might* clear in the unpositioned list. E.g. we may
  // clear an already placed left float, but the unpositioned list may only have
  // right floats.
  bool should_position_pending_floats =
      child.IsBlock() &&
      ClearanceMayAffectLayout(*exclusion_space_, unpositioned_floats_,
                               child.Style());

  // There are two conditions where we need to position our pending floats:
  //  1. If the child will be affected by clearance.
  //  2. If the child is a non-empty inline.
  // This collapses the previous margin strut, and optionally resolves *our*
  // BFC offset.
  if (should_position_pending_floats || is_non_empty_inline) {
    LayoutUnit origin_point_block_offset =
        previous_inflow_position->bfc_block_offset +
        previous_inflow_position->margin_strut.Sum();
    bool updated = MaybeUpdateFragmentBfcOffset(origin_point_block_offset);

    if (updated && abort_when_bfc_resolved_)
      return false;

    // If our BFC offset was updated we may have been affected by clearance
    // ourselves. We need to adjust the origin point to accomodate this.
    if (updated)
      origin_point_block_offset =
          std::max(origin_point_block_offset,
                   ConstraintSpace().ClearanceOffset().value_or(LayoutUnit()));

    bool positioned_direct_child_floats = !unpositioned_floats_.IsEmpty();

    PositionPendingFloats(origin_point_block_offset);

    // If we positioned float which are direct children, or the child is a
    // non-empty inline, we need to artificially "reset" the previous inflow
    // position, e.g. we clear the margin strut, and set the offset to our
    // block-start border edge.
    //
    // This behaviour is similar to if we had block-start border or padding.
    if ((positioned_direct_child_floats && updated) || is_non_empty_inline) {
      // We must have no border/scrollbar/padding here otherwise our BFC offset
      // would already be resolved.
      if (!is_non_empty_inline)
        DCHECK_EQ(border_scrollbar_padding_.block_start, LayoutUnit());

      previous_inflow_position->bfc_block_offset = origin_point_block_offset;
      previous_inflow_position->margin_strut = NGMarginStrut();
      previous_inflow_position->logical_block_offset = LayoutUnit();
    }
  }

  // Perform layout on the child.
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token);
  scoped_refptr<NGConstraintSpace> child_space =
      CreateConstraintSpaceForChild(child, child_data);
  scoped_refptr<NGLayoutResult> layout_result =
      child.Layout(*child_space, child_break_token);

  bool is_empty_block = IsEmptyBlock(child, *layout_result);

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
  if (!container_builder_.BfcOffset()) {
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
    MaybeUpdateFragmentBfcOffset(
        layout_result->BfcOffset().value().block_offset);

    // NOTE: Unlike other aborts, we don't try check if we *should* abort with
    // abort_when_bfc_resolved_, this is simply propagating an abort up to a
    // node which is able to restart the layout (a node that has resolved its
    // BFC offset).
    return false;
  }

  // We only want to copy from a layout that was successful. If the status was
  // kBfcOffsetResolved we may have unpositioned floats which we will position
  // in the current exclusion space once *our* BFC is resolved.
  //
  // The exclusion space is then updated when the child undergoes relayout
  // below.
  if (layout_result->Status() == NGLayoutResult::kSuccess) {
    DCHECK(layout_result->ExclusionSpace());
    exclusion_space_ =
        std::make_unique<NGExclusionSpace>(*layout_result->ExclusionSpace());
  }

  // A line-box may have a list of floats which we add as children.
  if (child.IsInline() &&
      (container_builder_.BfcOffset() || ConstraintSpace().FloatsBfcOffset())) {
    AddPositionedFloats(layout_result->PositionedFloats());
  }

  // We have special behaviour for an empty block which gets pushed down due to
  // clearance, see comment inside ComputeInflowPosition.
  bool empty_block_affected_by_clearance = false;

  // We try and position the child within the block formatting context. This
  // may cause our BFC offset to be resolved, in which case we should abort our
  // layout if needed.
  WTF::Optional<NGBfcOffset> child_bfc_offset;
  if (layout_result->BfcOffset()) {
    if (!PositionWithBfcOffset(layout_result->BfcOffset().value(),
                               &child_bfc_offset))
      return false;
  } else if (container_builder_.BfcOffset()) {
    child_bfc_offset =
        PositionWithParentBfc(child, *child_space, child_data, *layout_result,
                              &empty_block_affected_by_clearance);
  } else
    DCHECK(is_empty_block);

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
    scoped_refptr<NGConstraintSpace> new_child_space =
        CreateConstraintSpaceForChild(child, child_data, child_bfc_offset);
    layout_result = child.Layout(*new_child_space, child_break_token);

    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);

    DCHECK(layout_result->ExclusionSpace());
    exclusion_space_ =
        std::make_unique<NGExclusionSpace>(*layout_result->ExclusionSpace());
  }

  // We must have an actual fragment at this stage.
  DCHECK(layout_result->PhysicalFragment());
  const auto& physical_fragment = *layout_result->PhysicalFragment();
  NGFragment fragment(ConstraintSpace().GetWritingMode(), physical_fragment);

  NGLogicalOffset logical_offset =
      CalculateLogicalOffset(fragment, child_data.margins, child_bfc_offset);

  if (ConstraintSpace().HasBlockFragmentation()) {
    if (BreakBeforeChild(child, *layout_result, logical_offset.block_offset,
                         layout_result->IsPushedByFloats()))
      return true;
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result->FinalBreakAfter(), child.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);
  }

  // Only modify intrinsic_block_size_ if the fragment is non-empty block.
  //
  // Empty blocks don't immediately contribute to our size, instead we wait to
  // see what the final margin produced, e.g.
  // <div style="display: flow-root">
  //   <div style="margin-top: -8px"></div>
  //   <div style="margin-top: 10px"></div>
  // </div>
  if (!is_empty_block) {
    DCHECK(container_builder_.BfcOffset());
    intrinsic_block_size_ =
        std::max(intrinsic_block_size_,
                 logical_offset.block_offset + fragment.BlockSize());
  }

  container_builder_.AddChild(layout_result, logical_offset);
  if (child.IsBlock())
    container_builder_.PropagateBreak(layout_result);

  *previous_inflow_position =
      ComputeInflowPosition(*previous_inflow_position, child, child_data,
                            child_bfc_offset, logical_offset, *layout_result,
                            fragment, empty_block_affected_by_clearance);

  *previous_inline_break_token =
      child.IsInline() ? layout_result->PhysicalFragment()->BreakToken()
                       : nullptr;

  return true;
}

NGInflowChildData NGBlockLayoutAlgorithm::ComputeChildData(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());

  // Calculate margins in parent's writing mode.
  NGBoxStrut margins = CalculateMargins(child, child_break_token);

  // Append the current margin strut with child's block start margin.
  // Non empty border/padding, and new FC use cases are handled inside of the
  // child's layout
  NGMarginStrut margin_strut = previous_inflow_position.margin_strut;
  margin_strut.Append(margins.block_start,
                      child.Style().HasMarginBeforeQuirk());

  NGBfcOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().line_offset +
          border_scrollbar_padding_.LineLeft(ConstraintSpace().Direction()) +
          margins.LineLeft(ConstraintSpace().Direction()),
      previous_inflow_position.bfc_block_offset};

  return {child_bfc_offset, margin_strut, margins};
}

NGPreviousInflowPosition NGBlockLayoutAlgorithm::ComputeInflowPosition(
    const NGPreviousInflowPosition& previous_inflow_position,
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const WTF::Optional<NGBfcOffset>& child_bfc_offset,
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

bool NGBlockLayoutAlgorithm::PositionWithBfcOffset(
    const NGBfcOffset& bfc_offset,
    WTF::Optional<NGBfcOffset>* child_bfc_offset) {
  LayoutUnit bfc_block_offset = bfc_offset.block_offset;
  bool updated = MaybeUpdateFragmentBfcOffset(bfc_block_offset);

  if (updated && abort_when_bfc_resolved_)
    return false;

  PositionPendingFloats(bfc_block_offset);

  *child_bfc_offset = bfc_offset;
  return true;
}

NGBfcOffset NGBlockLayoutAlgorithm::PositionWithParentBfc(
    const NGLayoutInputNode& child,
    const NGConstraintSpace& space,
    const NGInflowChildData& child_data,
    const NGLayoutResult& layout_result,
    bool* empty_block_affected_by_clearance) {
  DCHECK(IsEmptyBlock(child, layout_result));

  // The child must be an in-flow zero-block-size fragment, use its end margin
  // strut for positioning.
  NGBfcOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().line_offset +
          border_scrollbar_padding_.LineLeft(ConstraintSpace().Direction()) +
          child_data.margins.LineLeft(ConstraintSpace().Direction()),
      child_data.bfc_offset_estimate.block_offset +
          layout_result.EndMarginStrut().Sum()};

  *empty_block_affected_by_clearance =
      AdjustToClearance(space.ClearanceOffset(), &child_bfc_offset);

  return child_bfc_offset;
}

LayoutUnit NGBlockLayoutAlgorithm::FragmentainerSpaceAvailable() const {
  DCHECK(container_builder_.BfcOffset().has_value());
  return ConstraintSpace().FragmentainerSpaceAtBfcStart() -
         container_builder_.BfcOffset()->block_offset;
}

bool NGBlockLayoutAlgorithm::IsFragmentainerOutOfSpace() const {
  if (!ConstraintSpace().HasBlockFragmentation())
    return false;
  if (!container_builder_.BfcOffset().has_value())
    return false;
  return intrinsic_block_size_ >= FragmentainerSpaceAvailable();
}

void NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  if (first_overflowing_line_ && !fit_all_lines_) {
    // A line box overflowed the fragmentainer, but we continued layout anyway,
    // in order to determine where to break in order to honor the widows
    // request. We never got around to actually breaking, before we ran out of
    // lines. So do it now.
    intrinsic_block_size_ = FragmentainerSpaceAvailable();
    container_builder_.SetDidBreak();
  }

  LayoutUnit used_block_size =
      BreakToken() ? BreakToken()->UsedBlockSize() : LayoutUnit();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), used_block_size + intrinsic_block_size_);

  block_size -= used_block_size;
  DCHECK_GE(block_size, LayoutUnit())
      << "Adding and subtracting the used_block_size shouldn't leave the "
         "block_size for this fragment smaller than zero.";

  LayoutUnit space_left = FragmentainerSpaceAvailable();

  if (space_left <= LayoutUnit()) {
    // The amount of space available may be zero, or even negative, if the
    // border-start edge of this block starts exactly at, or even after the
    // fragmentainer boundary. We're going to need a break before this block,
    // because no part of it fits in the current fragmentainer. Due to margin
    // collapsing with children, this situation is something that we cannot
    // always detect prior to layout. The fragment produced by this algorithm is
    // going to be thrown away. The parent layout algorithm will eventually
    // detect that there's no room for a fragment for this node, and drop the
    // fragment on the floor. Therefore it doesn't matter how we set up the
    // container builder, so just return.
    return;
  }

  if (container_builder_.DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space we
    // need to prepare a break token.
    container_builder_.SetUsedBlockSize(std::min(space_left, block_size) +
                                        used_block_size);
    container_builder_.SetBlockSize(std::min(space_left, block_size));
    container_builder_.SetIntrinsicBlockSize(space_left);

    if (first_overflowing_line_) {
      int line_number;
      if (fit_all_lines_) {
        line_number = first_overflowing_line_;
      } else {
        // We managed to finish layout of all the lines for the node, which
        // means that we won't have enough widows, unless we break earlier than
        // where we overflowed.
        int line_count = container_builder_.LineCount();
        line_number = std::max(line_count - Style().Widows(),
                               std::min(line_count, int(Style().Orphans())));
      }
      container_builder_.AddBreakBeforeLine(line_number);
    }
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    container_builder_.SetUsedBlockSize(space_left + used_block_size);
    container_builder_.SetDidBreak();
    container_builder_.SetBlockSize(space_left);
    container_builder_.SetIntrinsicBlockSize(space_left);
    container_builder_.PropagateSpaceShortage(block_size - space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  container_builder_.SetUsedBlockSize(used_block_size + block_size);
  container_builder_.SetBlockSize(block_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
}

bool NGBlockLayoutAlgorithm::BreakBeforeChild(
    NGLayoutInputNode child,
    const NGLayoutResult& layout_result,
    LayoutUnit block_offset,
    bool is_pushed_by_floats) {
  DCHECK(ConstraintSpace().HasBlockFragmentation());
  BreakType break_type = BreakTypeBeforeChild(
      child, layout_result, block_offset, is_pushed_by_floats);
  if (break_type == NoBreak)
    return false;

  LayoutUnit space_available = FragmentainerSpaceAvailable();
  LayoutUnit space_shortage;
  if (layout_result.MinimalSpaceShortage() == LayoutUnit::Max()) {
    // Calculate space shortage: Figure out how much more space would have been
    // sufficient to make the child fit right here in the current fragment.
    NGFragment fragment(ConstraintSpace().GetWritingMode(),
                        *layout_result.PhysicalFragment());
    LayoutUnit space_left = space_available - block_offset;
    space_shortage = fragment.BlockSize() - space_left;
  } else {
    // However, if space shortage was reported inside the child, use that. If we
    // broke inside the child, we didn't complete layout, so calculating space
    // shortage for the child as a whole would be impossible and pointless.
    space_shortage = layout_result.MinimalSpaceShortage();
  }

  if (child.IsInline()) {
    DCHECK_EQ(break_type, SoftBreak);
    if (!first_overflowing_line_) {
      // We're at the first overflowing line. This is the space shortage that
      // we are going to report. We do this in spite of not yet knowing
      // whether breaking here would violate orphans and widows requests. This
      // approach may result in a lower space shortage than what's actually
      // true, which leads to more layout passes than we'd otherwise
      // need. However, getting this optimal for orphans and widows would
      // require an additional piece of machinery. This case should be rare
      // enough (to worry about performance), so let's focus on code
      // simplicity instead.
      container_builder_.PropagateSpaceShortage(space_shortage);
    }
    // Attempt to honor orphans and widows requests.
    if (int line_count = container_builder_.LineCount()) {
      if (!first_overflowing_line_)
        first_overflowing_line_ = line_count;
      bool is_first_fragment = !BreakToken();
      // Figure out how many lines we need before the break. That entails to
      // attempt to honor the orphans request.
      int minimum_line_count = Style().Orphans();
      if (!is_first_fragment) {
        // If this isn't the first fragment, it means that there's a break both
        // before and after this fragment. So what was seen as trailing widows
        // in the previous fragment is essentially orphans for us now.
        minimum_line_count =
            std::max(minimum_line_count, static_cast<int>(Style().Widows()));
      }
      if (line_count < minimum_line_count) {
        if (is_first_fragment) {
          // Not enough orphans. Our only hope is if we can break before the
          // start of this block to improve on the situation. That's not
          // something we can determine at this point though. Permit the break,
          // but mark it as undesirable.
          container_builder_.SetHasLastResortBreak();
        }
        // We're already failing with orphans, so don't even try to deal with
        // widows.
        fit_all_lines_ = true;
      } else {
        // There are enough lines before the break. Try to make sure that
        // there'll be enough lines after the break as well. Attempt to honor
        // the widows request.
        DCHECK_GE(line_count, first_overflowing_line_);
        int widows_found = line_count - first_overflowing_line_ + 1;
        if (widows_found < Style().Widows()) {
          // Although we're out of space, we have to continue layout to figure
          // out exactly where to break in order to honor the widows
          // request. We'll make sure that we're going to leave at least as many
          // lines as specified by the 'widows' property for the next fragment
          // (if at all possible), which means that lines that could fit in the
          // current fragment (that we have already laid out) may have to be
          // saved for the next fragment.
          return false;
        } else {
          // We have determined that there are plenty of lines for the next
          // fragment, so we can just break exactly where we ran out of space,
          // rather than pushing some of the line boxes over to the next
          // fragment.
          fit_all_lines_ = true;
        }
      }
    }
  }

  if (!has_processed_first_child_ && !is_pushed_by_floats) {
    // We're breaking before the first piece of in-flow content inside this
    // block, even if it's not a valid class C break point [1] in this case. We
    // really don't want to break here, if we can find something better.
    //
    // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
    container_builder_.SetHasLastResortBreak();
  }

  // The remaining part of the fragmentainer (the unusable space for child
  // content, due to the break) should still be occupied by this container.
  // TODO(mstensho): Figure out if we really need to <0 here. It doesn't seem
  // right to have negative available space.
  intrinsic_block_size_ = space_available.ClampNegativeToZero();
  // Drop the fragment on the floor and retry at the start of the next
  // fragmentainer.
  container_builder_.AddBreakBeforeChild(child);
  container_builder_.SetDidBreak();
  if (break_type == ForcedBreak) {
    container_builder_.SetHasForcedBreak();
  } else {
    // Report space shortage, unless we're at a line box (in that case we've
    // already dealt with it further up).
    if (!child.IsInline()) {
      // TODO(mstensho): Turn this into a DCHECK, when the engine is ready for
      // it. Space shortage should really be positive here, or we might
      // ultimately fail to stretch the columns (column balancing).
      if (space_shortage > LayoutUnit())
        container_builder_.PropagateSpaceShortage(space_shortage);
    }
  }
  return true;
}

NGBlockLayoutAlgorithm::BreakType NGBlockLayoutAlgorithm::BreakTypeBeforeChild(
    NGLayoutInputNode child,
    const NGLayoutResult& layout_result,
    LayoutUnit block_offset,
    bool is_pushed_by_floats) const {
  if (!container_builder_.BfcOffset().has_value())
    return NoBreak;

  const NGPhysicalFragment& physical_fragment =
      *layout_result.PhysicalFragment();

  // If we haven't used any space at all in the fragmentainer yet, we cannot
  // break, or there'd be no progress. We'd end up creating an infinite number
  // of fragmentainers without putting any content into them.
  auto space_left = FragmentainerSpaceAvailable() - block_offset;
  if (space_left >= ConstraintSpace().FragmentainerBlockSize())
    return NoBreak;

  if (child.IsInline()) {
    NGFragment fragment(ConstraintSpace().GetWritingMode(), physical_fragment);
    return fragment.BlockSize() > space_left ? SoftBreak : NoBreak;
  }

  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child.Style().BreakBefore(), layout_result.InitialBreakBefore());
  EBreakBetween break_between =
      container_builder_.JoinedBreakBetweenValue(break_before);
  if (IsForcedBreakValue(ConstraintSpace(), break_between)) {
    // There should be a forced break before this child, and if we're not at the
    // first in-flow child, just go ahead and break.
    if (has_processed_first_child_)
      return ForcedBreak;
  }

  // If the block offset is past the fragmentainer boundary (or exactly at the
  // boundary), no part of the fragment is going to fit in the current
  // fragmentainer. Fragments may be pushed past the fragmentainer boundary by
  // margins.
  if (space_left <= LayoutUnit())
    return SoftBreak;

  const auto* token = physical_fragment.BreakToken();
  if (!token || token->IsFinished())
    return NoBreak;
  if (token && token->IsBlockType() &&
      ToNGBlockBreakToken(token)->HasLastResortBreak()) {
    // We've already found a place to break inside the child, but it wasn't an
    // optimal one, because it would violate some rules for breaking. Consider
    // breaking before this child instead, but only do so if it's at a valid
    // break point. It's a valid break point if we're between siblings, or if
    // it's a first child at a class C break point [1] (if it got pushed down by
    // floats). The break we've already found has been marked as a last-resort
    // break, but moving that last-resort break to an earlier (but equally bad)
    // last-resort break would just waste fragmentainer space and slow down
    // content progression.
    //
    // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
    if (has_processed_first_child_ || is_pushed_by_floats) {
      // This is a valid break point, and we can resolve the last-resort
      // situation.
      return SoftBreak;
    }
  }
  // TODO(mstensho): There are other break-inside values to consider here.
  if (child.Style().BreakInside() != EBreakInside::kAvoid)
    return NoBreak;

  // The child broke, and we're not at the start of a fragmentainer, and we're
  // supposed to avoid breaking inside the child.
  DCHECK(IsFirstFragment(ConstraintSpace(), physical_fragment));
  return SoftBreak;
}

NGBoxStrut NGBlockLayoutAlgorithm::CalculateMargins(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token) {
  DCHECK(child);
  if (child.IsInline())
    return {};
  const ComputedStyle& child_style = child.Style();

  scoped_refptr<NGConstraintSpace> space =
      NGConstraintSpaceBuilder(ConstraintSpace())
          .SetAvailableSize(child_available_size_)
          .SetPercentageResolutionSize(child_percentage_size_)
          .ToConstraintSpace(child_style.GetWritingMode());

  NGBoxStrut margins =
      ComputeMarginsFor(*space, child_style, ConstraintSpace());
  if (ShouldIgnoreBlockStartMargin(ConstraintSpace(), child, child_break_token))
    margins.block_start = LayoutUnit();

  // TODO(ikilpatrick): Move the auto margins calculation for different writing
  // modes to post-layout.
  if (!child.IsFloating() && !child.CreatesNewFormattingContext()) {
    WTF::Optional<MinMaxSize> sizes;
    if (NeedMinMaxSize(*space, child_style))
      sizes = child.ComputeMinMaxSize();

    LayoutUnit child_inline_size =
        ComputeInlineSizeForFragment(*space, child_style, sizes);

    // TODO(ikilpatrick): ApplyAutoMargins looks wrong as its not respecting
    // the parents writing mode?
    ApplyAutoMargins(child_style, Style(), space->AvailableSize().inline_size,
                     child_inline_size, &margins);
  }
  return margins;
}

scoped_refptr<NGConstraintSpace>
NGBlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const WTF::Optional<NGBfcOffset> floats_bfc_offset,
    const WTF::Optional<LayoutUnit> fixed_inline_size) {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());

  NGLogicalSize available_size(child_available_size_);
  NGLogicalSize percentage_size(child_percentage_size_);
  if (fixed_inline_size) {
    space_builder.SetIsFixedSizeInline(true);
    available_size.inline_size = fixed_inline_size.value();
    percentage_size.inline_size = fixed_inline_size.value();
  }
  space_builder.SetAvailableSize(available_size)
      .SetPercentageResolutionSize(percentage_size);

  if (NGBaseline::ShouldPropagateBaselines(child))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  bool is_new_fc = child.CreatesNewFormattingContext();
  space_builder.SetIsNewFormattingContext(is_new_fc)
      .SetBfcOffset(child_data.bfc_offset_estimate)
      .SetMarginStrut(child_data.margin_strut);

  if (!is_new_fc)
    space_builder.SetExclusionSpace(*exclusion_space_);

  if (!container_builder_.BfcOffset() && ConstraintSpace().FloatsBfcOffset()) {
    space_builder.SetFloatsBfcOffset(
        NGBfcOffset{child_data.bfc_offset_estimate.line_offset,
                    ConstraintSpace().FloatsBfcOffset()->block_offset});
  }

  if (floats_bfc_offset)
    space_builder.SetFloatsBfcOffset(floats_bfc_offset);

  if (!is_new_fc && !floats_bfc_offset) {
    space_builder.SetUnpositionedFloats(unpositioned_floats_);
  }

  WritingMode writing_mode;
  if (child.IsInline()) {
    space_builder.SetClearanceOffset(ConstraintSpace().ClearanceOffset());
    writing_mode = Style().GetWritingMode();
  } else {
    const ComputedStyle& child_style = child.Style();
    space_builder
        .SetClearanceOffset(
            exclusion_space_->ClearanceOffset(child_style.Clear()))
        .SetIsShrinkToFit(ShouldShrinkToFit(Style(), child_style))
        .SetTextDirection(child_style.Direction());
    writing_mode = child_style.GetWritingMode();
  }

  LayoutUnit space_available;
  if (ConstraintSpace().HasBlockFragmentation()) {
    space_available = ConstraintSpace().FragmentainerSpaceAtBfcStart();
    // If a block establishes a new formatting context we must know our
    // position in the formatting context, and are able to adjust the
    // fragmentation line.
    if (is_new_fc) {
      space_available -= child_data.bfc_offset_estimate.block_offset;
    }
    // The policy regarding collapsing block-start margin with the fragmentainer
    // block-start is the same throughout the entire fragmentainer (although it
    // really only matters at the beginning of each fragmentainer, we don't need
    // to bother to check whether we're actually at the start).
    space_builder.SetSeparateLeadingFragmentainerMargins(
        ConstraintSpace().HasSeparateLeadingFragmentainerMargins());
  }
  space_builder.SetFragmentainerBlockSize(
      ConstraintSpace().FragmentainerBlockSize());
  space_builder.SetFragmentainerSpaceAtBfcStart(space_available);
  space_builder.SetFragmentationType(
      ConstraintSpace().BlockFragmentationType());

  return space_builder.ToConstraintSpace(writing_mode);
}

// Add a baseline from a child box fragment.
// @return false if the specified child is not a box or is OOF.
bool NGBlockLayoutAlgorithm::AddBaseline(const NGBaselineRequest& request,
                                         const NGPhysicalFragment* child,
                                         LayoutUnit child_offset) {
  if (child->IsLineBox()) {
    const NGPhysicalLineBoxFragment* line_box =
        ToNGPhysicalLineBoxFragment(child);

    // Skip over a line-box which is empty. These don't any baselines which
    // should be added.
    if (line_box->Children().IsEmpty())
      return false;

    LayoutUnit offset = line_box->BaselinePosition(request.baseline_type);
    container_builder_.AddBaseline(request, offset + child_offset);
    return true;
  }

  LayoutObject* layout_object = child->GetLayoutObject();
  if (layout_object->IsFloatingOrOutOfFlowPositioned())
    return false;

  if (child->IsBox()) {
    const NGPhysicalBoxFragment* box = ToNGPhysicalBoxFragment(child);
    if (const NGBaseline* baseline = box->Baseline(request)) {
      container_builder_.AddBaseline(request, baseline->offset + child_offset);
      return true;
    }
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
        for (unsigned i = container_builder_.Children().size(); i--;) {
          if (AddBaseline(request, container_builder_.Children()[i].get(),
                          container_builder_.Offsets()[i].block_offset))
            break;
        }
        break;
      case NGBaselineAlgorithmType::kFirstLine:
        for (unsigned i = 0; i < container_builder_.Children().size(); i++) {
          if (AddBaseline(request, container_builder_.Children()[i].get(),
                          container_builder_.Offsets()[i].block_offset))
            break;
        }
        break;
    }
  }
}

bool NGBlockLayoutAlgorithm::MaybeUpdateFragmentBfcOffset(
    LayoutUnit bfc_block_offset) {
  if (container_builder_.BfcOffset())
    return false;

  NGBfcOffset bfc_offset(ConstraintSpace().BfcOffset().line_offset,
                         bfc_block_offset);
  if (AdjustToClearance(ConstraintSpace().ClearanceOffset(), &bfc_offset))
    container_builder_.SetIsPushedByFloats();
  container_builder_.SetBfcOffset(bfc_offset);

  return true;
}

void NGBlockLayoutAlgorithm::PositionPendingFloats(
    LayoutUnit origin_block_offset) {
  DCHECK(container_builder_.BfcOffset() || ConstraintSpace().FloatsBfcOffset())
      << "The parent BFC offset should be known here";

  NGBfcOffset bfc_offset = container_builder_.BfcOffset()
                               ? container_builder_.BfcOffset().value()
                               : ConstraintSpace().FloatsBfcOffset().value();

  LayoutUnit from_block_offset = bfc_offset.block_offset;

  const auto positioned_floats = PositionFloats(
      origin_block_offset, from_block_offset, unpositioned_floats_,
      ConstraintSpace(), exclusion_space_.get());

  AddPositionedFloats(positioned_floats);

  unpositioned_floats_.clear();
}

void NGBlockLayoutAlgorithm::AddPositionedFloats(
    const Vector<NGPositionedFloat>& positioned_floats) {
  DCHECK(container_builder_.BfcOffset() || ConstraintSpace().FloatsBfcOffset())
      << "The parent BFC offset should be known here";

  NGBfcOffset bfc_offset = container_builder_.BfcOffset()
                               ? container_builder_.BfcOffset().value()
                               : ConstraintSpace().FloatsBfcOffset().value();

  // TODO(ikilpatrick): Add DCHECK that any positioned floats are children.
  for (const auto& positioned_float : positioned_floats) {
    NGFragment child_fragment(
        ConstraintSpace().GetWritingMode(),
        *positioned_float.layout_result->PhysicalFragment());

    NGLogicalOffset logical_offset = LogicalFromBfcOffsets(
        child_fragment, positioned_float.bfc_offset, bfc_offset,
        container_builder_.Size().inline_size, ConstraintSpace().Direction());

    container_builder_.AddChild(positioned_float.layout_result, logical_offset);
    container_builder_.PropagateBreak(positioned_float.layout_result);
  }
}

}  // namespace blink
