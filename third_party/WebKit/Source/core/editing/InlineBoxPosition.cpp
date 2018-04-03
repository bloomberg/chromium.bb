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

#include "core/editing/InlineBoxPosition.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/InlineBoxTraversal.h"
#include "core/editing/Position.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutText.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"

namespace blink {

namespace {

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

// Returns true if |inlineBox| starts different direction of embedded text ru.
// See [1] for details.
// [1] UNICODE BIDIRECTIONAL ALGORITHM, http://unicode.org/reports/tr9/
bool IsStartOfDifferentDirection(const InlineBox* inline_box) {
  InlineBox* prev_box = inline_box->PrevLeafChild();
  if (!prev_box)
    return true;
  if (prev_box->Direction() == inline_box->Direction())
    return true;
  DCHECK_NE(prev_box->BidiLevel(), inline_box->BidiLevel());
  return prev_box->BidiLevel() > inline_box->BidiLevel();
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

InlineBoxPosition AdjustInlineBoxPositionForPrimaryDirection(
    InlineBox* inline_box,
    int caret_offset) {
  if (caret_offset == inline_box->CaretRightmostOffset()) {
    InlineBox* const next_box = inline_box->NextLeafChild();
    if (!next_box || next_box->BidiLevel() >= inline_box->BidiLevel())
      return InlineBoxPosition(inline_box, caret_offset);

    const unsigned level = next_box->BidiLevel();
    InlineBox* const prev_box =
        InlineBoxTraversal::FindLeftBidiRun(*inline_box, level);

    // For example, abc FED 123 ^ CBA
    if (prev_box && prev_box->BidiLevel() == level)
      return InlineBoxPosition(inline_box, caret_offset);

    // For example, abc 123 ^ CBA
    InlineBox* const result_box =
        InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(*inline_box,
                                                             level);
    return InlineBoxPosition(result_box, result_box->CaretRightmostOffset());
  }

  if (IsStartOfDifferentDirection(inline_box))
    return InlineBoxPosition(inline_box, caret_offset);

  const unsigned level = inline_box->PrevLeafChild()->BidiLevel();
  InlineBox* const next_box =
      InlineBoxTraversal::FindRightBidiRun(*inline_box, level);

  if (next_box && next_box->BidiLevel() == level)
    return InlineBoxPosition(inline_box, caret_offset);

  InlineBox* const result_box =
      InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(*inline_box, level);
  return InlineBoxPosition(result_box, result_box->CaretLeftmostOffset());
}

InlineBoxPosition AdjustInlineBoxPositionForTextDirectionInternal(
    InlineBox* inline_box,
    int caret_offset,
    UnicodeBidi unicode_bidi) {
  const TextDirection primary_direction =
      inline_box->Root().Block().Style()->Direction();
  if (inline_box->Direction() == primary_direction)
    return AdjustInlineBoxPositionForPrimaryDirection(inline_box, caret_offset);

  const unsigned char level = inline_box->BidiLevel();
  if (caret_offset == inline_box->CaretLeftmostOffset()) {
    InlineBox* const prev_box = inline_box->PrevLeafChildIgnoringLineBreak();
    if (!prev_box || prev_box->BidiLevel() < level) {
      // Left edge of a secondary run. Set to the right edge of the entire
      // run.
      InlineBox* const result_box =
          InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
              *inline_box, level);
      return InlineBoxPosition(result_box, result_box->CaretRightmostOffset());
    }

    if (prev_box->BidiLevel() <= level)
      return InlineBoxPosition(inline_box, caret_offset);
    // Right edge of a "tertiary" run. Set to the left edge of that run.
    InlineBox* const result_box =
        InlineBoxTraversal::FindLeftBoundaryOfBidiRunIgnoringLineBreak(
            *inline_box, level);
    return InlineBoxPosition(result_box, result_box->CaretLeftmostOffset());
  }

  if (unicode_bidi == UnicodeBidi::kPlaintext) {
    if (inline_box->BidiLevel() < level)
      return InlineBoxPosition(inline_box, inline_box->CaretLeftmostOffset());
    return InlineBoxPosition(inline_box, inline_box->CaretRightmostOffset());
  }

  InlineBox* const next_box = inline_box->NextLeafChildIgnoringLineBreak();
  if (!next_box || next_box->BidiLevel() < level) {
    // Right edge of a secondary run. Set to the left edge of the entire
    // run.
    InlineBox* const result_box =
        InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
            *inline_box, level);
    return InlineBoxPosition(result_box, result_box->CaretLeftmostOffset());
  }

  if (next_box->BidiLevel() <= level)
    return InlineBoxPosition(inline_box, caret_offset);

  // Left edge of a "tertiary" run. Set to the right edge of that run.
  InlineBox* const result_box =
      InlineBoxTraversal::FindRightBoundaryOfBidiRunIgnoringLineBreak(
          *inline_box, level);
  return InlineBoxPosition(result_box, result_box->CaretRightmostOffset());
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

template <typename Strategy>
InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
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

InlineBoxPosition AdjustInlineBoxPositionForTextDirection(
    InlineBox* inline_box,
    int caret_offset,
    UnicodeBidi unicode_bidi) {
  return AdjustInlineBoxPositionForTextDirectionInternal(
      inline_box, caret_offset, unicode_bidi);
}

}  // namespace blink
