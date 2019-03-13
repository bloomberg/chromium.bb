/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

// The traversal strategy for |LeftPositionOf()|.
template <typename Strategy>
struct TraversalLeft {
  STATIC_ONLY(TraversalLeft);

  static int CaretBackwardOffsetOf(const InlineBox& box) {
    return box.CaretRightmostOffset();
  }

  static int CaretForwardOffsetOf(const InlineBox& box) {
    return box.CaretLeftmostOffset();
  }

  static const InlineBox* ForwardLeafChildOf(const InlineBox& box) {
    return box.PrevLeafChild();
  }

  static const InlineBox* ForwardLeafChildIgnoringLineBreakOf(
      const InlineBox& box) {
    return box.PrevLeafChildIgnoringLineBreak();
  }

  static int ForwardGraphemeBoundaryOf(TextDirection direction,
                                       const Node& node,
                                       int offset) {
    if (direction == TextDirection::kLtr)
      return PreviousGraphemeBoundaryOf(node, offset);
    return NextGraphemeBoundaryOf(node, offset);
  }

  static bool IsOvershot(int offset, const InlineBox& box) {
    if (box.IsLeftToRightDirection())
      return offset < box.CaretMinOffset();
    return offset > box.CaretMaxOffset();
  }

  static PositionTemplate<Strategy> ForwardVisuallyDistinctCandidateOf(
      TextDirection direction,
      const PositionTemplate<Strategy>& position) {
    if (direction == TextDirection::kLtr)
      return PreviousVisuallyDistinctCandidate(position);
    return NextVisuallyDistinctCandidate(position);
  }

  static VisiblePositionTemplate<Strategy> HonorEditingBoundary(
      TextDirection direction,
      const VisiblePositionTemplate<Strategy>& visible_position,
      const PositionTemplate<Strategy>& anchor) {
    if (direction == TextDirection::kLtr) {
      return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
          visible_position, anchor);
    }
    return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
        visible_position, anchor);
  }

  // TODO(xiaochengh): The functions below are used only by bidi adjustment.
  // Merge them into inline_box_traversal.cc.

  static int CaretForwardOffsetInLineDirection(TextDirection line_direction,
                                               const InlineBox& box) {
    if (line_direction == TextDirection::kLtr)
      return box.CaretMinOffset();
    return box.CaretMaxOffset();
  }

  static const InlineBox* FindBackwardBidiRun(const InlineBox& box,
                                              unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBidiRun(box, bidi_level);
  }

  static const InlineBox& FindBackwardBoundaryOfEntireBidiRun(
      const InlineBox& box,
      unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(box,
                                                                bidi_level);
  }

  static const InlineBox* FindForwardBidiRun(const InlineBox& box,
                                             unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBidiRun(box, bidi_level);
  }

  static const InlineBox& FindForwardBoundaryOfEntireBidiRun(
      const InlineBox& box,
      unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(box, bidi_level);
  }

  static const InlineBox* ForwardNonPseudoLeafChildOf(const InlineBox& box) {
    for (const InlineBox* runner = ForwardLeafChildOf(box); runner;
         runner = ForwardLeafChildOf(*runner)) {
      if (runner->GetLineLayoutItem().GetNode())
        return runner;
    }
    return nullptr;
  }

  static const InlineBox* LogicalForwardMostInLine(TextDirection line_direction,
                                                   const InlineBox& box) {
    if (line_direction == TextDirection::kLtr)
      return box.Root().GetLogicalStartNonPseudoBox();
    return box.Root().GetLogicalEndNonPseudoBox();
  }

  static NGCaretNavigator::VisualCaretMovementResult ForwardPositionOf(
      const NGCaretNavigator& caret_navigator,
      const NGCaretNavigator::Position& caret_position) {
    DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
    return caret_navigator.LeftPositionOf(caret_position);
  }

  static NGCaretNavigator::Position MostBackwardPositionInFirstLine(
      const NGCaretNavigator& caret_navigator) {
    DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
    return caret_navigator.RightmostPositionInFirstLine();
  }

  static NGCaretNavigator::Position MostBackwardPositionInLastLine(
      const NGCaretNavigator& caret_navigator) {
    DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
    return caret_navigator.RightmostPositionInLastLine();
  }
};

// The traversal strategy for |RightPositionOf()|.
template <typename Strategy>
struct TraversalRight {
  STATIC_ONLY(TraversalRight);

  static int CaretBackwardOffsetOf(const InlineBox& box) {
    return box.CaretLeftmostOffset();
  }

  static int CaretForwardOffsetOf(const InlineBox& box) {
    return box.CaretRightmostOffset();
  }

  static const InlineBox* ForwardLeafChildOf(const InlineBox& box) {
    return box.NextLeafChild();
  }

  static const InlineBox* ForwardLeafChildIgnoringLineBreakOf(
      const InlineBox& box) {
    return box.NextLeafChildIgnoringLineBreak();
  }

  static int ForwardGraphemeBoundaryOf(TextDirection direction,
                                       const Node& node,
                                       int offset) {
    if (direction == TextDirection::kLtr)
      return NextGraphemeBoundaryOf(node, offset);
    return PreviousGraphemeBoundaryOf(node, offset);
  }

  static bool IsOvershot(int offset, const InlineBox& box) {
    if (box.IsLeftToRightDirection())
      return offset > box.CaretMaxOffset();
    return offset < box.CaretMinOffset();
  }

  static PositionTemplate<Strategy> ForwardVisuallyDistinctCandidateOf(
      TextDirection direction,
      const PositionTemplate<Strategy>& position) {
    if (direction == TextDirection::kLtr)
      return NextVisuallyDistinctCandidate(position);
    return PreviousVisuallyDistinctCandidate(position);
  }

  static VisiblePositionTemplate<Strategy> HonorEditingBoundary(
      TextDirection direction,
      const VisiblePositionTemplate<Strategy>& visible_position,
      const PositionTemplate<Strategy>& anchor) {
    if (direction == TextDirection::kLtr) {
      return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          visible_position, anchor);
    }
    return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
        visible_position, anchor);
  }

  // TODO(xiaochengh): The functions below are used only by bidi adjustment.
  // Merge them into inline_box_traversal.cc.

  static int CaretForwardOffsetInLineDirection(TextDirection line_direction,
                                               const InlineBox& box) {
    if (line_direction == TextDirection::kLtr)
      return box.CaretMaxOffset();
    return box.CaretMinOffset();
  }

  static const InlineBox* FindBackwardBidiRun(const InlineBox& box,
                                              unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBidiRun(box, bidi_level);
  }

  static const InlineBox& FindBackwardBoundaryOfEntireBidiRun(
      const InlineBox& box,
      unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(box, bidi_level);
  }

  static const InlineBox* FindForwardBidiRun(const InlineBox& box,
                                             unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBidiRun(box, bidi_level);
  }

  static const InlineBox& FindForwardBoundaryOfEntireBidiRun(
      const InlineBox& box,
      unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(box,
                                                                bidi_level);
  }

  static const InlineBox* ForwardNonPseudoLeafChildOf(const InlineBox& box) {
    for (const InlineBox* runner = ForwardLeafChildOf(box); runner;
         runner = ForwardLeafChildOf(*runner)) {
      if (runner->GetLineLayoutItem().GetNode())
        return runner;
    }
    return nullptr;
  }

  static const InlineBox* LogicalForwardMostInLine(TextDirection line_direction,
                                                   const InlineBox& box) {
    if (line_direction == TextDirection::kLtr)
      return box.Root().GetLogicalEndNonPseudoBox();
    return box.Root().GetLogicalStartNonPseudoBox();
  }

  static NGCaretNavigator::VisualCaretMovementResult ForwardPositionOf(
      const NGCaretNavigator& caret_navigator,
      const NGCaretNavigator::Position& caret_position) {
    DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
    return caret_navigator.RightPositionOf(caret_position);
  }

  static NGCaretNavigator::Position MostBackwardPositionInFirstLine(
      const NGCaretNavigator& caret_navigator) {
    DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
    return caret_navigator.LeftmostPositionInFirstLine();
  }

  static NGCaretNavigator::Position MostBackwardPositionInLastLine(
      const NGCaretNavigator& caret_navigator) {
    DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
    return caret_navigator.LeftmostPositionInLastLine();
  }
};

template <typename Traversal>
bool IsBeforeAtomicInlineOrLineBreak(const InlineBox& box, int offset) {
  if (offset != Traversal::CaretBackwardOffsetOf(box))
    return false;
  if (box.IsInlineTextBox() && ToInlineTextBox(box).IsLineBreak())
    return true;
  return box.GetLineLayoutItem().IsAtomicInlineLevel();
}

// TODO(xiaochengh): The function is for bidi adjustment.
// Merge it into inline_box_traversal.cc.
template <typename Traversal>
const InlineBox* LeadingBoxOfEntireSecondaryRun(const InlineBox* box) {
  const InlineBox* runner = box;
  while (true) {
    const InlineBox& backward_box =
        Traversal::FindBackwardBoundaryOfEntireBidiRun(*runner,
                                                       runner->BidiLevel());
    if (backward_box.BidiLevel() == runner->BidiLevel())
      return &backward_box;
    DCHECK_GT(backward_box.BidiLevel(), runner->BidiLevel());
    runner = &backward_box;

    const InlineBox& forward_box =
        Traversal::FindForwardBoundaryOfEntireBidiRun(*runner,
                                                      runner->BidiLevel());
    if (forward_box.BidiLevel() == runner->BidiLevel())
      return &forward_box;
    DCHECK_GT(forward_box.BidiLevel(), runner->BidiLevel());
    runner = &forward_box;
  }
}

// TODO(xiaochengh): The function is for bidi adjustment.
// Merge it into inline_box_traversal.cc.
// TODO(xiaochengh): Stop passing return value by non-const reference parameters
template <typename Traversal>
bool FindForwardBoxInPossiblyBidiContext(const InlineBox*& box,
                                         int& offset,
                                         TextDirection line_direction) {
  const unsigned char level = box->BidiLevel();
  if (box->Direction() == line_direction) {
    const InlineBox* const forward_box = Traversal::ForwardLeafChildOf(*box);
    if (!forward_box) {
      if (const InlineBox* logical_forward_most =
              Traversal::LogicalForwardMostInLine(line_direction, *box)) {
        box = logical_forward_most;
        offset =
            Traversal::CaretForwardOffsetInLineDirection(line_direction, *box);
      }
      return true;
    }
    if (forward_box->BidiLevel() >= level)
      return true;

    const unsigned char forward_level = forward_box->BidiLevel();
    const InlineBox* const backward_box =
        Traversal::FindBackwardBidiRun(*box, forward_level);
    if (backward_box && backward_box->BidiLevel() == forward_level)
      return true;

    box = forward_box;
    offset = Traversal::CaretBackwardOffsetOf(*box);
    return box->Direction() == line_direction;
  }

  const InlineBox* const forward_non_pseudo_box =
      Traversal::ForwardNonPseudoLeafChildOf(*box);
  if (forward_non_pseudo_box) {
    box = forward_non_pseudo_box;
    offset = Traversal::CaretBackwardOffsetOf(*box);
    if (box->BidiLevel() > level) {
      const InlineBox* const forward_bidi_run =
          Traversal::FindForwardBidiRun(*forward_non_pseudo_box, level);
      if (!forward_bidi_run || forward_bidi_run->BidiLevel() < level)
        return false;
    }
    return true;
  }
  // Trailing edge of a secondary run. Set to the leading edge of
  // the entire run.
  box = LeadingBoxOfEntireSecondaryRun<Traversal>(box);
  offset = Traversal::CaretForwardOffsetInLineDirection(line_direction, *box);
  return true;
}

template <typename Strategy, typename Traversal>
static PositionTemplate<Strategy> TraverseInternalAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  if (visible_position.IsNull())
    return PositionTemplate<Strategy>();

  const PositionTemplate<Strategy> deep_position =
      visible_position.DeepEquivalent();
  const PositionTemplate<Strategy> downstream_start =
      MostForwardCaretPosition(deep_position);
  const TextDirection line_direction =
      PrimaryDirectionOf(*deep_position.AnchorNode());
  const TextAffinity affinity = visible_position.Affinity();

  // Conceptually, starting from the given caret position, traverse each leaf
  // inline box and each caret position in the box (skipping CSS generated
  // content) until we have:
  // - crossed a grapheme boundary, or
  // - reached the line boundary, or
  // - reached an atomic inline.
  // TODO(xiaochengh): Refactor the code to make it closer to the above.
  // TODO(xiaochengh): Simplify loop termination conditions. Find and fix any
  // possibility of infinite iterations.
  for (InlineBoxPosition box_position =
           ComputeInlineBoxPosition(visible_position.ToPositionWithAffinity());
       ;) {
    const InlineBox* const box = box_position.inline_box;
    const int offset = box_position.offset_in_box;
    if (!box) {
      return Traversal::ForwardVisuallyDistinctCandidateOf(line_direction,
                                                           deep_position);
    }

    const InlineBox* next_box = nullptr;
    int next_offset = 0;
    bool can_create_position = false;

    // TODO(xiaochengh): The loop below iterates for exactly once. Merge it into
    // the outer loop and clean up the code.
    do {
      if (IsBeforeAtomicInlineOrLineBreak<Traversal>(*box, offset)) {
        return Traversal::ForwardVisuallyDistinctCandidateOf(box->Direction(),
                                                             deep_position);
      }

      const LineLayoutItem line_layout_item = box->GetLineLayoutItem();

      // Skip generated content.
      if (!line_layout_item.GetNode()) {
        next_box = Traversal::ForwardLeafChildOf(*box);
        if (!next_box) {
          return Traversal::ForwardVisuallyDistinctCandidateOf(line_direction,
                                                               deep_position);
        }
        next_offset = Traversal::CaretBackwardOffsetOf(*next_box);
        continue;
      }

      const int forward_grapheme_boundary =
          Traversal::ForwardGraphemeBoundaryOf(
              box->Direction(), *line_layout_item.GetNode(), offset);

      const int caret_min_offset = box->CaretMinOffset();
      const int caret_max_offset = box->CaretMaxOffset();

      if (forward_grapheme_boundary > caret_min_offset &&
          forward_grapheme_boundary < caret_max_offset) {
        next_box = box;
        next_offset = forward_grapheme_boundary;
        can_create_position = true;
        break;
      }

      if (Traversal::IsOvershot(forward_grapheme_boundary, *box)) {
        // Overshot forwardly.
        const InlineBox* const forward_box =
            Traversal::ForwardLeafChildIgnoringLineBreakOf(*box);
        if (!forward_box) {
          const PositionTemplate<Strategy>& forward_position =
              Traversal::ForwardVisuallyDistinctCandidateOf(
                  line_direction, visible_position.DeepEquivalent());
          if (forward_position.IsNull())
            return PositionTemplate<Strategy>();

          const InlineBox* forward_position_box =
              ComputeInlineBoxPosition(PositionWithAffinityTemplate<Strategy>(
                                           forward_position, affinity))
                  .inline_box;
          if (forward_position_box &&
              forward_position_box->Root() == box->Root())
            return PositionTemplate<Strategy>();
          return forward_position;
        }

        // Reposition at the other logical position corresponding to our
        // edge's visual position and go for another round.
        next_box = forward_box;
        next_offset = Traversal::CaretBackwardOffsetOf(*forward_box);
        continue;
      }

      DCHECK_EQ(forward_grapheme_boundary,
                Traversal::CaretForwardOffsetOf(*box));
      next_box = box;
      next_offset = forward_grapheme_boundary;
      // We may be at a bidi boundary, in which case the visual caret position
      // doesn't match its DOM position, and certain adjustment is needed.
      can_create_position = FindForwardBoxInPossiblyBidiContext<Traversal>(
          next_box, next_offset, line_direction);
    } while (false);

    if (!can_create_position) {
      box_position = InlineBoxPosition(next_box, next_offset);
      continue;
    }

    // TODO(xiaochengh): Eliminate single-char variable name.
    const PositionTemplate<Strategy> p =
        PositionTemplate<Strategy>::EditingPositionOf(
            next_box->GetLineLayoutItem().GetNode(), next_offset);

    if ((IsVisuallyEquivalentCandidate(p) &&
         MostForwardCaretPosition(p) != downstream_start) ||
        p.AtStartOfTree() || p.AtEndOfTree())
      return p;

    // TODO(xiaochengh): This detour to |p| seems unnecessary. Investigate if we
    // can simply use |InlineBoxPosition(next_box, next_offset)|.
    DCHECK_NE(p, deep_position);
    box_position = ComputeInlineBoxPosition(
        PositionWithAffinityTemplate<Strategy>(p, affinity));
  }
}

template <typename Strategy, typename Traversal>
PositionWithAffinityTemplate<Strategy> TraverseIntoChildContext(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsNotNull());
  DCHECK(position.IsBeforeAnchor() || position.IsAfterAnchor()) << position;
  DCHECK(position.AnchorNode()->GetLayoutObject()) << position;
  DCHECK(position.AnchorNode()->GetLayoutObject()->IsLayoutBlockFlow())
      << position;

  LayoutBlockFlow& target_block =
      *ToLayoutBlockFlow(position.AnchorNode()->GetLayoutObject());

  if (!target_block.IsLayoutNGMixin()) {
    // In most cases, we reach here by crossing editing boundary, in which case
    // returning null position suffices.
    // TODO(xiaochengh): Investigate if there are other cases that need a
    // non-trivial legacy fallback.
    return PositionWithAffinityTemplate<Strategy>();
  }

  if (!target_block.ChildrenInline() || !target_block.HasNGInlineNodeData() ||
      !target_block.GetNGInlineNodeData()->text_content.length()) {
    // TODO(xiaochengh): Implement when |target_block| has its own child blocks,
    // or when |target_block| is empty.
    return PositionWithAffinityTemplate<Strategy>();
  }

  NGCaretNavigator caret_navigator(target_block);
  DCHECK(caret_navigator.GetText().length());

  const NGCaretNavigator::Position position_in_target =
      position.IsBeforeAnchor()
          ? Traversal::MostBackwardPositionInFirstLine(caret_navigator)
          : Traversal::MostBackwardPositionInLastLine(caret_navigator);

  // When moving into inline block, the caret moves over the first character
  // seamlessly as if there's no inline block boundary. For example:
  // RightPositionOf(foo|<inline-block>bar</inline-block>)
  // -> foo<inline-block>b|ar</inline-block>
  const NGCaretNavigator::VisualCaretMovementResult result_position =
      Traversal::ForwardPositionOf(caret_navigator, position_in_target);

  if (!result_position.IsWithinContext()) {
    // TODO(xiaochengh): We reach here if |target_block| starts with an
    // enterable child context. Fix it with proper block navigation.
    // Also investigate if we reach here for other reasons.
    return PositionWithAffinityTemplate<Strategy>();
  }

  const NGOffsetMapping* mapping =
      NGInlineNode::GetOffsetMapping(&target_block);
  return FromPositionInDOMTree<Strategy>(
      mapping->GetPositionWithAffinity(*result_position.position));
}

template <typename Strategy, typename Traversal>
PositionWithAffinityTemplate<Strategy> TraverseWithBidiCaretAffinity(
    const PositionWithAffinityTemplate<Strategy>&
        start_position_with_affinity) {
  const PositionTemplate<Strategy> start_position =
      start_position_with_affinity.GetPosition();
  const Position start_position_in_dom = ToPositionInDOMTree(start_position);
  if (start_position_in_dom.IsNull())
    return PositionWithAffinityTemplate<Strategy>();

  LayoutBlockFlow* const context =
      NGOffsetMapping::GetInlineFormattingContextOf(start_position_in_dom);
  if (!context) {
    // We reach here if, e.g., the position is in an empty block.
    // TODO(xiaochengh): Investigate if we reach here for other reaseons.
#if DCHECK_IS_ON()
    const Node* node = start_position.ComputeContainerNode();
    DCHECK(node) << start_position;
    const LayoutObject* object = node->GetLayoutObject();
    DCHECK(object) << start_position;
    DCHECK(object->IsLayoutBlockFlow()) << start_position;
    DCHECK(!HasRenderedNonAnonymousDescendantsWithHeight(object))
        << start_position;
#endif
    const TextDirection block_direction =
        DirectionOfEnclosingBlockOf(start_position);
    // TODO(xiaochengh): Should return the visual line start/end of the position
    // below.
    return PositionWithAffinityTemplate<Strategy>(
        Traversal::ForwardVisuallyDistinctCandidateOf(block_direction,
                                                      start_position));
  }

  DCHECK(context->IsLayoutNGMixin());
  const NGOffsetMapping* mapping = NGInlineNode::GetOffsetMapping(context);
  DCHECK(mapping);

  const base::Optional<unsigned> start_offset =
      mapping->GetTextContentOffset(start_position_in_dom);
  DCHECK(start_offset.has_value());

  NGCaretNavigator caret_navigator(*context);
  const NGCaretNavigator::Position start_caret_position =
      caret_navigator.CaretPositionFromTextContentOffsetAndAffinity(
          start_offset.value(), start_position_with_affinity.Affinity());
  NGCaretNavigator::VisualCaretMovementResult result_caret_position =
      Traversal::ForwardPositionOf(caret_navigator, start_caret_position);
  if (result_caret_position.IsWithinContext()) {
    DCHECK(result_caret_position.position.has_value());
    return FromPositionInDOMTree<Strategy>(mapping->GetPositionWithAffinity(
        result_caret_position.position.value()));
  }

  if (result_caret_position.HasEnteredChildContext()) {
    DCHECK(result_caret_position.position.has_value());
    const PositionTemplate<Strategy> outside_child_context =
        FromPositionInDOMTree<Strategy>(
            mapping->GetPositionWithAffinity(*result_caret_position.position))
            .GetPosition();

    return TraverseIntoChildContext<Strategy, Traversal>(outside_child_context);
  }

  // We reach here if we need to move out of the current block.
  if (result_caret_position.IsBeforeContext()) {
    // TODO(xiaochengh): Move to the visual end of the previous block.
    return PositionWithAffinityTemplate<Strategy>();
  }

  DCHECK(result_caret_position.IsAfterContext());
  // TODO(xiaochengh): Move to the visual beginning of the next block.
  return PositionWithAffinityTemplate<Strategy>();
}

template <typename Strategy, typename Traversal>
VisiblePositionTemplate<Strategy> TraverseAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;

  VisiblePositionTemplate<Strategy> result;
  if (!RuntimeEnabledFeatures::BidiCaretAffinityEnabled() ||
      !NGInlineFormattingContextOf(visible_position.DeepEquivalent())) {
    const PositionTemplate<Strategy> pos =
        TraverseInternalAlgorithm<Strategy, Traversal>(visible_position);
    // TODO(yosin) Why can't we move left from the last position in a tree?
    if (pos.AtStartOfTree() || pos.AtEndOfTree())
      return VisiblePositionTemplate<Strategy>();
    result = CreateVisiblePosition(pos);
    DCHECK_NE(result.DeepEquivalent(), visible_position.DeepEquivalent());
  } else {
    const PositionWithAffinityTemplate<Strategy> pos =
        TraverseWithBidiCaretAffinity<Strategy, Traversal>(
            visible_position.ToPositionWithAffinity());
    if (pos.IsNull())
      return VisiblePositionTemplate<Strategy>();
    result = CreateVisiblePosition(pos);
    DCHECK_NE(result.ToPositionWithAffinity(),
              visible_position.ToPositionWithAffinity())
        << visible_position;
  }

  return Traversal::HonorEditingBoundary(
      DirectionOfEnclosingBlockOf(result.DeepEquivalent()), result,
      visible_position.DeepEquivalent());
}

}  // namespace

VisiblePosition LeftPositionOf(const VisiblePosition& visible_position) {
  return TraverseAlgorithm<EditingStrategy, TraversalLeft<EditingStrategy>>(
      visible_position);
}

VisiblePositionInFlatTree LeftPositionOf(
    const VisiblePositionInFlatTree& visible_position) {
  return TraverseAlgorithm<EditingInFlatTreeStrategy,
                           TraversalLeft<EditingInFlatTreeStrategy>>(
      visible_position);
}

VisiblePosition RightPositionOf(const VisiblePosition& visible_position) {
  return TraverseAlgorithm<EditingStrategy, TraversalRight<EditingStrategy>>(
      visible_position);
}

VisiblePositionInFlatTree RightPositionOf(
    const VisiblePositionInFlatTree& visible_position) {
  return TraverseAlgorithm<EditingInFlatTreeStrategy,
                           TraversalRight<EditingInFlatTreeStrategy>>(
      visible_position);
}

}  // namespace blink
