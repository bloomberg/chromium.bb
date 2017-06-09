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
#include "core/editing/VisibleUnits.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"

namespace blink {

namespace {

template <typename Strategy>
static PositionTemplate<Strategy> LeftVisuallyDistinctCandidate(
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
      return primary_direction == TextDirection::kLtr
                 ? PreviousVisuallyDistinctCandidate(deep_position)
                 : NextVisuallyDistinctCandidate(deep_position);
    }
    LineLayoutItem line_layout_item = box->GetLineLayoutItem();

    while (true) {
      if ((line_layout_item.IsAtomicInlineLevel() || line_layout_item.IsBR()) &&
          offset == box->CaretRightmostOffset()) {
        return box->IsLeftToRightDirection()
                   ? PreviousVisuallyDistinctCandidate(deep_position)
                   : NextVisuallyDistinctCandidate(deep_position);
      }
      if (!line_layout_item.GetNode()) {
        box = box->PrevLeafChild();
        if (!box) {
          return primary_direction == TextDirection::kLtr
                     ? PreviousVisuallyDistinctCandidate(deep_position)
                     : NextVisuallyDistinctCandidate(deep_position);
        }
        line_layout_item = box->GetLineLayoutItem();
        offset = box->CaretRightmostOffset();
        continue;
      }

      offset =
          box->IsLeftToRightDirection()
              ? PreviousGraphemeBoundaryOf(line_layout_item.GetNode(), offset)
              : NextGraphemeBoundaryOf(line_layout_item.GetNode(), offset);

      const int caret_min_offset = box->CaretMinOffset();
      const int caret_max_offset = box->CaretMaxOffset();

      if (offset > caret_min_offset && offset < caret_max_offset)
        break;

      if (box->IsLeftToRightDirection() ? offset < caret_min_offset
                                        : offset > caret_max_offset) {
        // Overshot to the left.
        InlineBox* const prev_box = box->PrevLeafChildIgnoringLineBreak();
        if (!prev_box) {
          PositionTemplate<Strategy> position_on_left =
              primary_direction == TextDirection::kLtr
                  ? PreviousVisuallyDistinctCandidate(
                        visible_position.DeepEquivalent())
                  : NextVisuallyDistinctCandidate(
                        visible_position.DeepEquivalent());
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
        offset = prev_box->CaretRightmostOffset();
        continue;
      }

      DCHECK_EQ(offset, box->CaretLeftmostOffset());

      unsigned char level = box->BidiLevel();
      InlineBox* prev_box = box->PrevLeafChild();

      if (box->Direction() == primary_direction) {
        if (!prev_box) {
          InlineBox* logical_start = nullptr;
          if (primary_direction == TextDirection::kLtr
                  ? box->Root().GetLogicalStartBoxWithNode(logical_start)
                  : box->Root().GetLogicalEndBoxWithNode(logical_start)) {
            box = logical_start;
            line_layout_item = box->GetLineLayoutItem();
            offset = primary_direction == TextDirection::kLtr
                         ? box->CaretMinOffset()
                         : box->CaretMaxOffset();
          }
          break;
        }
        if (prev_box->BidiLevel() >= level)
          break;

        level = prev_box->BidiLevel();

        InlineBox* next_box = box;
        do {
          next_box = next_box->NextLeafChild();
        } while (next_box && next_box->BidiLevel() > level);

        if (next_box && next_box->BidiLevel() == level)
          break;

        box = prev_box;
        line_layout_item = box->GetLineLayoutItem();
        offset = box->CaretRightmostOffset();
        if (box->Direction() == primary_direction)
          break;
        continue;
      }

      while (prev_box && !prev_box->GetLineLayoutItem().GetNode())
        prev_box = prev_box->PrevLeafChild();

      if (prev_box) {
        box = prev_box;
        line_layout_item = box->GetLineLayoutItem();
        offset = box->CaretRightmostOffset();
        if (box->BidiLevel() > level) {
          do {
            prev_box = prev_box->PrevLeafChild();
          } while (prev_box && prev_box->BidiLevel() > level);

          if (!prev_box || prev_box->BidiLevel() < level)
            continue;
        }
      } else {
        // Trailing edge of a secondary run. Set to the leading edge of
        // the entire run.
        while (true) {
          while (InlineBox* next_box = box->NextLeafChild()) {
            if (next_box->BidiLevel() < level)
              break;
            box = next_box;
          }
          if (box->BidiLevel() == level)
            break;
          level = box->BidiLevel();
          while (InlineBox* prev_box = box->PrevLeafChild()) {
            if (prev_box->BidiLevel() < level)
              break;
            box = prev_box;
          }
          if (box->BidiLevel() == level)
            break;
          level = box->BidiLevel();
        }
        line_layout_item = box->GetLineLayoutItem();
        offset = primary_direction == TextDirection::kLtr
                     ? box->CaretMinOffset()
                     : box->CaretMaxOffset();
      }
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

template <typename Strategy>
VisiblePositionTemplate<Strategy> LeftPositionOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const PositionTemplate<Strategy> pos =
      LeftVisuallyDistinctCandidate(visible_position);
  // TODO(yosin) Why can't we move left from the last position in a tree?
  if (pos.AtStartOfTree() || pos.AtEndOfTree())
    return VisiblePositionTemplate<Strategy>();

  const VisiblePositionTemplate<Strategy> left = CreateVisiblePosition(pos);
  DCHECK_NE(left.DeepEquivalent(), visible_position.DeepEquivalent());

  return DirectionOfEnclosingBlockOf(left.DeepEquivalent()) ==
                 TextDirection::kLtr
             ? HonorEditingBoundaryAtOrBefore(left,
                                              visible_position.DeepEquivalent())
             : HonorEditingBoundaryAtOrAfter(left,
                                             visible_position.DeepEquivalent());
}

template <typename Strategy>
PositionTemplate<Strategy> RightVisuallyDistinctCandidate(
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
      return primary_direction == TextDirection::kLtr
                 ? NextVisuallyDistinctCandidate(deep_position)
                 : PreviousVisuallyDistinctCandidate(deep_position);
    }
    LayoutObject* layout_object =
        LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());

    while (true) {
      if ((layout_object->IsAtomicInlineLevel() || layout_object->IsBR()) &&
          offset == box->CaretLeftmostOffset()) {
        return box->IsLeftToRightDirection()
                   ? NextVisuallyDistinctCandidate(deep_position)
                   : PreviousVisuallyDistinctCandidate(deep_position);
      }
      if (!layout_object->GetNode()) {
        box = box->NextLeafChild();
        if (!box) {
          return primary_direction == TextDirection::kLtr
                     ? NextVisuallyDistinctCandidate(deep_position)
                     : PreviousVisuallyDistinctCandidate(deep_position);
        }
        layout_object =
            LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());
        offset = box->CaretLeftmostOffset();
        continue;
      }

      offset =
          box->IsLeftToRightDirection()
              ? NextGraphemeBoundaryOf(layout_object->GetNode(), offset)
              : PreviousGraphemeBoundaryOf(layout_object->GetNode(), offset);

      const int caret_min_offset = box->CaretMinOffset();
      const int caret_max_offset = box->CaretMaxOffset();

      if (offset > caret_min_offset && offset < caret_max_offset)
        break;

      if (box->IsLeftToRightDirection() ? offset > caret_max_offset
                                        : offset < caret_min_offset) {
        // Overshot to the right.
        InlineBox* const next_box = box->NextLeafChildIgnoringLineBreak();
        if (!next_box) {
          PositionTemplate<Strategy> position_on_right =
              primary_direction == TextDirection::kLtr
                  ? NextVisuallyDistinctCandidate(deep_position)
                  : PreviousVisuallyDistinctCandidate(deep_position);
          if (position_on_right.IsNull())
            return PositionTemplate<Strategy>();

          InlineBox* box_on_right =
              ComputeInlineBoxPosition(position_on_right, affinity,
                                       primary_direction)
                  .inline_box;
          if (box_on_right && box_on_right->Root() == box->Root())
            return PositionTemplate<Strategy>();
          return position_on_right;
        }

        // Reposition at the other logical position corresponding to our
        // edge's visual position and go for another round.
        box = next_box;
        layout_object =
            LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());
        offset = next_box->CaretLeftmostOffset();
        continue;
      }

      DCHECK_EQ(offset, box->CaretRightmostOffset());

      unsigned char level = box->BidiLevel();
      InlineBox* next_box = box->NextLeafChild();

      if (box->Direction() == primary_direction) {
        if (!next_box) {
          InlineBox* logical_end = nullptr;
          if (primary_direction == TextDirection::kLtr
                  ? box->Root().GetLogicalEndBoxWithNode(logical_end)
                  : box->Root().GetLogicalStartBoxWithNode(logical_end)) {
            box = logical_end;
            layout_object =
                LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());
            offset = primary_direction == TextDirection::kLtr
                         ? box->CaretMaxOffset()
                         : box->CaretMinOffset();
          }
          break;
        }

        if (next_box->BidiLevel() >= level)
          break;

        level = next_box->BidiLevel();

        InlineBox* prev_box = box;
        do {
          prev_box = prev_box->PrevLeafChild();
        } while (prev_box && prev_box->BidiLevel() > level);

        // For example, abc FED 123 ^ CBA
        if (prev_box && prev_box->BidiLevel() == level)
          break;

        // For example, abc 123 ^ CBA or 123 ^ CBA abc
        box = next_box;
        layout_object =
            LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());
        offset = box->CaretLeftmostOffset();
        if (box->Direction() == primary_direction)
          break;
        continue;
      }

      while (next_box && !next_box->GetLineLayoutItem().GetNode())
        next_box = next_box->NextLeafChild();

      if (next_box) {
        box = next_box;
        layout_object =
            LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());
        offset = box->CaretLeftmostOffset();

        if (box->BidiLevel() > level) {
          do {
            next_box = next_box->NextLeafChild();
          } while (next_box && next_box->BidiLevel() > level);

          if (!next_box || next_box->BidiLevel() < level)
            continue;
        }
      } else {
        // Trailing edge of a secondary run. Set to the leading edge of the
        // entire run.
        while (true) {
          while (InlineBox* prev_box = box->PrevLeafChild()) {
            if (prev_box->BidiLevel() < level)
              break;
            box = prev_box;
          }
          if (box->BidiLevel() == level)
            break;
          level = box->BidiLevel();
          while (InlineBox* next_box = box->NextLeafChild()) {
            if (next_box->BidiLevel() < level)
              break;
            box = next_box;
          }
          if (box->BidiLevel() == level)
            break;
          level = box->BidiLevel();
        }
        layout_object =
            LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem());
        offset = primary_direction == TextDirection::kLtr
                     ? box->CaretMaxOffset()
                     : box->CaretMinOffset();
      }
      break;
    }

    p = PositionTemplate<Strategy>::EditingPositionOf(layout_object->GetNode(),
                                                      offset);

    if ((IsVisuallyEquivalentCandidate(p) &&
         MostForwardCaretPosition(p) != downstream_start) ||
        p.AtStartOfTree() || p.AtEndOfTree())
      return p;

    DCHECK_NE(p, deep_position);
  }
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> RightPositionOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const PositionTemplate<Strategy> pos =
      RightVisuallyDistinctCandidate(visible_position);
  // TODO(editing-dev): Why can't we move left from the last position in a tree?
  if (pos.AtStartOfTree() || pos.AtEndOfTree())
    return VisiblePositionTemplate<Strategy>();

  const VisiblePositionTemplate<Strategy> right = CreateVisiblePosition(pos);
  DCHECK_NE(right.DeepEquivalent(), visible_position.DeepEquivalent());

  return DirectionOfEnclosingBlockOf(right.DeepEquivalent()) ==
                 TextDirection::kLtr
             ? HonorEditingBoundaryAtOrAfter(right,
                                             visible_position.DeepEquivalent())
             : HonorEditingBoundaryAtOrBefore(
                   right, visible_position.DeepEquivalent());
}

}  // namespace

VisiblePosition LeftPositionOf(const VisiblePosition& visible_position) {
  return LeftPositionOfAlgorithm<EditingStrategy>(visible_position);
}

VisiblePositionInFlatTree LeftPositionOf(
    const VisiblePositionInFlatTree& visible_position) {
  return LeftPositionOfAlgorithm<EditingInFlatTreeStrategy>(visible_position);
}

VisiblePosition RightPositionOf(const VisiblePosition& visible_position) {
  return RightPositionOfAlgorithm<EditingStrategy>(visible_position);
}

VisiblePositionInFlatTree RightPositionOf(
    const VisiblePositionInFlatTree& visible_position) {
  return RightPositionOfAlgorithm<EditingInFlatTreeStrategy>(visible_position);
}

}  // namespace blink
