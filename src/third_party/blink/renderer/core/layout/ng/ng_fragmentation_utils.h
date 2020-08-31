// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENTATION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENTATION_UTILS_H_

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class NGLayoutResult;

// Join two adjacent break values specified on break-before and/or break-
// after. avoid* values win over auto values, and forced break values win over
// avoid* values. |first_value| is specified on an element earlier in the flow
// than |second_value|. This method is used at class A break points [1], to join
// the values of the previous break-after and the next break-before, to figure
// out whether we may, must, or should not break at that point. It is also used
// when propagating break-before values from first children and break-after
// values on last children to their container.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value);

// Return true if the specified break value has a forced break effect in the
// current fragmentation context.
bool IsForcedBreakValue(const NGConstraintSpace&, EBreakBetween);

// Return true if the specified break value means that we should avoid breaking,
// given the current fragmentation context.
template <typename Property>
bool IsAvoidBreakValue(const NGConstraintSpace&, Property);

// Return true if we're resuming layout after a previous break.
inline bool IsResumingLayout(const NGBlockBreakToken* token) {
  return token && !token->IsBreakBefore();
}

// Calculate the final "break-between" value at a class A or C breakpoint. This
// is the combination of all break-before and break-after values that met at the
// breakpoint.
EBreakBetween CalculateBreakBetweenValue(NGLayoutInputNode child,
                                         const NGLayoutResult&,
                                         const NGBoxFragmentBuilder&);

// Calculate the appeal of breaking before this child.
NGBreakAppeal CalculateBreakAppealBefore(const NGConstraintSpace&,
                                         NGLayoutInputNode child,
                                         const NGLayoutResult&,
                                         const NGBoxFragmentBuilder&,
                                         bool has_container_separation);

// Calculate the appeal of breaking inside this child.
NGBreakAppeal CalculateBreakAppealInside(const NGConstraintSpace& space,
                                         NGBlockNode child,
                                         const NGLayoutResult&);

// Return the block space that was available in the current fragmentainer at the
// start of the current block formatting context. Note that if the start of the
// current block formatting context is in a previous fragmentainer, the size of
// the current fragmentainer is returned instead.
inline LayoutUnit FragmentainerSpaceAtBfcStart(const NGConstraintSpace& space) {
  DCHECK(space.HasKnownFragmentainerBlockSize());
  return space.FragmentainerBlockSize() - space.FragmentainerOffsetAtBfc();
}

// Adjust a box strut (margins, borders, scrollbars, and/or padding) to take
// fragmentation into account. Leading block margin, border, scrollbar or
// padding should only take up space in the first fragment generated from a
// node.
inline void AdjustForFragmentation(const NGBlockBreakToken* break_token,
                                   NGBoxStrut* box_strut) {
  if (LIKELY(!break_token))
    return;
  if (break_token->IsBreakBefore())
    return;
  box_strut->block_start = LayoutUnit();
}

// Set up a child's constraint space builder for block fragmentation. The child
// participates in the same fragmentation context as parent_space. If the child
// establishes a new formatting context, |fragmentainer_offset_delta| must be
// set to the offset from the parent block formatting context, or, if the parent
// formatting context starts in a previous fragmentainer; the offset from the
// current fragmentainer block-start.
void SetupSpaceBuilderForFragmentation(const NGConstraintSpace& parent_space,
                                       const NGLayoutInputNode& child,
                                       LayoutUnit fragmentainer_offset_delta,
                                       NGConstraintSpaceBuilder*,
                                       bool is_new_fc);

// Set up a node's fragment builder for block fragmentation. To be done at the
// beginning of layout.
void SetupFragmentBuilderForFragmentation(
    const NGConstraintSpace&,
    const NGBlockBreakToken* previous_break_token,
    NGBoxFragmentBuilder*);

inline void SetupFragmentBuilderForFragmentation(const NGConstraintSpace&,
                                                 const NGInlineBreakToken*,
                                                 NGLineBoxFragmentBuilder*) {}

// Write fragmentation information to the fragment builder after layout.
void FinishFragmentation(const NGConstraintSpace&,
                         const NGBlockBreakToken* previous_break_token,
                         LayoutUnit block_size,
                         LayoutUnit intrinsic_block_size,
                         LayoutUnit space_left,
                         NGBoxFragmentBuilder*);

// Outcome of considering (and possibly attempting) breaking before a child.
enum class NGBreakStatus {
  // Continue layout. No break was inserted before the child (but there may be
  // a break inside).
  kContinue,

  // A break was inserted before the child. Discard the child fragment and
  // finish layout of the container. If there was a break inside the child, it
  // will be discarded along with the child fragment.
  kBrokeBefore,

  // The child couldn't fit here, but no break was inserted before the child,
  // as it was an unappealing place to break, and we have a better earlier
  // breakpoint. We now need to abort the current layout, and go back and
  // re-layout to said earlier breakpoint.
  kNeedsEarlierBreak
};

// Insert a fragmentainer break before the child if necessary. In that case, the
// previous in-flow position will be updated, we'll return |kBrokeBefore|. If we
// don't break inside, we'll consider the appeal of doing so anyway (and store
// it as the most appealing break point so far if that's the case), since we
// might have to go back and break here. Return |kContinue| if we're to continue
// laying out. If |kNeedsEarlierBreak| is returned, it means that we ran out of
// space, but shouldn't break before the child, but rather abort layout, and
// re-layout to a previously found good breakpoint.  If
// |has_container_separation| is true, it means that we're at a valid
// breakpoint. We obviously prefer valid breakpoints, but sometimes we need to
// break at undesirable locations. Class A breakpoints occur between block
// siblings. Class B breakpoints between line boxes. Both these breakpoint
// classes imply that we're already past the first in-flow child in the
// container, but there's also another way of achieving container separation:
// class C breakpoints. Those occur if there's a positive gap between the
// block-start content edge of the container and the block-start margin edge of
// the first in-flow child. https://www.w3.org/TR/css-break-3/#possible-breaks
NGBreakStatus BreakBeforeChildIfNeeded(const NGConstraintSpace&,
                                       NGLayoutInputNode child,
                                       const NGLayoutResult&,
                                       LayoutUnit fragmentainer_block_offset,
                                       bool has_container_separation,
                                       NGBoxFragmentBuilder*);

// Insert a break before the child, and propagate space shortage if needed.
void BreakBeforeChild(const NGConstraintSpace&,
                      NGLayoutInputNode child,
                      const NGLayoutResult&,
                      LayoutUnit fragmentainer_block_offset,
                      base::Optional<NGBreakAppeal> appeal,
                      bool is_forced_break,
                      NGBoxFragmentBuilder*);

// Propagate the block-size of unbreakable content. This is used to inflate the
// initial minimal column block-size when balancing columns, before we calculate
// a tentative (or final) column block-size. Unbreakable content will actually
// fragment if the columns aren't large enough, and we want to prevent that, if
// possible.
inline void PropagateUnbreakableBlockSize(LayoutUnit block_size,
                                          LayoutUnit fragmentainer_block_offset,
                                          NGBoxFragmentBuilder* builder) {
  // Whatever is before the block-start of the fragmentainer isn't considered to
  // intersect with the fragmentainer, so subtract it (by adding the negative
  // offset).
  if (fragmentainer_block_offset < LayoutUnit())
    block_size += fragmentainer_block_offset;
  builder->PropagateTallestUnbreakableBlockSize(block_size);
}

// Propagate space shortage to the builder and beyond, if appropriate. This is
// something we do during column balancing, when we already have a tentative
// column block-size, as a means to calculate by how much we need to stretch the
// columns to make everything fit.
void PropagateSpaceShortage(const NGConstraintSpace&,
                            const NGLayoutResult&,
                            LayoutUnit fragmentainer_block_offset,
                            NGBoxFragmentBuilder*);

// Move past the breakpoint before the child, if possible, and return true. Also
// update the appeal of breaking before or inside the child (if we're not going
// to break before it). If false is returned, it means that we need to break
// before the child (or even earlier).
bool MovePastBreakpoint(const NGConstraintSpace& space,
                        NGLayoutInputNode child,
                        const NGLayoutResult& layout_result,
                        LayoutUnit fragmentainer_block_offset,
                        NGBreakAppeal appeal_before,
                        NGBoxFragmentBuilder* builder);

// If the appeal of breaking before or inside the child is the same or higher
// than any previous breakpoint we've found, set a new breakpoint in the
// builder, and update appeal accordingly.
void UpdateEarlyBreakAtBlockChild(const NGConstraintSpace&,
                                  NGBlockNode child,
                                  const NGLayoutResult&,
                                  NGBreakAppeal appeal_before,
                                  NGBoxFragmentBuilder*);

// Attempt to insert a soft break before the child, and return true if we did.
// If false is returned, it means that the desired breakpoint is earlier in the
// container, and that we need to abort and re-layout to that breakpoint.
bool AttemptSoftBreak(const NGConstraintSpace&,
                      NGLayoutInputNode child,
                      const NGLayoutResult&,
                      LayoutUnit fragmentainer_block_offset,
                      NGBreakAppeal appeal_before,
                      NGBoxFragmentBuilder*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENTATION_UTILS_H_
