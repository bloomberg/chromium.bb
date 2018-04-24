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

#include "third_party/blink/renderer/core/editing/inline_box_position.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"

namespace blink {

namespace {

class LeftEdge {
  STATIC_ONLY(LeftEdge);

 public:
  static InlineBoxPosition UnadjustedInlineBoxPosition(
      const InlineBox& inline_box) {
    return InlineBoxPosition(&inline_box, inline_box.CaretLeftmostOffset());
  }

  static InlineBox* BackwardLeafChild(const InlineBox& inline_box) {
    return inline_box.PrevLeafChild();
  }

  static InlineBox* BackwardLeafChildIgnoringLineBreak(
      const InlineBox& inline_box) {
    return inline_box.PrevLeafChildIgnoringLineBreak();
  }

  // Returns true if |inlineBox| starts different direction of embedded text ru.
  // See [1] for details.
  // [1] UNICODE BIDIRECTIONAL ALGORITHM, http://unicode.org/reports/tr9/
  static bool IsStartOfDifferentDirection(const InlineBox& inline_box) {
    InlineBox* prev_box = inline_box.PrevLeafChild();
    if (!prev_box)
      return true;
    if (prev_box->Direction() == inline_box.Direction())
      return true;
    DCHECK_NE(prev_box->BidiLevel(), inline_box.BidiLevel());
    return prev_box->BidiLevel() > inline_box.BidiLevel();
  }

  static InlineBox* FindForwardBidiRun(const InlineBox& inline_box,
                                       unsigned bidi_level) {
    return InlineBoxTraversal::FindRightBidiRun(inline_box, bidi_level);
  }

  static InlineBoxPosition FindForwardBoundaryOfEntireBidiRunIgnoringLineBreak(
      const InlineBox& inline_box,
      unsigned bidi_level) {
    const InlineBox* result_box =
        InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
            inline_box, bidi_level);
    return InlineBoxPosition(result_box, result_box->CaretRightmostOffset());
  }

  static InlineBoxPosition FindBackwardBoundaryOfEntireBidiRun(
      const InlineBox& inline_box,
      unsigned bidi_level) {
    const InlineBox* result_box =
        InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(inline_box,
                                                            bidi_level);
    return InlineBoxPosition(result_box, result_box->CaretLeftmostOffset());
  }

  static InlineBoxPosition FindBackwardBoundaryOfBidiRunIgnoringLineBreak(
      const InlineBox& inline_box,
      unsigned bidi_level) {
    const InlineBox* result_box =
        InlineBoxTraversal::FindLeftBoundaryOfBidiRunIgnoringLineBreak(
            inline_box, bidi_level);
    return InlineBoxPosition(result_box, result_box->CaretLeftmostOffset());
  }
};

class RightEdge {
  STATIC_ONLY(RightEdge);

 public:
  static InlineBoxPosition UnadjustedInlineBoxPosition(
      const InlineBox& inline_box) {
    return InlineBoxPosition(&inline_box, inline_box.CaretRightmostOffset());
  }

  static InlineBox* BackwardLeafChild(const InlineBox& inline_box) {
    return inline_box.NextLeafChild();
  }

  static const InlineBox* BackwardLeafChildIgnoringLineBreak(
      const InlineBox& inline_box) {
    return inline_box.NextLeafChildIgnoringLineBreak();
  }

  // TODO(editing-dev): This function is almost identical to
  // LeftEdge::IsStartOfDifferentDirection(). Try to unify them.
  static bool IsStartOfDifferentDirection(const InlineBox& inline_box) {
    const InlineBox* const next_box = inline_box.NextLeafChild();
    if (!next_box)
      return true;
    return next_box->BidiLevel() >= inline_box.BidiLevel();
  }

  static InlineBox* FindForwardBidiRun(const InlineBox& inline_box,
                                       unsigned bidi_level) {
    return InlineBoxTraversal::FindLeftBidiRun(inline_box, bidi_level);
  }

  static InlineBoxPosition FindForwardBoundaryOfEntireBidiRunIgnoringLineBreak(
      const InlineBox& inline_box,
      unsigned bidi_level) {
    const InlineBox* result_box =
        InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
            inline_box, bidi_level);
    return InlineBoxPosition(result_box, result_box->CaretLeftmostOffset());
  }

  static InlineBoxPosition FindBackwardBoundaryOfEntireBidiRun(
      const InlineBox& inline_box,
      unsigned bidi_level) {
    const InlineBox* result_box =
        InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(inline_box,
                                                             bidi_level);
    return InlineBoxPosition(result_box, result_box->CaretRightmostOffset());
  }

  static InlineBoxPosition FindBackwardBoundaryOfBidiRunIgnoringLineBreak(
      const InlineBox& inline_box,
      unsigned bidi_level) {
    const InlineBox* result_box =
        InlineBoxTraversal::FindRightBoundaryOfBidiRunIgnoringLineBreak(
            inline_box, bidi_level);
    return InlineBoxPosition(result_box, result_box->CaretRightmostOffset());
  }
};

bool IsNonTextLeafChild(LayoutObject* object) {
  if (object->SlowFirstChild())
    return false;
  if (object->IsText())
    return false;
  return true;
}

InlineTextBox* SearchAheadForBetterMatch(const LayoutText* layout_object) {
  LayoutBlock* container = layout_object->ContainingBlock();
  for (LayoutObject* next = layout_object->NextInPreOrder(container); next;
       next = next->NextInPreOrder(container)) {
    if (next->IsLayoutBlock())
      return nullptr;
    if (next->IsBR())
      return nullptr;
    if (IsNonTextLeafChild(next))
      return nullptr;
    if (next->IsText()) {
      InlineTextBox* match = nullptr;
      int min_offset = INT_MAX;
      for (InlineTextBox* box : ToLayoutText(next)->TextBoxes()) {
        int caret_min_offset = box->CaretMinOffset();
        if (caret_min_offset < min_offset) {
          match = box;
          min_offset = caret_min_offset;
        }
      }
      if (match)
        return match;
    }
  }
  return nullptr;
}

template <typename Strategy>
PositionTemplate<Strategy> DownstreamIgnoringEditingBoundaries(
    PositionTemplate<Strategy> position) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostForwardCaretPosition(position, kCanCrossEditingBoundary);
  }
  return position;
}

template <typename Strategy>
PositionTemplate<Strategy> UpstreamIgnoringEditingBoundaries(
    PositionTemplate<Strategy> position) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostBackwardCaretPosition(position, kCanCrossEditingBoundary);
  }
  return position;
}

// |EdgeSide| defines which edge of |inline_box| to adjust. When
// traversing leaf InlineBoxes, "forward" means the direction from the adjusted
// edge to the other edge of |inline_box|, namely:
// - |LeftEdge|: "forward" means left-to-right
// - |RightEdge|: "forward" means right-to-left
template <typename EdgeSide>
InlineBoxPosition AdjustInlineBoxPositionForPrimaryDirection(
    const InlineBox& inline_box) {
  if (EdgeSide::IsStartOfDifferentDirection(inline_box))
    return EdgeSide::UnadjustedInlineBoxPosition(inline_box);

  const unsigned level = EdgeSide::BackwardLeafChild(inline_box)->BidiLevel();
  InlineBox* const forward_box =
      EdgeSide::FindForwardBidiRun(inline_box, level);

  // For example, abc FED 123 ^ CBA when adjusting right edge of 123
  if (forward_box && forward_box->BidiLevel() == level)
    return EdgeSide::UnadjustedInlineBoxPosition(inline_box);

  // For example, abc 123 ^ CBA when adjusting right edge of 123
  return EdgeSide::FindBackwardBoundaryOfEntireBidiRun(inline_box, level);
}

template <typename EdgeSide>
InlineBoxPosition AdjustInlineBoxPositionForTextDirectionAlgorithm(
    const InlineBox& inline_box,
    UnicodeBidi unicode_bidi) {
  const TextDirection primary_direction =
      inline_box.Root().Block().Style()->Direction();
  if (inline_box.Direction() == primary_direction)
    return AdjustInlineBoxPositionForPrimaryDirection<EdgeSide>(inline_box);

  if (unicode_bidi == UnicodeBidi::kPlaintext)
    return EdgeSide::UnadjustedInlineBoxPosition(inline_box);

  const unsigned char level = inline_box.BidiLevel();
  const InlineBox* const backward_box =
      EdgeSide::BackwardLeafChildIgnoringLineBreak(inline_box);
  if (!backward_box || backward_box->BidiLevel() < level) {
    // Backward edge of a secondary run. Set to the forward edge of the entire
    // run.
    return EdgeSide::FindForwardBoundaryOfEntireBidiRunIgnoringLineBreak(
        inline_box, level);
  }

  if (backward_box->BidiLevel() <= level)
    return EdgeSide::UnadjustedInlineBoxPosition(inline_box);

  // Forward edge of a "tertiary" run. Set to the backward edge of that run.
  return EdgeSide::FindBackwardBoundaryOfBidiRunIgnoringLineBreak(inline_box,
                                                                  level);
}

InlineBoxPosition AdjustInlineBoxPositionForTextDirection(
    InlineBox* inline_box,
    int caret_offset,
    UnicodeBidi unicode_bidi) {
  DCHECK(caret_offset == inline_box->CaretLeftmostOffset() ||
         caret_offset == inline_box->CaretRightmostOffset());
  return caret_offset == inline_box->CaretLeftmostOffset()
             ? AdjustInlineBoxPositionForTextDirectionAlgorithm<LeftEdge>(
                   *inline_box, unicode_bidi)
             : AdjustInlineBoxPositionForTextDirectionAlgorithm<RightEdge>(
                   *inline_box, unicode_bidi);
}

// Returns true if |caret_offset| is at edge of |box| based on |affinity|.
// |caret_offset| must be either |box.CaretMinOffset()| or
// |box.CaretMaxOffset()|.
bool IsCaretAtEdgeOfInlineTextBox(int caret_offset,
                                  const InlineTextBox& box,
                                  TextAffinity affinity) {
  if (caret_offset == box.CaretMinOffset())
    return affinity == TextAffinity::kDownstream;
  DCHECK_EQ(caret_offset, box.CaretMaxOffset());
  if (affinity == TextAffinity::kUpstream)
    return true;
  return box.NextLeafChild() && box.NextLeafChild()->IsLineBreak();
}

InlineBoxPosition ComputeInlineBoxPositionForTextNode(
    const LayoutText* text_layout_object,
    int caret_offset,
    TextAffinity affinity) {
  // TODO(editing-dev): Add the following DCHECK when ready.
  // DCHECK(CanUseInlineBox(*text_layout_object));

  InlineBox* inline_box = nullptr;
  InlineTextBox* candidate = nullptr;

  for (InlineTextBox* box : text_layout_object->TextBoxes()) {
    int caret_min_offset = box->CaretMinOffset();
    int caret_max_offset = box->CaretMaxOffset();

    if (caret_offset < caret_min_offset || caret_offset > caret_max_offset ||
        (caret_offset == caret_max_offset && box->IsLineBreak()))
      continue;

    if (caret_offset > caret_min_offset && caret_offset < caret_max_offset)
      return InlineBoxPosition(box, caret_offset);

    if (IsCaretAtEdgeOfInlineTextBox(caret_offset, *box, affinity)) {
      inline_box = box;
      break;
    }

    candidate = box;
  }
  if (candidate && candidate == text_layout_object->LastTextBox() &&
      affinity == TextAffinity::kDownstream) {
    inline_box = SearchAheadForBetterMatch(text_layout_object);
    if (inline_box)
      caret_offset = inline_box->CaretMinOffset();
  }
  if (!inline_box)
    inline_box = candidate;

  if (!inline_box)
    return InlineBoxPosition();
  return AdjustInlineBoxPositionForTextDirection(
      inline_box, caret_offset, text_layout_object->Style()->GetUnicodeBidi());
}

InlineBoxPosition ComputeInlineBoxPositionForAtomicInline(
    const LayoutObject* layout_object,
    int caret_offset) {
  // TODO(editing-dev): Add the following DCHECK when ready.
  // DCHECK(CanUseInlineBox(*layout_object);
  DCHECK(layout_object->IsBox());
  InlineBox* const inline_box = ToLayoutBox(layout_object)->InlineBoxWrapper();
  if (!inline_box)
    return InlineBoxPosition();
  if ((caret_offset > inline_box->CaretMinOffset() &&
       caret_offset < inline_box->CaretMaxOffset()))
    return InlineBoxPosition(inline_box, caret_offset);
  return AdjustInlineBoxPositionForTextDirection(
      inline_box, caret_offset, layout_object->Style()->GetUnicodeBidi());
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> ComputeInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>&);

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> AdjustBlockFlowPositionToInline(
    const PositionTemplate<Strategy>& position) {
  // Try a visually equivalent position with possibly opposite editability. This
  // helps in case |position| is in an editable block but surrounded by
  // non-editable positions. It acts to negate the logic at the beginning of
  // |LayoutObject::CreatePositionWithAffinity()|.
  const PositionTemplate<Strategy>& downstream_equivalent =
      DownstreamIgnoringEditingBoundaries(position);
  if (downstream_equivalent != position) {
    return ComputeInlineAdjustedPositionAlgorithm(
        PositionWithAffinityTemplate<Strategy>(downstream_equivalent,
                                               TextAffinity::kUpstream));
  }
  const PositionTemplate<Strategy>& upstream_equivalent =
      UpstreamIgnoringEditingBoundaries(position);
  if (upstream_equivalent == position ||
      DownstreamIgnoringEditingBoundaries(upstream_equivalent) == position)
    return PositionWithAffinityTemplate<Strategy>();

  return ComputeInlineAdjustedPositionAlgorithm(
      PositionWithAffinityTemplate<Strategy>(upstream_equivalent,
                                             TextAffinity::kUpstream));
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> ComputeInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position) {
  // TODO(yoichio): We don't assume |position| is canonicalized no longer and
  // there are few cases failing to compute. Fix it: crbug.com/812535.
  DCHECK(!position.AnchorNode()->IsShadowRoot()) << position;
  DCHECK(position.GetPosition().AnchorNode()->GetLayoutObject()) << position;
  const LayoutObject& layout_object =
      *position.GetPosition().AnchorNode()->GetLayoutObject();

  if (layout_object.IsText())
    return position;

  if (layout_object.IsAtomicInlineLevel()) {
    // TODO(crbug.com/567964): Change the following branch to DCHECK once fixed.
    if (!layout_object.IsInline())
      return PositionWithAffinityTemplate<Strategy>();
    return position;
  }

  if (!layout_object.IsLayoutBlockFlow() ||
      !CanHaveChildrenForEditing(position.AnchorNode()) ||
      !HasRenderedNonAnonymousDescendantsWithHeight(&layout_object))
    return PositionWithAffinityTemplate<Strategy>();
  return AdjustBlockFlowPositionToInline(position.GetPosition());
}

// Returns true if |layout_object| and |offset| points after line end.
template <typename Strategy>
bool NeedsLineEndAdjustment(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  const LayoutObject& layout_object = *position.AnchorNode()->GetLayoutObject();
  if (!layout_object.IsText())
    return false;
  const LayoutText& layout_text = ToLayoutText(layout_object);
  if (layout_text.IsBR())
    return position.IsAfterAnchor();
  // For normal text nodes.
  if (!layout_text.Style()->PreserveNewline())
    return false;
  if (!layout_text.TextLength() ||
      layout_text.CharacterAt(layout_text.TextLength() - 1) != '\n')
    return false;
  if (position.IsAfterAnchor())
    return true;
  return position.IsOffsetInAnchor() &&
         position.OffsetInContainerNode() ==
             static_cast<int>(layout_text.TextLength());
}

// Returns the first InlineBoxPosition at next line of last InlineBoxPosition
// in |layout_object| if it exists to avoid making InlineBoxPosition at end of
// line.
template <typename Strategy>
InlineBoxPosition NextLinePositionOf(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  const LayoutText& layout_text =
      ToLayoutTextOrDie(*position.AnchorNode()->GetLayoutObject());
  InlineTextBox* const last = layout_text.LastTextBox();
  if (!last)
    return InlineBoxPosition();
  const RootInlineBox& root = last->Root();
  for (const RootInlineBox* runner = root.NextRootBox(); runner;
       runner = runner->NextRootBox()) {
    InlineBox* const inline_box = runner->FirstLeafChild();
    if (!inline_box)
      continue;

    return AdjustInlineBoxPositionForTextDirection(
        inline_box, inline_box->CaretMinOffset(),
        layout_text.Style()->GetUnicodeBidi());
  }
  return InlineBoxPosition();
}

template <typename Strategy>
InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  if (NeedsLineEndAdjustment(adjusted))
    return NextLinePositionOf(adjusted);

  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  DCHECK(!position.AnchorNode()->IsShadowRoot()) << adjusted;
  DCHECK(position.AnchorNode()->GetLayoutObject()) << adjusted;
  const LayoutObject& layout_object = *position.AnchorNode()->GetLayoutObject();
  const int caret_offset = position.ComputeEditingOffset();

  if (layout_object.IsText()) {
    return ComputeInlineBoxPositionForTextNode(
        &ToLayoutText(layout_object), caret_offset, adjusted.Affinity());
  }

  DCHECK(layout_object.IsAtomicInlineLevel());
  DCHECK(layout_object.IsInline());
  return ComputeInlineBoxPositionForAtomicInline(&layout_object, caret_offset);
}

template <typename Strategy>
InlineBoxPosition ComputeInlineBoxPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  const PositionWithAffinityTemplate<Strategy> adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return InlineBoxPosition();
  return ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);
}

}  // namespace

// TODO(xiaochengh): Migrate current callers of ComputeInlineBoxPosition to
// ComputeInlineAdjustedPosition() instead.

InlineBoxPosition ComputeInlineBoxPosition(
    const PositionWithAffinity& position) {
  return ComputeInlineBoxPositionTemplate<EditingStrategy>(position);
}

InlineBoxPosition ComputeInlineBoxPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeInlineBoxPositionTemplate<EditingInFlatTreeStrategy>(position);
}

InlineBoxPosition ComputeInlineBoxPosition(const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return ComputeInlineBoxPosition(position.ToPositionWithAffinity());
}

PositionWithAffinity ComputeInlineAdjustedPosition(
    const PositionWithAffinity& position) {
  return ComputeInlineAdjustedPositionAlgorithm(position);
}

PositionInFlatTreeWithAffinity ComputeInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeInlineAdjustedPositionAlgorithm(position);
}

PositionWithAffinity ComputeInlineAdjustedPosition(
    const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return ComputeInlineAdjustedPositionAlgorithm(
      position.ToPositionWithAffinity());
}

InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPosition(
    const PositionWithAffinity& position) {
  return ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(position);
}

InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(position);
}

}  // namespace blink
