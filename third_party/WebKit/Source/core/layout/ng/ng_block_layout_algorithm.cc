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
#include "core/layout/ng/list/ng_list_layout_algorithm.h"
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
  DCHECK_EQ(LayoutUnit(), fragment.BlockSize()) << child.ToString();
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

Optional<MinMaxSize> NGBlockLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  MinMaxSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().ContainsSize())
    return sizes;

  const TextDirection direction = Style().Direction();
  LayoutUnit float_left_inline_size = input.float_left_inline_size;
  LayoutUnit float_right_inline_size = input.float_right_inline_size;

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

    MinMaxSizeInput child_input;
    if (!child.CreatesNewFormattingContext())
      child_input = {float_left_inline_size, float_right_inline_size};

    MinMaxSize child_sizes;
    if (child.IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |child| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes = child.ComputeMinMaxSize(child_input);
    } else {
      Optional<MinMaxSize> child_minmax;
      if (NeedMinMaxSizeForContentContribution(child_style))
        child_minmax = child.ComputeMinMaxSize(child_input);

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
    NGLayoutInputNode child,
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

  if (child.IsInline()) {
    LayoutUnit offset =
        LineOffsetForTextAlign(Style().GetTextAlign(), Style().Direction(),
                               child_available_size_.inline_size);
    if (IsRtl(Style().Direction()))
      offset = child_available_size_.inline_size - offset;
    inline_offset += offset;
  }

  // If we've reached here, both the child and the current layout don't have a
  // BFC offset yet. Children in this situation are always placed at a logical
  // block offset of 0.
  DCHECK(!container_builder_.BfcOffset());
  return {inline_offset, LayoutUnit()};
}

scoped_refptr<NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  WTF::Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(ConstraintSpace(), Style())) {
    MinMaxSizeInput zero_input;
    min_max_size = ComputeMinMaxSize(zero_input);
  }

  border_scrollbar_padding_ =
      CalculateBorderScrollbarPadding(ConstraintSpace(), Style(), Node());

  NGLogicalSize size = CalculateBorderBoxSize(
      ConstraintSpace(), Style(), min_max_size, CalculateDefaultBlockSize());

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
    } else if (child.IsListMarker()) {
      container_builder_.SetUnpositionedListMarker(ToNGBlockNode(child));
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

  // There are still a couple of opportunities to find something solid for this
  // block to hang on to, if we haven't already been able to do so. Keep track
  // of this, so that we can abort layout if necessary.
  bool bfc_updated = false;

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

    if (!container_builder_.BfcOffset()) {
      // If we have collapsed through the block start and all children (if any),
      // now is the time to determine the BFC offset, because finally we have
      // found something solid to hang on to (like clearance or a bottom border,
      // for instance). If we're a new formatting context, though, we shouldn't
      // be here, because then the offset should already have been determined.
      DCHECK(!ConstraintSpace().IsNewFormattingContext());
      bfc_updated = MaybeUpdateFragmentBfcOffset(end_bfc_block_offset);
      DCHECK(bfc_updated);
    } else {
      // The trailing margin strut will be part of our intrinsic block size, but
      // only if there is something that separates the end margin strut from the
      // input margin strut (typically child content, block start
      // border/padding, or this being a new BFC). If the margin strut from a
      // previous sibling or ancestor managed to collapse through all our
      // children (if any at all, that is), it means that the resulting end
      // margin strut actually pushes us down, and it should obviously not be
      // doubly accounted for as our block size.
      intrinsic_block_size_ = std::max(
          intrinsic_block_size_,
          previous_inflow_position.logical_block_offset + margin_strut_sum);
    }

    intrinsic_block_size_ += border_scrollbar_padding_.block_end;
    end_margin_strut = NGMarginStrut();
  }

  // Recompute the block-axis size now that we know our content size.
  intrinsic_block_size_ = std::max(intrinsic_block_size_,
                                   CalculateMinimumBlockSize(end_margin_strut));
  size.block_size = ComputeBlockSizeForFragment(ConstraintSpace(), Style(),
                                                intrinsic_block_size_);
  container_builder_.SetBlockSize(size.block_size);

  // If our BFC offset is still unknown, there's one last thing to take into
  // consideration: Non-empty blocks always know their position in space. If we
  // have a break token, it means that we know the blocks' position even if
  // they're empty; it will be at the very start of the fragmentainer.
  if (!container_builder_.BfcOffset() && (size.block_size || BreakToken())) {
    end_bfc_block_offset += end_margin_strut.Sum();
    bfc_updated = MaybeUpdateFragmentBfcOffset(end_bfc_block_offset);
    DCHECK(bfc_updated);
  }

  if (bfc_updated && abort_when_bfc_resolved_) {
    // New formatting contexts, and where we have an empty block affected by
    // clearance should already have their BFC offset resolved, and shouldn't
    // enter this branch.
    DCHECK(!previous_inflow_position.empty_block_affected_by_clearance);
    DCHECK(!ConstraintSpace().IsNewFormattingContext());

    container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
    return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
  }

  if (container_builder_.BfcOffset()) {
    PositionPendingFloats(end_bfc_block_offset);

    // Do not collapse margins between the last in-flow child and bottom margin
    // of its parent if the parent has height != auto.
    if (!Style().LogicalHeight().IsAuto()) {
      // TODO(layout-ng): handle LogicalMinHeight, LogicalMaxHeight.
      end_margin_strut = NGMarginStrut();
    }
  }

  container_builder_.SetEndMarginStrut(end_margin_strut);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.SetPadding(ComputePadding(ConstraintSpace(), Style()));

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
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token);

  NGMarginStrut adjoining_margin_strut(previous_inflow_position->margin_strut);
  adjoining_margin_strut.Append(child_data.margins.block_start,
                                child_style.HasMarginBeforeQuirk());

  LayoutUnit initial_child_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset +
      adjoining_margin_strut.Sum();
  LayoutUnit child_bfc_offset_estimate = initial_child_bfc_offset_estimate;

  // If the child has a non-zero block-start margin, our initial estimate will
  // be that any pending floats will be flush (block-start-wise) with this
  // child, since they are affected by margin collapsing. Furthermore, this
  // child's margin may also pull parent blocks downwards. However, this is only
  // the case if the child fits beside the floats at the current block
  // offset. If it doesn't (or if it gets clearance), the child needs to be
  // pushed down. In this case, the child's margin no longer collapses with the
  // previous margin strut, so the pending floats and parent blocks need to
  // ignore this margin, which may cause them to end up at completely different
  // positions than initially estimated. In other words, we'll need another
  // layout pass if this happens.
  bool abort_if_cleared = child_data.margins.block_start != LayoutUnit();
  NGLayoutOpportunity opportunity;
  scoped_refptr<NGLayoutResult> layout_result;
  std::tie(layout_result, opportunity) =
      LayoutNewFormattingContext(child, child_break_token, child_data,
                                 child_bfc_offset_estimate, abort_if_cleared);

  if (!layout_result) {
    DCHECK(abort_if_cleared);
    // Layout got aborted, because the child got pushed down by floats, and we
    // had pending floats that we tentatively positioned incorrectly, due to a
    // margin that shouldn't have affected them. Try again without the child's
    // margin. This re-layout *must* produce a fragment and opportunity which
    // fits within the exclusion space.
    DCHECK_GT(opportunity.rect.start_offset.block_offset,
              child_bfc_offset_estimate);
    NGMarginStrut non_adjoining_margin_strut(
        previous_inflow_position->margin_strut);
    child_bfc_offset_estimate = child_data.bfc_offset_estimate.block_offset +
                                non_adjoining_margin_strut.Sum();

    // Make sure that we don't move below the previously found layout
    // opportunity. This could otherwise happen if the child has negative
    // margins.
    child_bfc_offset_estimate = std::min(
        child_bfc_offset_estimate, opportunity.rect.start_offset.block_offset);

    std::tie(layout_result, opportunity) = LayoutNewFormattingContext(
        child, child_break_token, child_data, child_bfc_offset_estimate,
        /* abort_if_cleared */ false);
  }
  DCHECK(layout_result->PhysicalFragment());

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

  PositionListMarker(*layout_result, logical_offset);

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
    const NGInflowChildData& child_data,
    LayoutUnit child_origin_block_offset,
    bool abort_if_cleared) {
  const TextDirection direction = ConstraintSpace().Direction();
  const WritingMode writing_mode = ConstraintSpace().GetWritingMode();

  // Position all the pending floats into a temporary exclusion space. This
  // *doesn't* place them into our output exclusion space yet, as we don't know
  // where the child will be positioned, and hence what *out* BFC offset is.
  // If we already know our BFC offset, this won't have any affect.
  NGExclusionSpace tmp_exclusion_space(*exclusion_space_);
  PositionFloats(child_origin_block_offset, child_origin_block_offset,
                 unpositioned_floats_, ConstraintSpace(), &tmp_exclusion_space);

  LayoutUnit child_bfc_line_offset =
      ConstraintSpace().BfcOffset().line_offset +
      border_scrollbar_padding_.LineLeft(direction) +
      child_data.margins.LineLeft(direction);

  // The origin offset is where we should start looking for layout
  // opportunities. It needs to be adjusted by the child's clearance, in
  // addition to the parent's (if we don't know our BFC offset yet).
  NGBfcOffset origin_offset = {child_bfc_line_offset,
                               child_origin_block_offset};
  AdjustToClearance(tmp_exclusion_space.ClearanceOffset(child.Style().Clear()),
                    &origin_offset);
  if (!container_builder_.BfcOffset())
    AdjustToClearance(ConstraintSpace().ClearanceOffset(), &origin_offset);

  // Before we lay out, figure out how much inline space we have available at
  // the start block offset estimate (the child is not allowed to overlap with
  // floats, so we need to find out how much space is used by floats at this
  // block offset). This may affect the inline size of the child, e.g. when it's
  // specified as auto, or if it's a table (with table-layout:auto). This will
  // not affect percentage resolution, because that's going to be resolved
  // against the containing block, regardless of adjacent floats. When looking
  // for space, we ignore inline margins, as they will overlap with any adjacent
  // floats.
  LayoutUnit inline_margin = child_data.margins.InlineSum();
  LayoutUnit inline_size =
      (child_available_size_.inline_size - inline_margin).ClampNegativeToZero();
  NGLayoutOpportunity opportunity = tmp_exclusion_space.FindLayoutOpportunity(
      origin_offset, inline_size, NGLogicalSize());

  scoped_refptr<NGLayoutResult> layout_result;

  // Now we lay out. This will give us a child fragment and thus its size, which
  // means that we can find out where it's actually going to fit. If it doesn't
  // fit where it was laid out, and is pushed downwards, we'll lay out over
  // again, since a new BFC offset could result in a new fragment size,
  // e.g. when inline size is auto, or if we're block-fragmented.
  do {
    if (abort_if_cleared &&
        origin_offset.block_offset < opportunity.rect.BlockStartOffset()) {
      // Abort if we got pushed downwards. We need to adjust
      // child_origin_block_offset and try again.
      layout_result = nullptr;
      break;
    }

    origin_offset.block_offset = opportunity.rect.BlockStartOffset();
    // The available inline size in the child constraint space needs to include
    // inline margins, since layout algorithms (both legacy and NG) will resolve
    // auto inline size by subtracting the inline margins from available inline
    // size. We have calculated a layout opportunity without margins in mind,
    // since they overlap with adjacent floats. Now we need to add them.
    NGLogicalSize child_available_size = {
        (opportunity.rect.InlineSize() + inline_margin).ClampNegativeToZero(),
        child_available_size_.block_size};
    auto child_space =
        CreateConstraintSpaceForChild(child, child_data, child_available_size);
    layout_result = child.Layout(*child_space, child_break_token);
    DCHECK(layout_result->PhysicalFragment());

    // Now find a layout opportunity where the fragment is actually going to
    // fit.
    NGFragment fragment(writing_mode, *layout_result->PhysicalFragment());
    opportunity = tmp_exclusion_space.FindLayoutOpportunity(
        origin_offset, inline_size, fragment.Size());
  } while (origin_offset.block_offset < opportunity.rect.BlockStartOffset());

  return std::make_pair(std::move(layout_result), opportunity);
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
      CreateConstraintSpaceForChild(child, child_data, child_available_size_);
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
        CreateConstraintSpaceForChild(child, child_data, child_available_size_,
                                      child_bfc_offset);
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

  NGLogicalOffset logical_offset = CalculateLogicalOffset(
      child, fragment, child_data.margins, child_bfc_offset);

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

  PositionListMarker(*layout_result, logical_offset);

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

  if (child.IsInline()) {
    child_bfc_offset.line_offset +=
        LineOffsetForTextAlign(Style().GetTextAlign(), Style().Direction(),
                               child_available_size_.inline_size);
  }

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
    if (NeedMinMaxSize(*space, child_style)) {
      // We only want to guess the child's size here, so preceding floats are of
      // no interest.
      MinMaxSizeInput zero_input;
      sizes = child.ComputeMinMaxSize(zero_input);
    }

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
    const NGLogicalSize child_available_size,
    const WTF::Optional<NGBfcOffset> floats_bfc_offset) {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());

  NGLogicalSize available_size(child_available_size);
  NGLogicalSize percentage_size(child_percentage_size_);
  if (percentage_size.block_size == NGSizeIndefinite &&
      Node().GetDocument().InQuirksMode()) {
    // Implement percentage height calculation quirk
    // https://quirks.spec.whatwg.org/#the-percentage-height-calculation-quirk
    if (!Style().IsDisplayTableType() && !Style().HasOutOfFlowPosition()) {
      percentage_size.block_size =
          constraint_space_.PercentageResolutionSize().block_size;
    }
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

    // PositionListMarker() requires a first line baseline.
    if (container_builder_.UnpositionedListMarker()) {
      space_builder.AddBaselineRequest(
          {NGBaselineAlgorithmType::kFirstLine,
           IsHorizontalWritingMode(constraint_space_.GetWritingMode())
               ? kAlphabeticBaseline
               : kIdeographicBaseline});
    }
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

// In quirks mode, BODY and HTML elements must completely fill initial
// containing block. Percentage resolution size is minimal size
// that would fill the ICB.
LayoutUnit NGBlockLayoutAlgorithm::CalculateDefaultBlockSize() {
  if (!Node().GetDocument().InQuirksMode())
    return NGSizeIndefinite;

  bool is_quirky_element = Node().IsDocumentElement() || Node().IsBody();
  if (is_quirky_element && !Style().HasOutOfFlowPosition()) {
    LayoutUnit block_size = ConstraintSpace().AvailableSize().block_size;
    block_size -= ComputeMarginsForSelf(ConstraintSpace(), Style()).BlockSum();
    return block_size.ClampNegativeToZero();
  }
  return NGSizeIndefinite;
}

LayoutUnit NGBlockLayoutAlgorithm::CalculateMinimumBlockSize(
    const NGMarginStrut& end_margin_strut) {
  if (!Node().GetDocument().InQuirksMode())
    return NGSizeIndefinite;

  if (Node().IsDocumentElement() && Node().Style().LogicalHeight().IsAuto()) {
    return ConstraintSpace().AvailableSize().block_size -
           ComputeMarginsForSelf(ConstraintSpace(), Style()).BlockSum();
  }
  if (Node().IsBody() && Node().Style().LogicalHeight().IsAuto()) {
    LayoutUnit body_block_end_margin =
        ComputeMarginsForSelf(ConstraintSpace(), Style()).block_end;
    LayoutUnit margin_sum;
    if (container_builder_.BfcOffset()) {
      NGMarginStrut body_strut = end_margin_strut;
      body_strut.Append(body_block_end_margin, /* is_quirky */ false);
      margin_sum = container_builder_.BfcOffset().value().block_offset -
                   ConstraintSpace().BfcOffset().block_offset +
                   body_strut.Sum();
    } else {
      // end_margin_strut is top margin when we have no BfcOffset.
      margin_sum = end_margin_strut.Sum() + body_block_end_margin;
    }
    LayoutUnit minimum_block_size =
        ConstraintSpace().AvailableSize().block_size - margin_sum;
    return minimum_block_size.ClampNegativeToZero();
  }
  return NGSizeIndefinite;
}

void NGBlockLayoutAlgorithm::PositionListMarker(
    const NGLayoutResult& layout_result,
    const NGLogicalOffset& content_offset) {
  // If this is not a list-item, propagate unpositioned list markers to
  // ancestors.
  if (!node_.IsListItem()) {
    if (layout_result.UnpositionedListMarker()) {
      container_builder_.SetUnpositionedListMarker(
          layout_result.UnpositionedListMarker());
    }
    return;
  }

  // If this is a list item, add the unpositioned list marker as a child.
  NGBlockNode list_marker_node = layout_result.UnpositionedListMarker();
  if (!list_marker_node) {
    list_marker_node = container_builder_.UnpositionedListMarker();
    if (!list_marker_node)
      return;
    container_builder_.SetUnpositionedListMarker(NGBlockNode(nullptr));
  }
  NGListLayoutAlgorithm::AddListMarkerForBlockContent(
      list_marker_node, constraint_space_, *layout_result.PhysicalFragment(),
      content_offset, &container_builder_);
}

}  // namespace blink
