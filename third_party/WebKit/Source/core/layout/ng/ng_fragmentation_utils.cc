// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragmentation_utils.h"

#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

// Return the total amount of block space spent on a node by fragments
// preceding this one (but not including this one).
LayoutUnit PreviouslyUsedBlockSpace(const NGConstraintSpace& constraint_space,
                                    const NGPhysicalFragment& fragment) {
  if (!fragment.IsBox())
    return LayoutUnit();
  const auto* break_token = ToNGBlockBreakToken(fragment.BreakToken());
  if (!break_token)
    return LayoutUnit();
  NGBoxFragment logical_fragment(constraint_space.GetWritingMode(),
                                 ToNGPhysicalBoxFragment(fragment));
  return break_token->UsedBlockSize() - logical_fragment.BlockSize();
}

// Return true if the specified fragment is the first generated fragment of
// some node.
bool IsFirstFragment(const NGConstraintSpace& constraint_space,
                     const NGPhysicalFragment& fragment) {
  // TODO(mstensho): Figure out how to behave for non-box fragments here. How
  // can we tell whether it's the first one? Looking for previously used block
  // space certainly isn't the answer.
  if (!fragment.IsBox())
    return true;
  return PreviouslyUsedBlockSpace(constraint_space, fragment) <= LayoutUnit();
}

// Return true if the specified fragment is the final fragment of some node.
bool IsLastFragment(const NGPhysicalFragment& fragment) {
  const auto* break_token = fragment.BreakToken();
  return !break_token || break_token->IsFinished();
}

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
  if (break_value == EBreakBetween::kLeft ||
      break_value == EBreakBetween::kPage ||
      break_value == EBreakBetween::kRecto ||
      break_value == EBreakBetween::kRight ||
      break_value == EBreakBetween::kVerso)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}

bool ShouldIgnoreBlockStartMargin(const NGConstraintSpace& constraint_space,
                                  NGLayoutInputNode child,
                                  const NGBreakToken* child_break_token) {
  // Always ignore margins if we're not at the start of the child.
  if (child_break_token && child_break_token->IsBlockType() &&
      !ToNGBlockBreakToken(child_break_token)->IsBreakBefore())
    return true;

  // If we're not fragmented or have been explicitly instructed to honor
  // margins, don't ignore them.
  if (!constraint_space.HasBlockFragmentation() ||
      constraint_space.HasSeparateLeadingFragmentainerMargins())
    return false;

  // Only ignore margins if we're at the start of the fragmentainer.
  if (constraint_space.FragmentainerBlockSize() !=
      constraint_space.FragmentainerSpaceAtBfcStart())
    return false;

  // Otherwise, only ignore in-flow margins.
  return !child.IsFloating() && !child.IsOutOfFlowPositioned();
}

}  // namespace blink
