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

#include "core/editing/SelectionModifier.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/InlineBoxTraversal.h"
#include "core/editing/VisibleUnits.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"

namespace blink {

namespace {

// The traversal strategy for |LeftPositionOf()|.
template <typename Strategy>
struct TraversalLeft {
  STATIC_ONLY(TraversalLeft);

  static InlineBox* BackwardLeafChildOf(const InlineBox& box) {
    return box.NextLeafChild();
  }

  static int CaretEndOffsetOf(const InlineBox& box) {
    return box.CaretRightmostOffset();
  }

  static int CaretMinOffsetOf(TextDirection direction, const InlineBox& box) {
    if (direction == TextDirection::kLtr)
      return box.CaretMinOffset();
    return box.CaretMaxOffset();
  }

  static int CaretStartOffsetOf(const InlineBox& box) {
    return box.CaretLeftmostOffset();
  }

  static InlineBox* FindBackwardBidiRun(const InlineBox& box,
                                        unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBidiRun(box, bidi_level);
  }

  static InlineBox* FindBackwardBoundaryOfEntireBidiRun(const InlineBox& box,
                                                        unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(box,
                                                                bidi_level);
  }

  static InlineBox* FindForwardBidiRun(const InlineBox& box,
                                       unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBidiRun(box, bidi_level);
  }

  static InlineBox* FindForwardBoundaryOfEntireBidiRun(const InlineBox& box,
                                                       unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(box, bidi_level);
  }

  static int ForwardGraphemeBoundaryOf(TextDirection direction,
                                       Node* node,
                                       int offset) {
    if (direction == TextDirection::kLtr)
      return PreviousGraphemeBoundaryOf(node, offset);
    return NextGraphemeBoundaryOf(node, offset);
  }

  static InlineBox* ForwardLeafChildOf(const InlineBox& box) {
    return box.PrevLeafChild();
  }

  static InlineBox* ForwardLeafChildIgnoringLineBreakOf(const InlineBox& box) {
    return box.PrevLeafChildIgnoringLineBreak();
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
    if (direction == TextDirection::kLtr)
      return HonorEditingBoundaryAtOrBefore(visible_position, anchor);
    return HonorEditingBoundaryAtOrAfter(visible_position, anchor);
  }

  static Node* LogicalStartBoxOf(TextDirection direction,
                                 const InlineBox& box,
                                 InlineBox*& result_box) {
    if (direction == TextDirection::kLtr)
      return box.Root().GetLogicalStartBoxWithNode(result_box);
    return box.Root().GetLogicalEndBoxWithNode(result_box);
  }

  static bool IsOvershot(int offset, const InlineBox& box) {
    if (box.IsLeftToRightDirection())
      return offset < box.CaretMinOffset();
    return offset > box.CaretMaxOffset();
  }
};

// The traversal strategy for |RightPositionOf()|.
template <typename Strategy>
struct TraversalRight {
  STATIC_ONLY(TraversalRight);

  static InlineBox* BackwardLeafChildOf(const InlineBox& box) {
    return box.PrevLeafChild();
  }

  static int CaretEndOffsetOf(const InlineBox& box) {
    return box.CaretLeftmostOffset();
  }

  static int CaretMinOffsetOf(TextDirection direction, const InlineBox& box) {
    if (direction == TextDirection::kLtr)
      return box.CaretMaxOffset();
    return box.CaretMinOffset();
  }

  static int CaretStartOffsetOf(const InlineBox& box) {
    return box.CaretRightmostOffset();
  }

  static InlineBox* FindBackwardBidiRun(const InlineBox& box,
                                        unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBidiRun(box, bidi_level);
  }

  static InlineBox* FindBackwardBoundaryOfEntireBidiRun(const InlineBox& box,
                                                        unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(box, bidi_level);
  }

  static InlineBox* FindForwardBidiRun(const InlineBox& box,
                                       unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBidiRun(box, bidi_level);
  }

  static InlineBox* FindForwardBoundaryOfEntireBidiRun(const InlineBox& box,
                                                       unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(box,
                                                                bidi_level);
  }

  static int ForwardGraphemeBoundaryOf(TextDirection direction,
                                       Node* node,
                                       int offset) {
    if (direction == TextDirection::kLtr)
      return NextGraphemeBoundaryOf(node, offset);
    return PreviousGraphemeBoundaryOf(node, offset);
  }

  static InlineBox* ForwardLeafChildOf(const InlineBox& box) {
    return box.NextLeafChild();
  }

  static InlineBox* ForwardLeafChildIgnoringLineBreakOf(const InlineBox& box) {
    return box.NextLeafChildIgnoringLineBreak();
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
    if (direction == TextDirection::kLtr)
      return HonorEditingBoundaryAtOrAfter(visible_position, anchor);
    return HonorEditingBoundaryAtOrBefore(visible_position, anchor);
  }

  static Node* LogicalStartBoxOf(TextDirection direction,
                                 const InlineBox& box,
                                 InlineBox*& result_box) {
    if (direction == TextDirection::kLtr)
      return box.Root().GetLogicalEndBoxWithNode(result_box);
    return box.Root().GetLogicalStartBoxWithNode(result_box);
  }

  static bool IsOvershot(int offset, const InlineBox& box) {
    if (box.IsLeftToRightDirection())
      return offset > box.CaretMaxOffset();
    return offset < box.CaretMinOffset();
  }
};

// TODO(yosin): We should rename local variables and comments in
// |TraverseInternalAlgorithm()| to generic name based on |Traversal| instead of
// assuming right-to-left traversal.
template <typename Strategy, typename Traversal>
static PositionTemplate<Strategy> TraverseInternalAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const PositionTemplate<Strategy> deep_position =
      visible_position.DeepEquivalent();
  PositionTemplate<Strategy> p = deep_position;

  if (p.IsNull())
    return PositionTemplate<Strategy>();

  const PositionTemplate<Strategy> downstream_start =
      MostForwardCaretPosition(p);
  const TextDirection primary_direction = PrimaryDirectionOf(*p.AnchorNode());
  const TextAffinity affinity = visible_position.Affinity();

  while (true) {
    InlineBoxPosition box_position =
        ComputeInlineBoxPosition(p, affinity, primary_direction);
    InlineBox* box = box_position.inline_box;
    int offset = box_position.offset_in_box;
    if (!box) {
      return Traversal::ForwardVisuallyDistinctCandidateOf(primary_direction,
                                                           deep_position);
    }
    LineLayoutItem line_layout_item = box->GetLineLayoutItem();

    while (true) {
      if ((line_layout_item.IsAtomicInlineLevel() || line_layout_item.IsBR()) &&
          offset == Traversal::CaretEndOffsetOf(*box)) {
        return Traversal::ForwardVisuallyDistinctCandidateOf(box->Direction(),
                                                             deep_position);
      }
      if (!line_layout_item.GetNode()) {
        box = Traversal::ForwardLeafChildOf(*box);
        if (!box) {
          return Traversal::ForwardVisuallyDistinctCandidateOf(
              primary_direction, deep_position);
        }
        line_layout_item = box->GetLineLayoutItem();
        offset = Traversal::CaretEndOffsetOf(*box);
        continue;
      }

      offset = Traversal::ForwardGraphemeBoundaryOf(
          box->Direction(), line_layout_item.GetNode(), offset);

      const int caret_min_offset = box->CaretMinOffset();
      const int caret_max_offset = box->CaretMaxOffset();

      if (offset > caret_min_offset && offset < caret_max_offset)
        break;

      if (Traversal::IsOvershot(offset, *box)) {
        // Overshot to the left.
        InlineBox* const prev_box =
            Traversal::ForwardLeafChildIgnoringLineBreakOf(*box);
        if (!prev_box) {
          const PositionTemplate<Strategy>& position_on_left =
              Traversal::ForwardVisuallyDistinctCandidateOf(
                  primary_direction, visible_position.DeepEquivalent());
          if (position_on_left.IsNull())
            return PositionTemplate<Strategy>();

          InlineBox* box_on_left =
              ComputeInlineBoxPosition(position_on_left, affinity,
                                       primary_direction)
                  .inline_box;
          if (box_on_left && box_on_left->Root() == box->Root())
            return PositionTemplate<Strategy>();
          return position_on_left;
        }

        // Reposition at the other logical position corresponding to our
        // edge's visual position and go for another round.
        box = prev_box;
        line_layout_item = box->GetLineLayoutItem();
        offset = Traversal::CaretEndOffsetOf(*prev_box);
        continue;
      }

      DCHECK_EQ(offset, Traversal::CaretStartOffsetOf(*box));

      unsigned char level = box->BidiLevel();
      InlineBox* prev_box = Traversal::ForwardLeafChildOf(*box);

      if (box->Direction() == primary_direction) {
        if (!prev_box) {
          InlineBox* logical_start = nullptr;
          if (Traversal::LogicalStartBoxOf(primary_direction, *box,
                                           logical_start)) {
            box = logical_start;
            line_layout_item = box->GetLineLayoutItem();
            offset = Traversal::CaretMinOffsetOf(primary_direction, *box);
          }
          break;
        }
        if (prev_box->BidiLevel() >= level)
          break;

        level = prev_box->BidiLevel();

        InlineBox* const next_box = Traversal::FindBackwardBidiRun(*box, level);
        if (next_box && next_box->BidiLevel() == level)
          break;

        box = prev_box;
        line_layout_item = box->GetLineLayoutItem();
        offset = Traversal::CaretEndOffsetOf(*box);
        if (box->Direction() == primary_direction)
          break;
        continue;
      }

      while (prev_box && !prev_box->GetLineLayoutItem().GetNode())
        prev_box = Traversal::ForwardLeafChildOf(*prev_box);

      if (prev_box) {
        box = prev_box;
        line_layout_item = box->GetLineLayoutItem();
        offset = Traversal::CaretEndOffsetOf(*box);
        if (box->BidiLevel() > level) {
          prev_box = Traversal::FindForwardBidiRun(*prev_box, level);
          if (!prev_box || prev_box->BidiLevel() < level)
            continue;
        }
        break;
      }
      // Trailing edge of a secondary run. Set to the leading edge of
      // the entire run.
      while (true) {
        box = Traversal::FindBackwardBoundaryOfEntireBidiRun(*box, level);
        if (box->BidiLevel() == level)
          break;
        level = box->BidiLevel();
        box = Traversal::FindForwardBoundaryOfEntireBidiRun(*box, level);
        if (box->BidiLevel() == level)
          break;
        level = box->BidiLevel();
      }
      line_layout_item = box->GetLineLayoutItem();
      offset = Traversal::CaretMinOffsetOf(primary_direction, *box);
      break;
    }

    p = PositionTemplate<Strategy>::EditingPositionOf(
        line_layout_item.GetNode(), offset);

    if ((IsVisuallyEquivalentCandidate(p) &&
         MostForwardCaretPosition(p) != downstream_start) ||
        p.AtStartOfTree() || p.AtEndOfTree())
      return p;

    DCHECK_NE(p, deep_position);
  }
}

template <typename Strategy, typename Traversal>
VisiblePositionTemplate<Strategy> TraverseAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const PositionTemplate<Strategy> pos =
      TraverseInternalAlgorithm<Strategy, Traversal>(visible_position);
  // TODO(yosin) Why can't we move left from the last position in a tree?
  if (pos.AtStartOfTree() || pos.AtEndOfTree())
    return VisiblePositionTemplate<Strategy>();

  const VisiblePositionTemplate<Strategy> result = CreateVisiblePosition(pos);
  DCHECK_NE(result.DeepEquivalent(), visible_position.DeepEquivalent());

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
