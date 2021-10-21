// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {
// At a class A break point [1], the break value with the highest precedence
// wins. If the two values have the same precedence (e.g. "left" and "right"),
// the value specified on a latter object wins.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
inline int FragmentainerBreakPrecedence(EBreakBetween break_value) {
  // "auto" has the lowest priority.
  // "avoid*" values win over "auto".
  // "avoid-page" wins over "avoid-column".
  // "avoid" wins over "avoid-page".
  // Forced break values win over "avoid".
  // Any forced page break value wins over "column" forced break.
  // More specific break values (left, right, recto, verso) wins over generic
  // "page" values.

  switch (break_value) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case EBreakBetween::kAuto:
      return 0;
    case EBreakBetween::kAvoidColumn:
      return 1;
    case EBreakBetween::kAvoidPage:
      return 2;
    case EBreakBetween::kAvoid:
      return 3;
    case EBreakBetween::kColumn:
      return 4;
    case EBreakBetween::kPage:
      return 5;
    case EBreakBetween::kLeft:
    case EBreakBetween::kRight:
    case EBreakBetween::kRecto:
    case EBreakBetween::kVerso:
      return 6;
  }
}

// Return layout overflow block-size that's not clipped (or simply the
// block-size if it *is* clipped).
LayoutUnit BlockAxisLayoutOverflow(const NGLayoutResult& result,
                                   WritingDirectionMode writing_direction) {
  const NGPhysicalFragment& fragment = result.PhysicalFragment();
  LayoutUnit block_size = NGFragment(writing_direction, fragment).BlockSize();
  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(fragment)) {
    if (box->HasNonVisibleOverflow()) {
      OverflowClipAxes block_axis =
          writing_direction.IsHorizontal() ? kOverflowClipY : kOverflowClipX;
      if (box->GetOverflowClipAxes() & block_axis)
        box = nullptr;
    }
    if (box) {
      WritingModeConverter converter(writing_direction, fragment.Size());
      block_size =
          std::max(block_size,
                   converter.ToLogical(box->LayoutOverflow()).BlockEndOffset());
    }
    return block_size;
  }

  // Ruby annotations do not take up space in the line box, so we need this to
  // make sure that we don't let them cross the fragmentation line without
  // noticing.
  LayoutUnit annotation_overflow = result.AnnotationOverflow();
  if (annotation_overflow > LayoutUnit())
    block_size += annotation_overflow;
  return block_size;
}

// Return true if the container is being resumed after a fragmentainer break,
// and the child is at the first fragment of a node, and we are allowed to break
// before it. Normally, this isn't allowed, as that would take us nowhere,
// progress-wise, but for multicol in nested fragmentation, we'll allow it in
// some cases. If we set the appeal of breaking before the first child high
// enough, we'll automatically discard any subsequent less perfect
// breakpoints. This will make us push everything that would break with an
// appeal lower than the minimum appeal (stored in the constraint space) ahead
// of us, until we reach the next column row (in the next outer fragmentainer).
// That row may be taller, which might help us avoid breaking violations.
bool IsBreakableAtStartOfResumedContainer(
    const NGConstraintSpace& space,
    const NGLayoutResult& child_layout_result,
    const NGBoxFragmentBuilder& builder) {
  if (space.MinBreakAppeal() != kBreakAppealLastResort &&
      IsResumingLayout(builder.PreviousBreakToken())) {
    if (const auto* box_fragment = DynamicTo<NGPhysicalBoxFragment>(
            child_layout_result.PhysicalFragment()))
      return box_fragment->IsFirstForNode();
    return true;
  }
  return false;
}

}  // anonymous namespace

EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value) {
  if (FragmentainerBreakPrecedence(second_value) >=
      FragmentainerBreakPrecedence(first_value))
    return second_value;
  return first_value;
}

bool IsForcedBreakValue(const NGConstraintSpace& constraint_space,
                        EBreakBetween break_value) {
  if (break_value == EBreakBetween::kColumn)
    return constraint_space.BlockFragmentationType() == kFragmentColumn;
  // TODO(mstensho): The innermost fragmentation type doesn't tell us everything
  // here. We might want to force a break to the next page, even if we're in
  // multicol (printing multicol, for instance).
  if (break_value == EBreakBetween::kLeft ||
      break_value == EBreakBetween::kPage ||
      break_value == EBreakBetween::kRecto ||
      break_value == EBreakBetween::kRight ||
      break_value == EBreakBetween::kVerso)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}

template <typename Property>
bool IsAvoidBreakValue(const NGConstraintSpace& constraint_space,
                       Property break_value) {
  if (break_value == Property::kAvoid)
    return constraint_space.HasBlockFragmentation();
  if (break_value == Property::kAvoidColumn)
    return constraint_space.BlockFragmentationType() == kFragmentColumn;
  // TODO(mstensho): The innermost fragmentation type doesn't tell us everything
  // here. We might want to avoid breaking to the next page, even if we're
  // in multicol (printing multicol, for instance).
  if (break_value == Property::kAvoidPage)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}
// The properties break-after, break-before and break-inside may all specify
// avoid* values. break-after and break-before use EBreakBetween, and
// break-inside uses EBreakInside.
template bool CORE_TEMPLATE_EXPORT IsAvoidBreakValue(const NGConstraintSpace&,
                                                     EBreakBetween);
template bool CORE_TEMPLATE_EXPORT IsAvoidBreakValue(const NGConstraintSpace&,
                                                     EBreakInside);

EBreakBetween CalculateBreakBetweenValue(NGLayoutInputNode child,
                                         const NGLayoutResult& layout_result,
                                         const NGBoxFragmentBuilder& builder) {
  if (child.IsInline())
    return EBreakBetween::kAuto;
  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child.Style().BreakBefore(), layout_result.InitialBreakBefore());
  return builder.JoinedBreakBetweenValue(break_before);
}

NGBreakAppeal CalculateBreakAppealBefore(const NGConstraintSpace& space,
                                         NGLayoutInputNode child,
                                         const NGLayoutResult& layout_result,
                                         const NGBoxFragmentBuilder& builder,
                                         bool has_container_separation) {
  DCHECK(layout_result.Status() == NGLayoutResult::kSuccess ||
         layout_result.Status() == NGLayoutResult::kOutOfFragmentainerSpace);
  NGBreakAppeal break_appeal = kBreakAppealPerfect;
  if (!has_container_separation &&
      layout_result.Status() == NGLayoutResult::kSuccess) {
    if (!IsBreakableAtStartOfResumedContainer(space, layout_result, builder)) {
      // This is not a valid break point. If there's no container separation, it
      // means that we're breaking before the first piece of in-flow content
      // inside this block, even if it's not a valid class C break point [1]. We
      // really don't want to break here, if we can find something better.
      //
      // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
      return kBreakAppealLastResort;
    }

    // This is the first child after a break. We are normally not allowed to
    // break before those, but in this case we will allow it, to prevent
    // suboptimal breaks that might otherwise occur further ahead in the
    // fragmentainer. If necessary, we'll push this child (and all subsequent
    // content) past all the columns in the current row all the way to the the
    // next row in the next outer fragmentainer, where there may be more space,
    // in order to avoid suboptimal breaks.
    break_appeal = space.MinBreakAppeal();
  }

  EBreakBetween break_between =
      CalculateBreakBetweenValue(child, layout_result, builder);
  if (IsAvoidBreakValue(space, break_between)) {
    // If there's a break-{after,before}:avoid* involved at this breakpoint, its
    // appeal will decrease.
    break_appeal = std::min(break_appeal, kBreakAppealViolatingBreakAvoid);
  }
  return break_appeal;
}

NGBreakAppeal CalculateBreakAppealInside(
    const NGConstraintSpace& space,
    const NGLayoutResult& layout_result,
    absl::optional<NGBreakAppeal> hypothetical_appeal) {
  if (layout_result.HasForcedBreak())
    return kBreakAppealPerfect;
  const auto& physical_fragment = layout_result.PhysicalFragment();
  const auto* break_token =
      DynamicTo<NGBlockBreakToken>(physical_fragment.BreakToken());
  NGBreakAppeal appeal;
  bool consider_break_inside_avoidance;
  if (hypothetical_appeal) {
    // The hypothetical appeal of breaking inside should only be considered if
    // we haven't actually broken.
    DCHECK(!break_token);
    appeal = *hypothetical_appeal;
    consider_break_inside_avoidance = true;
  } else {
    appeal = layout_result.BreakAppeal();
    consider_break_inside_avoidance = break_token;
  }

  // We don't let break-inside:avoid affect the child's stored break appeal, but
  // we rather handle it now, on the outside. The reason is that we want to be
  // able to honor any 'avoid' values on break-before or break-after among the
  // children of the child, even if we need to disregrard a break-inside:avoid
  // rule on the child itself. This prevents us from violating more rules than
  // necessary: if we need to break inside the child (even if it should be
  // avoided), we'll at least break at the most appealing location inside.
  if (consider_break_inside_avoidance &&
      appeal > kBreakAppealViolatingBreakAvoid &&
      IsAvoidBreakValue(space, physical_fragment.Style().BreakInside()))
    appeal = kBreakAppealViolatingBreakAvoid;
  return appeal;
}

void SetupSpaceBuilderForFragmentation(const NGConstraintSpace& parent_space,
                                       const NGLayoutInputNode& child,
                                       LayoutUnit fragmentainer_offset_delta,
                                       NGConstraintSpaceBuilder* builder,
                                       bool is_new_fc) {
  DCHECK(parent_space.HasBlockFragmentation());

  // If the child is truly unbreakable, it won't participate in block
  // fragmentation. If it's too tall to fit, it will either overflow the
  // fragmentainer or get brutally sliced into pieces (without looking for
  // allowed breakpoints, since there are none, by definition), depending on
  // fragmentation type (multicol vs. printing). We still need to perform block
  // fragmentation inside inline nodes, though: While the line box itself is
  // monolithic, there may be floats inside, which are fragmentable.
  if (child.IsMonolithic() && !child.IsInline())
    return;

  builder->SetFragmentainerBlockSize(parent_space.FragmentainerBlockSize());
  builder->SetFragmentainerOffsetAtBfc(parent_space.FragmentainerOffsetAtBfc() +
                                       fragmentainer_offset_delta);
  builder->SetFragmentationType(parent_space.BlockFragmentationType());

  if (parent_space.IsInColumnBfc() && !is_new_fc)
    builder->SetIsInColumnBfc();
  builder->SetMinBreakAppeal(parent_space.MinBreakAppeal());
}

void SetupFragmentBuilderForFragmentation(
    const NGConstraintSpace& space,
    const NGBlockBreakToken* previous_break_token,
    NGBoxFragmentBuilder* builder) {
  // When resuming layout after a break, we may not be allowed to break again
  // (because of clipped overflow). In such situations, we should not call
  // SetHasBlockFragmentation(), but we still need to resume layout correctly,
  // based on the previous break token.
  DCHECK(space.HasBlockFragmentation() || previous_break_token);
  if (space.HasBlockFragmentation())
    builder->SetHasBlockFragmentation();
  builder->SetPreviousBreakToken(previous_break_token);

  if (space.IsInitialColumnBalancingPass())
    builder->SetIsInitialColumnBalancingPass();

  unsigned sequence_number = 0;
  if (previous_break_token && !previous_break_token->IsBreakBefore()) {
    sequence_number = previous_break_token->SequenceNumber() + 1;
    builder->SetIsFirstForNode(false);
  }
  builder->SetSequenceNumber(sequence_number);

  builder->AdjustBorderScrollbarPaddingForFragmentation(previous_break_token);

  if (builder->IsInitialColumnBalancingPass()) {
    const NGBoxStrut& unbreakable = builder->BorderScrollbarPadding();
    builder->PropagateTallestUnbreakableBlockSize(unbreakable.block_start);
    builder->PropagateTallestUnbreakableBlockSize(unbreakable.block_end);
  }
}

bool IsNodeFullyGrown(NGBlockNode node,
                      const NGConstraintSpace& space,
                      LayoutUnit current_total_block_size,
                      const NGBoxStrut& border_padding,
                      LayoutUnit inline_size) {
  // Pass an "infinite" intrinsic size to see how the block-size is
  // constrained. If it doesn't affect the block size, it means that the node
  // cannot grow any further.
  LayoutUnit max_block_size = ComputeBlockSizeForFragment(
      space, node.Style(), border_padding, LayoutUnit::Max(), inline_size);
  DCHECK_GE(max_block_size, current_total_block_size);
  return max_block_size == current_total_block_size;
}

NGBreakStatus FinishFragmentation(NGBlockNode node,
                                  const NGConstraintSpace& space,
                                  LayoutUnit trailing_border_padding,
                                  LayoutUnit space_left,
                                  NGBoxFragmentBuilder* builder) {
  const NGBlockBreakToken* previous_break_token = builder->PreviousBreakToken();
  LayoutUnit previously_consumed_block_size;
  if (previous_break_token && !previous_break_token->IsBreakBefore())
    previously_consumed_block_size = previous_break_token->ConsumedBlockSize();
  bool is_past_end =
      previous_break_token && previous_break_token->IsAtBlockEnd();

  LayoutUnit fragments_total_block_size = builder->FragmentsTotalBlockSize();
  LayoutUnit desired_block_size =
      fragments_total_block_size - previously_consumed_block_size;

  // Consumed block-size stored in the break tokens is always stretched to the
  // fragmentainers. If this wasn't also the case for all previous fragments
  // (because we reached the end of the node and were overflowing), we may end
  // up with negative values here.
  desired_block_size = desired_block_size.ClampNegativeToZero();

  LayoutUnit intrinsic_block_size = builder->IntrinsicBlockSize();

  LayoutUnit final_block_size = desired_block_size;

  if (builder->FoundColumnSpanner())
    builder->SetDidBreakSelf();

  if (is_past_end) {
    final_block_size = intrinsic_block_size = LayoutUnit();
  } else if (builder->FoundColumnSpanner()) {
    // There's a column spanner (or more) inside. This means that layout got
    // interrupted and thus hasn't reached the end of this block yet. We're
    // going to resume inside this block when done with the spanner(s). This is
    // true even if there is no column content siblings after the spanner(s).
    //
    // <div style="columns:2;">
    //   <div id="container" style="height:100px;">
    //     <div id="child" style="height:20px;"></div>
    //     <div style="column-span:all;"></div>
    //   </div>
    // </div>
    //
    // We'll create fragments for #container both before and after the spanner.
    // Before the spanner we'll create one for each column, each 10px tall
    // (height of #child divided into 2 columns). After the spanner, there's no
    // more content, but the specified height is 100px, so distribute what we
    // haven't already consumed (100px - 20px = 80px) over two columns. We get
    // two fragments for #container after the spanner, each 40px tall.
    final_block_size = std::min(final_block_size, intrinsic_block_size) -
                       trailing_border_padding;
  } else if (space_left != kIndefiniteSize && desired_block_size > space_left &&
             space.HasBlockFragmentation()) {
    // We're taller than what we have room for. We don't want to use more than
    // |space_left|, but if the intrinsic block-size is larger than that, it
    // means that there's something unbreakable (monolithic) inside (or we'd
    // already have broken inside). We'll allow this to overflow the
    // fragmentainer.
    //
    // TODO(mstensho): This is desired behavior for multicol, but not ideal for
    // printing, where we'd prefer the unbreakable content to be sliced into
    // different pages, lest it be clipped and lost.
    //
    // There is a last-resort breakpoint before trailing border and padding, so
    // first check if we can break there and still make progress.
    DCHECK_GE(intrinsic_block_size, trailing_border_padding);
    DCHECK_GE(desired_block_size, trailing_border_padding);

    LayoutUnit subtractable_border_padding;
    if (desired_block_size > trailing_border_padding)
      subtractable_border_padding = trailing_border_padding;

    final_block_size =
        std::min(desired_block_size - subtractable_border_padding,
                 std::max(space_left,
                          intrinsic_block_size - subtractable_border_padding));

    // We'll only need to break inside if we need more space after any
    // unbreakable content that we may have forcefully fitted here.
    if (final_block_size < desired_block_size)
      builder->SetDidBreakSelf();
  }

  LogicalBoxSides sides;
  // If this isn't the first fragment, omit the block-start border.
  if (previously_consumed_block_size)
    sides.block_start = false;
  // If this isn't the last fragment with same-flow content, omit the block-end
  // border. If something overflows the node, we'll keep on creating empty
  // fragments to contain the overflow (which establishes a parallel flow), but
  // those fragments should make no room (nor paint) block-end border/paddding.
  if (builder->DidBreakSelf() || is_past_end)
    sides.block_end = false;
  builder->SetSidesToInclude(sides);

  builder->SetConsumedBlockSize(previously_consumed_block_size +
                                final_block_size);
  builder->SetFragmentBlockSize(final_block_size);

  if (builder->FoundColumnSpanner() || !space.HasBlockFragmentation())
    return NGBreakStatus::kContinue;

  if (space_left == kIndefiniteSize) {
    // We don't know how space is available (initial column balancing pass), so
    // we won't break.
    builder->SetIsAtBlockEnd();
    return NGBreakStatus::kContinue;
  }

  if (builder->HasChildBreakInside()) {
    // We broke before or inside one of our children. Even if we fit within the
    // remaining space, and even if the child involved in the break were to be
    // in a parallel flow, we still need to prepare a break token for this node,
    // so that we can resume layout of its broken or unstarted children in the
    // next fragmentainer.
    //
    // If we're at the end of the node, we need to mark the outgoing break token
    // as such. This is a way for the parent algorithm to determine whether we
    // need to insert a break there, or whether we may continue with any sibling
    // content. If we are allowed to continue, while there's still child content
    // left to be laid out, said content ends up in a parallel flow.
    // https://www.w3.org/TR/css-break-3/#parallel-flows
    //
    // TODO(mstensho): The spec actually says that we enter a parallel flow once
    // we're past the block-end *content edge*, but here we're checking against
    // the *border edge* instead. Does it matter?
    if (is_past_end) {
      builder->SetIsAtBlockEnd();
      // We entered layout already at the end of the block (but with overflowing
      // children). So we should take up no more space on our own.
      DCHECK_EQ(desired_block_size, LayoutUnit());
    } else if (desired_block_size <= space_left) {
      // We have room for the calculated block-size in the current
      // fragmentainer, but we need to figure out whether this node is going to
      // produce more non-zero block-size fragments or not.
      //
      // If the block-size is constrained / fixed (in which case
      // IsNodeFullyGrown() will return true now), we know that we're at the
      // end. If block-size is unconstrained (or at least allowed to grow a bit
      // more), we're only at the end if no in-flow content inside broke.

      bool was_broken_by_child = builder->HasInflowChildBreakInside();
      if (!was_broken_by_child && space.IsNewFormattingContext())
        was_broken_by_child = builder->HasFloatBreakInside();

      if (!was_broken_by_child ||
          IsNodeFullyGrown(node, space, fragments_total_block_size,
                           builder->BorderPadding(),
                           builder->InitialBorderBoxSize().inline_size)) {
        if (node.HasNonVisibleBlockOverflow() &&
            builder->HasChildBreakInside()) {
          // We have reached the end of a fragmentable node that clips overflow
          // in the block direction. If something broke inside at this point, we
          // need to relayout without fragmentation, so that we don't generate
          // any additional fragments (apart from the one we're working on) from
          // this node. We don't want any zero-sized clipped fragments that
          // contribute to superfluous fragmentainers.
          return NGBreakStatus::kDisableFragmentation;
        }

        builder->SetIsAtBlockEnd();
      }
    }

    if (builder->IsAtBlockEnd()) {
      // This node is to be resumed in the next fragmentainer. Make sure that
      // consumed block-size includes the entire remainder of the fragmentainer.
      // The fragment will normally take up all that space, but not if we've
      // reached the end of the node (and we are breaking because of
      // overflow). We include the entire fragmentainer in consumed block-size
      // in order to write offsets correctly back to legacy layout.
      builder->SetConsumedBlockSize(previously_consumed_block_size +
                                    std::max(final_block_size, space_left));
    }

    return NGBreakStatus::kContinue;
  }

  if (desired_block_size > space_left) {
    // No child inside broke, but we're too tall to fit.
    if (!previously_consumed_block_size) {
      // This is the first fragment generated for the node. Avoid breaking
      // inside block-start border, scrollbar and padding, if possible. No valid
      // breakpoints there.
      const NGFragmentGeometry& geometry = builder->InitialFragmentGeometry();
      LayoutUnit block_start_unbreakable_space =
          geometry.border.block_start + geometry.scrollbar.block_start +
          geometry.padding.block_start;
      if (space_left < block_start_unbreakable_space)
        builder->ClampBreakAppeal(kBreakAppealLastResort);
    }
    if (space.BlockFragmentationType() == kFragmentColumn &&
        !space.IsInitialColumnBalancingPass())
      builder->PropagateSpaceShortage(desired_block_size - space_left);
    if (desired_block_size <= intrinsic_block_size) {
      // We only want to break inside if there's a valid class C breakpoint [1].
      // That is, we need a non-zero gap between the last child (outer block-end
      // edge) and this container (inner block-end edge). We've just found that
      // not to be the case. If we have found a better early break, we should
      // break there. Otherwise mark the break as unappealing, as breaking here
      // means that we're going to break inside the block-end padding or border,
      // or right before them. No valid breakpoints there.
      //
      // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
      if (builder->HasEarlyBreak())
        return NGBreakStatus::kNeedsEarlierBreak;
      builder->ClampBreakAppeal(kBreakAppealLastResort);
    }
    return NGBreakStatus::kContinue;
  }

  // The end of the block fits in the current fragmentainer.
  builder->SetIsAtBlockEnd();
  return NGBreakStatus::kContinue;
}

NGBreakStatus FinishFragmentationForFragmentainer(
    const NGConstraintSpace& space,
    NGBoxFragmentBuilder* builder) {
  DCHECK(builder->IsFragmentainerBoxType());
  const NGBlockBreakToken* previous_break_token = builder->PreviousBreakToken();
  LayoutUnit consumed_block_size =
      previous_break_token ? previous_break_token->ConsumedBlockSize()
                           : LayoutUnit();
  if (space.HasKnownFragmentainerBlockSize()) {
    // Just copy the block-size from the constraint space. Calculating the
    // size the regular way would cause some problems with overflow. For one,
    // we don't want to produce a break token if there's no child content that
    // requires it. When we lay out, we use FragmentainerCapacity(), so this
    // is what we need to add to consumed block-size for the next break
    // token. The fragment block-size itself will be based directly on the
    // fragmentainer size from the constraint space, though.
    LayoutUnit block_size = space.FragmentainerBlockSize();
    LayoutUnit fragmentainer_capacity = FragmentainerCapacity(space);
    builder->SetFragmentBlockSize(block_size);
    consumed_block_size += fragmentainer_capacity;
    builder->SetConsumedBlockSize(consumed_block_size);

    // We clamp the fragmentainer block size from 0 to 1 for legacy write-back
    // if there is content that overflows the zero-height fragmentainer.
    // Set the consumed block size adjustment for legacy if this results
    // in a different consumed block size than is used for NG layout.
    LayoutUnit consumed_block_size_for_legacy =
        previous_break_token
            ? previous_break_token->ConsumedBlockSizeForLegacy()
            : LayoutUnit();
    LayoutUnit legacy_fragmentainer_block_size =
        (builder->IntrinsicBlockSize() > LayoutUnit()) ? fragmentainer_capacity
                                                       : block_size;
    LayoutUnit consumed_block_size_legacy_adjustment =
        consumed_block_size_for_legacy + legacy_fragmentainer_block_size -
        consumed_block_size;
    builder->SetConsumedBlockSizeLegacyAdjustment(
        consumed_block_size_legacy_adjustment);
  } else {
    // When we are in the initial column balancing pass, use the block-size
    // calculated by the algorithm. Since any previously consumed block-size
    // is already baked in (in order to correctly honor specified block-size
    // (which makes sense to everyone but fragmentainers)), we need to extract
    // it again now.
    LayoutUnit fragments_total_block_size = builder->FragmentsTotalBlockSize();
    builder->SetFragmentBlockSize(fragments_total_block_size -
                                  consumed_block_size);
    builder->SetConsumedBlockSize(fragments_total_block_size);
  }
  if (builder->IsEmptySpannerParent() &&
      builder->HasOutOfFlowFragmentainerDescendants())
    builder->SetIsEmptySpannerParent(false);

  return NGBreakStatus::kContinue;
}

NGBreakStatus BreakBeforeChildIfNeeded(const NGConstraintSpace& space,
                                       NGLayoutInputNode child,
                                       const NGLayoutResult& layout_result,
                                       LayoutUnit fragmentainer_block_offset,
                                       bool has_container_separation,
                                       NGBoxFragmentBuilder* builder) {
  DCHECK(space.HasBlockFragmentation());

  if (has_container_separation) {
    EBreakBetween break_between =
        CalculateBreakBetweenValue(child, layout_result, *builder);
    if (IsForcedBreakValue(space, break_between)) {
      BreakBeforeChild(space, child, layout_result, fragmentainer_block_offset,
                       kBreakAppealPerfect, /* is_forced_break */ true,
                       builder);
      return NGBreakStatus::kBrokeBefore;
    }
  }

  NGBreakAppeal appeal_before = CalculateBreakAppealBefore(
      space, child, layout_result, *builder, has_container_separation);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastBreakpoint(space, child, layout_result,
                         fragmentainer_block_offset, appeal_before, builder))
    return NGBreakStatus::kContinue;

  // Breaking inside the child isn't appealing, and we're out of space. Figure
  // out where to insert a soft break. It will either be before this child, or
  // before an earlier sibling, if there's a more appealing breakpoint there.
  if (!AttemptSoftBreak(space, child, layout_result, fragmentainer_block_offset,
                        appeal_before, builder))
    return NGBreakStatus::kNeedsEarlierBreak;

  return NGBreakStatus::kBrokeBefore;
}

void BreakBeforeChild(const NGConstraintSpace& space,
                      NGLayoutInputNode child,
                      const NGLayoutResult& layout_result,
                      LayoutUnit fragmentainer_block_offset,
                      absl::optional<NGBreakAppeal> appeal,
                      bool is_forced_break,
                      NGBoxFragmentBuilder* builder) {
#if DCHECK_IS_ON()
  if (layout_result.Status() == NGLayoutResult::kSuccess) {
    // In order to successfully break before a node, this has to be its first
    // fragment.
    const auto& physical_fragment = layout_result.PhysicalFragment();
    DCHECK(!physical_fragment.IsBox() ||
           To<NGPhysicalBoxFragment>(physical_fragment).IsFirstForNode());
  }
#endif

  // Report space shortage. Note that we're not doing this for line boxes here
  // (only blocks), because line boxes need handle it in their own way (due to
  // how we implement widows).
  if (child.IsBlock() && space.HasKnownFragmentainerBlockSize()) {
    PropagateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                           builder);
  }

  // If the fragmentainer block-size is unknown, we have no reason to insert
  // soft breaks.
  DCHECK(is_forced_break || space.HasKnownFragmentainerBlockSize());

  // We'll drop the fragment (if any) on the floor and retry at the start of the
  // next fragmentainer.
  builder->AddBreakBeforeChild(child, appeal, is_forced_break);
}

void PropagateSpaceShortage(const NGConstraintSpace& space,
                            const NGLayoutResult& layout_result,
                            LayoutUnit fragmentainer_block_offset,
                            NGBoxFragmentBuilder* builder) {
  // Space shortage is only reported for soft breaks, and they can only exist if
  // we know the fragmentainer block-size.
  DCHECK(space.HasKnownFragmentainerBlockSize());

  // Only multicol cares about space shortage.
  if (space.BlockFragmentationType() != kFragmentColumn)
    return;

  LayoutUnit space_shortage;
  if (layout_result.MinimalSpaceShortage() == LayoutUnit::Max()) {
    // Calculate space shortage: Figure out how much more space would have been
    // sufficient to make the child fragment fit right here in the current
    // fragmentainer. If layout aborted, though, we can't propagate anything.
    if (layout_result.Status() != NGLayoutResult::kSuccess)
      return;
    NGFragment fragment(space.GetWritingDirection(),
                        layout_result.PhysicalFragment());
    space_shortage = fragmentainer_block_offset + fragment.BlockSize() -
                     space.FragmentainerBlockSize();
  } else {
    // However, if space shortage was reported inside the child, use that. If we
    // broke inside the child, we didn't complete layout, so calculating space
    // shortage for the child as a whole would be impossible and pointless.
    space_shortage = layout_result.MinimalSpaceShortage();
  }

  // TODO(mstensho): Turn this into a DCHECK, when the engine is ready for
  // it. Space shortage should really be positive here, or we might ultimately
  // fail to stretch the columns (column balancing).
  if (space_shortage > LayoutUnit())
    builder->PropagateSpaceShortage(space_shortage);
}

bool MovePastBreakpoint(const NGConstraintSpace& space,
                        NGLayoutInputNode child,
                        const NGLayoutResult& layout_result,
                        LayoutUnit fragmentainer_block_offset,
                        NGBreakAppeal appeal_before,
                        NGBoxFragmentBuilder* builder) {
  if (layout_result.Status() != NGLayoutResult::kSuccess) {
    // Layout aborted - no fragment was produced. There's nothing to move
    // past. We need to break before.
    DCHECK_EQ(layout_result.Status(), NGLayoutResult::kOutOfFragmentainerSpace);
    return false;
  }

  if (!child.IsInline() && builder) {
    // We need to propagate the initial break-before value up our container
    // chain, until we reach a container that's not a first child. If we get all
    // the way to the root of the fragmentation context without finding any such
    // container, we have no valid class A break point, and if a forced break
    // was requested, none will be inserted.
    builder->SetInitialBreakBeforeIfNeeded(child.Style().BreakBefore());

    // We also need to store the previous break-after value we've seen, since it
    // will serve as input to the next breakpoint (where we will combine the
    // break-after value of the previous child and the break-before value of the
    // next child, to figure out what to do at the breakpoint). The break-after
    // value of the last child will also be propagated up our container chain,
    // until we reach a container that's not a last child. This will be the
    // class A break point that it affects.
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result.FinalBreakAfter(), child.Style().BreakAfter());
    builder->SetPreviousBreakAfter(break_after);
  }

  const auto& physical_fragment = layout_result.PhysicalFragment();
  NGFragment fragment(space.GetWritingDirection(), physical_fragment);

  if (!space.HasKnownFragmentainerBlockSize()) {
    if (space.IsInitialColumnBalancingPass() && builder) {
      if (child.IsMonolithic() ||
          (child.IsBlock() &&
           IsAvoidBreakValue(space, child.Style().BreakInside()))) {
        // If this is the initial column balancing pass, attempt to make the
        // column block-size at least as large as the tallest piece of
        // monolithic content and/or block with break-inside:avoid.
        PropagateUnbreakableBlockSize(fragment.BlockSize(),
                                      fragmentainer_block_offset, builder);
      }
    }
    // We only care about soft breaks if we have a fragmentainer block-size.
    // During column balancing this may be unknown.
    return true;
  }

  const auto* break_token =
      DynamicTo<NGBlockBreakToken>(physical_fragment.BreakToken());

  LayoutUnit space_left =
      FragmentainerCapacity(space) - fragmentainer_block_offset;

  // If we haven't used any space at all in the fragmentainer yet, we cannot
  // break before this child, or there'd be no progress. We'd risk creating an
  // infinite number of fragmentainers without putting any content into them. If
  // we have set a minimum break appeal (better than kBreakAppealLastResort),
  // though, we might have to allow breaking here.
  bool refuse_break_before = space_left >= FragmentainerCapacity(space) &&
                             (!builder || !IsBreakableAtStartOfResumedContainer(
                                              space, layout_result, *builder));

  // If the child starts past the end of the fragmentainer (probably due to a
  // block-start margin), we must break before it.
  bool must_break_before = false;
  if (space_left < LayoutUnit()) {
    must_break_before = true;
  } else if (space_left == LayoutUnit()) {
    // If the child starts exactly at the end, we'll allow the child here if the
    // fragment contains the block-end of the child, or if it's a column
    // spanner. Otherwise we have to break before it. We don't want empty
    // fragments with nothing useful inside, if it's to be resumed in the next
    // fragmentainer.
    must_break_before = !layout_result.ColumnSpanner() && break_token &&
                        !break_token->IsAtBlockEnd();
  }
  if (must_break_before) {
    DCHECK(!refuse_break_before);
    return false;
  }

  NGBreakAppeal appeal_inside =
      CalculateBreakAppealInside(space, layout_result);
  if (break_token || appeal_inside < kBreakAppealPerfect) {
    // The block child broke inside, either in this fragmentation context, or in
    // an inner one. We now need to decide whether to keep that break, or if it
    // would be better to break before it. Allow breaking inside if it has the
    // same appeal or higher than breaking before or breaking earlier. Also, if
    // breaking before is impossible, break inside regardless of appeal.
    if (refuse_break_before)
      return true;
    if (appeal_inside >= appeal_before &&
        (!builder || !builder->HasEarlyBreak() ||
         appeal_inside >= builder->EarlyBreak().BreakAppeal()))
      return true;
  } else if (refuse_break_before ||
             BlockAxisLayoutOverflow(
                 layout_result, space.GetWritingDirection()) <= space_left) {
    // The child either fits, or we are not allowed to break. So we can move
    // past this breakpoint.
    if (child.IsBlock() && builder) {
      // We're tentatively not going to break before or inside this child, but
      // we'll check the appeal of breaking there anyway. It may be the best
      // breakpoint we'll ever find. (Note that we only do this for block
      // children, since, when it comes to inline layout, we first need to lay
      // out all the line boxes, so that we know what do to in order to honor
      // orphans and widows, if at all possible.)
      UpdateEarlyBreakAtBlockChild(space, To<NGBlockNode>(child), layout_result,
                                   appeal_before, builder);
    }
    return true;
  }

  // We don't want to break inside, so we should attempt to break before.
  return false;
}

void UpdateEarlyBreakAtBlockChild(const NGConstraintSpace& space,
                                  NGBlockNode child,
                                  const NGLayoutResult& layout_result,
                                  NGBreakAppeal appeal_before,
                                  NGBoxFragmentBuilder* builder) {
  // If the child already broke, it's a little too late to look for breakpoints.
  DCHECK(!layout_result.PhysicalFragment().BreakToken());

  // See if there's a good breakpoint inside the child.
  NGBreakAppeal appeal_inside = kBreakAppealLastResort;
  if (const NGEarlyBreak* breakpoint = layout_result.GetEarlyBreak()) {
    appeal_inside = CalculateBreakAppealInside(space, layout_result,
                                               breakpoint->BreakAppeal());
    if (!builder->HasEarlyBreak() ||
        builder->EarlyBreak().BreakAppeal() <= breakpoint->BreakAppeal()) {
      // Found a good breakpoint inside the child. Add the child to the early
      // break container chain, and store it.
      auto* parent_break =
          MakeGarbageCollected<NGEarlyBreak>(child, appeal_inside, breakpoint);
      builder->SetEarlyBreak(parent_break);
    }
  }

  // Breaking before isn't better if it's less appealing than what we already
  // have (obviously), and also not if it has the same appeal as the break
  // location inside the child that we just found (when the appeal is the same,
  // whatever takes us further wins).
  if (appeal_before <= appeal_inside)
    return;

  if (builder->HasEarlyBreak() &&
      builder->EarlyBreak().BreakAppeal() > appeal_before)
    return;

  builder->SetEarlyBreak(
      MakeGarbageCollected<NGEarlyBreak>(child, appeal_before));
}

bool AttemptSoftBreak(const NGConstraintSpace& space,
                      NGLayoutInputNode child,
                      const NGLayoutResult& layout_result,
                      LayoutUnit fragmentainer_block_offset,
                      NGBreakAppeal appeal_before,
                      NGBoxFragmentBuilder* builder) {
  // if there's a breakpoint with higher appeal among earlier siblings, we need
  // to abort and re-layout to that breakpoint.
  if (builder->HasEarlyBreak() &&
      builder->EarlyBreak().BreakAppeal() > appeal_before) {
    // Found a better place to break. Before aborting, calculate and report
    // space shortage from where we'd actually break.
    PropagateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                           builder);
    return false;
  }

  // Break before the child. Note that there may be a better break further up
  // with higher appeal (but it's too early to tell), in which case this
  // breakpoint will be replaced.
  BreakBeforeChild(space, child, layout_result, fragmentainer_block_offset,
                   appeal_before, /* is_forced_break */ false, builder);
  return true;
}

const NGEarlyBreak* EnterEarlyBreakInChild(const NGBlockNode& child,
                                           const NGEarlyBreak& early_break) {
  if (early_break.Type() != NGEarlyBreak::kBlock ||
      early_break.BlockNode() != child)
    return nullptr;

  // If there's no break inside, we should already have broken before the child.
  DCHECK(early_break.BreakInside());
  return early_break.BreakInside();
}

bool IsEarlyBreakTarget(const NGEarlyBreak& early_break,
                        const NGBoxFragmentBuilder& builder,
                        const NGLayoutInputNode& child) {
  if (early_break.Type() == NGEarlyBreak::kLine)
    return child.IsInline() && early_break.LineNumber() == builder.LineCount();
  return early_break.IsBreakBefore() && early_break.BlockNode() == child;
}

NGConstraintSpace CreateConstraintSpaceForColumns(
    const NGConstraintSpace& parent_space,
    LogicalSize column_size,
    LogicalSize percentage_resolution_size,
    bool allow_discard_start_margin,
    bool balance_columns,
    NGBreakAppeal min_break_appeal) {
  NGConstraintSpaceBuilder space_builder(
      parent_space, parent_space.GetWritingDirection(), /* is_new_fc */ true);
  space_builder.SetAvailableSize(column_size);
  space_builder.SetPercentageResolutionSize(percentage_resolution_size);
  space_builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchImplicit);
  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetFragmentainerBlockSize(column_size.block_size);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsInColumnBfc();
  if (balance_columns)
    space_builder.SetIsInsideBalancedColumns();
  space_builder.SetMinBreakAppeal(min_break_appeal);
  if (allow_discard_start_margin) {
    // Unless it's the first column in the multicol container, or the first
    // column after a spanner, margins at fragmentainer boundaries should be
    // eaten and truncated to zero. Note that this doesn't apply to margins at
    // forced breaks, but we'll deal with those when we get to them. Set up a
    // margin strut that eats all leading adjacent margins.
    space_builder.SetDiscardingMarginStrut();
  }

  space_builder.SetBaselineAlgorithmType(parent_space.BaselineAlgorithmType());

  return space_builder.ToConstraintSpace();
}

NGBoxFragmentBuilder CreateContainerBuilderForMulticol(
    const NGBlockNode& multicol,
    const NGConstraintSpace& space,
    const NGFragmentGeometry& fragment_geometry) {
  const ComputedStyle* style = &multicol.Style();
  NGBoxFragmentBuilder multicol_container_builder(multicol, style, &space,
                                                  style->GetWritingDirection());
  multicol_container_builder.SetIsNewFormattingContext(true);
  multicol_container_builder.SetInitialFragmentGeometry(fragment_geometry);
  multicol_container_builder.SetIsBlockFragmentationContextRoot();

  return multicol_container_builder;
}

NGConstraintSpace CreateConstraintSpaceForMulticol(
    const NGBlockNode& multicol) {
  WritingDirectionMode writing_direction_mode =
      multicol.Style().GetWritingDirection();
  NGConstraintSpaceBuilder space_builder(
      writing_direction_mode.GetWritingMode(), writing_direction_mode,
      /* is_new_fc */ true);
  return space_builder.ToConstraintSpace();
}

const NGBlockBreakToken* PreviousFragmentainerBreakToken(
    const NGBoxFragmentBuilder& container_builder,
    wtf_size_t index) {
  const NGBlockBreakToken* previous_break_token = nullptr;
  for (wtf_size_t i = index; i > 0; --i) {
    auto* previous_fragment =
        container_builder.Children()[i - 1].fragment.get();
    if (previous_fragment->IsFragmentainerBox()) {
      previous_break_token = To<NGBlockBreakToken>(
          To<NGPhysicalBoxFragment>(previous_fragment)->BreakToken());
      break;
    }
  }
  return previous_break_token;
}

const NGBlockBreakToken* FindPreviousBreakToken(
    const NGPhysicalBoxFragment& fragment) {
  const LayoutBox* box = To<LayoutBox>(fragment.GetLayoutObject());
  DCHECK(box);
  DCHECK_GE(box->PhysicalFragmentCount(), 1u);

  // Bail early if this is the first fragment. There'll be no previous break
  // token then.
  if (fragment.IsFirstForNode())
    return nullptr;

  // If this isn't the first fragment, it means that there has to be multiple
  // fragments.
  DCHECK_GT(box->PhysicalFragmentCount(), 1u);

  const NGPhysicalBoxFragment* previous_fragment;
  if (const auto* break_token = To<NGBlockBreakToken>(fragment.BreakToken())) {
    // The sequence number of the outgoing break token is the same as the index
    // of this fragment.
    DCHECK_GE(break_token->SequenceNumber(), 1u);
    previous_fragment =
        box->GetPhysicalFragment(break_token->SequenceNumber() - 1);
  } else {
    // This is the last fragment, so its incoming break token will be the
    // outgoing one from the penultimate fragment.
    previous_fragment =
        box->GetPhysicalFragment(box->PhysicalFragmentCount() - 2);
  }
  return To<NGBlockBreakToken>(previous_fragment->BreakToken());
}

wtf_size_t PreviousInnerFragmentainerIndex(
    const NGPhysicalBoxFragment& fragment) {
  // This should be a fragmentation context root, typically a multicol
  // container.
  DCHECK(fragment.IsFragmentationContextRoot());

  const LayoutBox* box = To<LayoutBox>(fragment.GetLayoutObject());
  DCHECK_GE(box->PhysicalFragmentCount(), 1u);
  if (box->PhysicalFragmentCount() == 1)
    return 0;

  wtf_size_t idx = 0;
  // Walk the list of fragments generated by the node, until we reach the
  // specified one. Note that some fragments may not contain any fragmentainers
  // at all, if all the space is taken up by column spanners, for instance.
  for (const NGPhysicalBoxFragment& walker : box->PhysicalFragments()) {
    if (&walker == &fragment)
      return idx;
    const auto* break_token = To<NGBlockBreakToken>(walker.BreakToken());

    // Find the last fragmentainer inside this fragment.
    const auto children = break_token->ChildBreakTokens();
    for (auto& child_token : base::Reversed(children)) {
      DCHECK(child_token->IsBlockType());
      if (child_token->InputNode() != break_token->InputNode()) {
        // Not a fragmentainer (probably a spanner)
        continue;
      }
      const auto& block_child_token = To<NGBlockBreakToken>(*child_token);
      // There may be a break before the first column, if we had to break
      // between the block-start border/padding of the multicol container and
      // its contents due to space shortage.
      if (block_child_token.IsBreakBefore())
        continue;
      idx = block_child_token.SequenceNumber() + 1;
      break;
    }
  }

  NOTREACHED();
  return idx;
}

PhysicalOffset OffsetInStitchedFragments(
    const NGPhysicalBoxFragment& fragment,
    PhysicalSize* out_stitched_fragments_size) {
  auto writing_direction = fragment.Style().GetWritingDirection();
  LayoutUnit stitched_block_size;
  LayoutUnit fragment_block_offset;
#if DCHECK_IS_ON()
  bool found_self = false;
#endif
  for (const NGPhysicalBoxFragment& walker :
       To<LayoutBox>(fragment.GetLayoutObject())->PhysicalFragments()) {
    if (&walker == &fragment) {
      fragment_block_offset = stitched_block_size;
#if DCHECK_IS_ON()
      found_self = true;
#endif
    }
    stitched_block_size += NGFragment(writing_direction, walker).BlockSize();
  }
#if DCHECK_IS_ON()
  DCHECK(found_self);
#endif
  LogicalSize stitched_fragments_logical_size(
      NGFragment(writing_direction, fragment).InlineSize(),
      stitched_block_size);
  PhysicalSize stitched_fragments_physical_size(ToPhysicalSize(
      stitched_fragments_logical_size, writing_direction.GetWritingMode()));
  if (out_stitched_fragments_size)
    *out_stitched_fragments_size = stitched_fragments_physical_size;
  LogicalOffset offset_in_stitched_box(LayoutUnit(), fragment_block_offset);
  WritingModeConverter converter(writing_direction,
                                 stitched_fragments_physical_size);
  return converter.ToPhysical(offset_in_stitched_box, fragment.Size());
}

}  // namespace blink
