/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/VisibleSelection.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/SelectionAdjuster.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate()
    : affinity_(TextAffinity::kDownstream),
      selection_type_(kNoSelection),
      base_is_first_(true),
      is_directional_(false),
      granularity_(kCharacterGranularity),
      has_trailing_whitespace_(false) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const SelectionTemplate<Strategy>& selection)
    : base_(selection.Base()),
      extent_(selection.Extent()),
      affinity_(selection.Affinity()),
      selection_type_(kNoSelection),
      is_directional_(selection.IsDirectional()),
      granularity_(selection.Granularity()),
      has_trailing_whitespace_(selection.HasTrailingWhitespace()) {
  Validate(granularity_);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::Create(
    const SelectionTemplate<Strategy>& selection) {
  return VisibleSelectionTemplate(selection);
}

VisibleSelection CreateVisibleSelection(const SelectionInDOMTree& selection) {
  return VisibleSelection::Create(selection);
}

VisibleSelectionInFlatTree CreateVisibleSelection(
    const SelectionInFlatTree& selection) {
  return VisibleSelectionInFlatTree::Create(selection);
}

template <typename Strategy>
static SelectionType ComputeSelectionType(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end) {
  if (start.IsNull()) {
    DCHECK(end.IsNull());
    return kNoSelection;
  }
  DCHECK(!NeedsLayoutTreeUpdate(start)) << start << ' ' << end;
  if (start == end)
    return kCaretSelection;
  if (MostBackwardCaretPosition(start) == MostBackwardCaretPosition(end))
    return kCaretSelection;
  return kRangeSelection;
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const VisibleSelectionTemplate<Strategy>& other)
    : base_(other.base_),
      extent_(other.extent_),
      start_(other.start_),
      end_(other.end_),
      affinity_(other.affinity_),
      selection_type_(other.selection_type_),
      base_is_first_(other.base_is_first_),
      is_directional_(other.is_directional_),
      granularity_(other.granularity_),
      has_trailing_whitespace_(other.has_trailing_whitespace_) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>& VisibleSelectionTemplate<Strategy>::
operator=(const VisibleSelectionTemplate<Strategy>& other) {
  base_ = other.base_;
  extent_ = other.extent_;
  start_ = other.start_;
  end_ = other.end_;
  affinity_ = other.affinity_;
  selection_type_ = other.selection_type_;
  base_is_first_ = other.base_is_first_;
  is_directional_ = other.is_directional_;
  granularity_ = other.granularity_;
  has_trailing_whitespace_ = other.has_trailing_whitespace_;
  return *this;
}

template <typename Strategy>
SelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::AsSelection()
    const {
  typename SelectionTemplate<Strategy>::Builder builder;
  if (base_.IsNotNull())
    builder.SetBaseAndExtent(base_, extent_);
  return builder.SetAffinity(affinity_)
      .SetGranularity(granularity_)
      .SetIsDirectional(is_directional_)
      .SetHasTrailingWhitespace(has_trailing_whitespace_)
      .Build();
}

EphemeralRange FirstEphemeralRangeOf(const VisibleSelection& selection) {
  if (selection.IsNone())
    return EphemeralRange();
  Position start = selection.Start().ParentAnchoredEquivalent();
  Position end = selection.End().ParentAnchoredEquivalent();
  return EphemeralRange(start, end);
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::ToNormalizedEphemeralRange() const {
  if (IsNone())
    return EphemeralRangeTemplate<Strategy>();

  // Make sure we have an updated layout since this function is called
  // in the course of running edit commands which modify the DOM.
  // Failing to ensure this can result in equivalentXXXPosition calls returning
  // incorrect results.
  DCHECK(!NeedsLayoutTreeUpdate(start_)) << *this;

  if (IsCaret()) {
    // If the selection is a caret, move the range start upstream. This
    // helps us match the conventions of text editors tested, which make
    // style determinations based on the character before the caret, if any.
    const PositionTemplate<Strategy> start =
        MostBackwardCaretPosition(start_).ParentAnchoredEquivalent();
    return EphemeralRangeTemplate<Strategy>(start, start);
  }
  // If the selection is a range, select the minimum range that encompasses
  // the selection. Again, this is to match the conventions of text editors
  // tested, which make style determinations based on the first character of
  // the selection. For instance, this operation helps to make sure that the
  // "X" selected below is the only thing selected. The range should not be
  // allowed to "leak" out to the end of the previous text node, or to the
  // beginning of the next text node, each of which has a different style.
  //
  // On a treasure map, <b>X</b> marks the spot.
  //                       ^ selected
  //
  DCHECK(IsRange());
  return NormalizeRange(EphemeralRangeTemplate<Strategy>(start_, end_));
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::AppendTrailingWhitespace() {
  if (IsNone())
    return;
  DCHECK_EQ(granularity_, kWordGranularity);
  if (!IsRange())
    return;
  const PositionTemplate<Strategy>& new_end = SkipWhitespace(end_);
  if (end_ == new_end)
    return;
  has_trailing_whitespace_ = true;
  end_ = new_end;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::SetBaseAndExtentToDeepEquivalents() {
  // Move the selection to rendered positions, if possible.
  bool base_and_extent_equal = base_ == extent_;
  if (base_.IsNotNull()) {
    base_ = CreateVisiblePosition(base_, affinity_).DeepEquivalent();
    if (base_and_extent_equal)
      extent_ = base_;
  }
  if (extent_.IsNotNull() && !base_and_extent_equal)
    extent_ = CreateVisiblePosition(extent_, affinity_).DeepEquivalent();

  // Make sure we do not have a dangling base or extent.
  if (base_.IsNull() && extent_.IsNull()) {
    base_is_first_ = true;
  } else if (base_.IsNull()) {
    base_ = extent_;
    base_is_first_ = true;
  } else if (extent_.IsNull()) {
    extent_ = base_;
    base_is_first_ = true;
  } else {
    base_is_first_ = base_.CompareTo(extent_) <= 0;
  }
}

template <typename Strategy>
static PositionTemplate<Strategy> ComputeStartRespectingGranularity(
    const PositionWithAffinityTemplate<Strategy>& passed_start,
    TextGranularity granularity) {
  DCHECK(passed_start.IsNotNull());

  switch (granularity) {
    case kCharacterGranularity:
      // Don't do any expansion.
      return passed_start.GetPosition();
    case kWordGranularity: {
      // General case: Select the word the caret is positioned inside of.
      // If the caret is on the word boundary, select the word according to
      // |wordSide|.
      // Edge case: If the caret is after the last word in a soft-wrapped line
      // or the last word in the document, select that last word
      // (LeftWordIfOnBoundary).
      // Edge case: If the caret is after the last word in a paragraph, select
      // from the the end of the last word to the line break (also
      // RightWordIfOnBoundary);
      const VisiblePositionTemplate<Strategy> visible_start =
          CreateVisiblePosition(passed_start);
      if (IsEndOfEditableOrNonEditableContent(visible_start) ||
          (IsEndOfLine(visible_start) && !IsStartOfLine(visible_start) &&
           !IsEndOfParagraph(visible_start))) {
        return StartOfWord(visible_start, kLeftWordIfOnBoundary)
            .DeepEquivalent();
      }
      return StartOfWord(visible_start, kRightWordIfOnBoundary)
          .DeepEquivalent();
    }
    case kSentenceGranularity:
      return StartOfSentence(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
    case kLineGranularity:
      return StartOfLine(CreateVisiblePosition(passed_start)).DeepEquivalent();
    case kLineBoundary:
      return StartOfLine(CreateVisiblePosition(passed_start)).DeepEquivalent();
    case kParagraphGranularity: {
      const VisiblePositionTemplate<Strategy> pos =
          CreateVisiblePosition(passed_start);
      if (IsStartOfLine(pos) && IsEndOfEditableOrNonEditableContent(pos))
        return StartOfParagraph(PreviousPositionOf(pos)).DeepEquivalent();
      return StartOfParagraph(pos).DeepEquivalent();
    }
    case kDocumentBoundary:
      return StartOfDocument(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
    case kParagraphBoundary:
      return StartOfParagraph(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
    case kSentenceBoundary:
      return StartOfSentence(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
  }

  NOTREACHED();
  return passed_start.GetPosition();
}

template <typename Strategy>
static PositionTemplate<Strategy> ComputeEndRespectingGranularity(
    const PositionTemplate<Strategy>& start,
    const PositionWithAffinityTemplate<Strategy>& passed_end,
    TextGranularity granularity) {
  DCHECK(passed_end.IsNotNull());

  switch (granularity) {
    case kCharacterGranularity:
      // Don't do any expansion.
      return passed_end.GetPosition();
    case kWordGranularity: {
      // General case: Select the word the caret is positioned inside of.
      // If the caret is on the word boundary, select the word according to
      // |wordSide|.
      // Edge case: If the caret is after the last word in a soft-wrapped line
      // or the last word in the document, select that last word
      // (|LeftWordIfOnBoundary|).
      // Edge case: If the caret is after the last word in a paragraph, select
      // from the the end of the last word to the line break (also
      // |RightWordIfOnBoundary|);
      const VisiblePositionTemplate<Strategy> original_end =
          CreateVisiblePosition(passed_end);
      EWordSide side = kRightWordIfOnBoundary;
      if (IsEndOfEditableOrNonEditableContent(original_end) ||
          (IsEndOfLine(original_end) && !IsStartOfLine(original_end) &&
           !IsEndOfParagraph(original_end)))
        side = kLeftWordIfOnBoundary;

      const VisiblePositionTemplate<Strategy> word_end =
          EndOfWord(original_end, side);
      if (!IsEndOfParagraph(original_end))
        return word_end.DeepEquivalent();
      if (IsEmptyTableCell(start.AnchorNode()))
        return word_end.DeepEquivalent();

      // Select the paragraph break (the space from the end of a paragraph
      // to the start of the next one) to match TextEdit.
      const VisiblePositionTemplate<Strategy> end = NextPositionOf(word_end);
      Element* const table = TableElementJustBefore(end);
      if (!table) {
        if (end.IsNull())
          return word_end.DeepEquivalent();
        return end.DeepEquivalent();
      }

      if (!IsEnclosingBlock(table))
        return word_end.DeepEquivalent();

      // The paragraph break after the last paragraph in the last cell
      // of a block table ends at the start of the paragraph after the
      // table.
      const VisiblePositionTemplate<Strategy> next =
          NextPositionOf(end, kCannotCrossEditingBoundary);
      if (next.IsNull())
        return word_end.DeepEquivalent();
      return next.DeepEquivalent();
    }
    case kSentenceGranularity:
      return EndOfSentence(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case kLineGranularity: {
      const VisiblePositionTemplate<Strategy> end =
          EndOfLine(CreateVisiblePosition(passed_end));
      if (!IsEndOfParagraph(end))
        return end.DeepEquivalent();
      // If the end of this line is at the end of a paragraph, include the
      // space after the end of the line in the selection.
      const VisiblePositionTemplate<Strategy> next = NextPositionOf(end);
      if (next.IsNull())
        return end.DeepEquivalent();
      return next.DeepEquivalent();
    }
    case kLineBoundary:
      return EndOfLine(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case kParagraphGranularity: {
      const VisiblePositionTemplate<Strategy> visible_paragraph_end =
          EndOfParagraph(CreateVisiblePosition(passed_end));

      // Include the "paragraph break" (the space from the end of this
      // paragraph to the start of the next one) in the selection.
      const VisiblePositionTemplate<Strategy> end =
          NextPositionOf(visible_paragraph_end);

      Element* const table = TableElementJustBefore(end);
      if (!table) {
        if (end.IsNull())
          return visible_paragraph_end.DeepEquivalent();
        return end.DeepEquivalent();
      }

      if (!IsEnclosingBlock(table)) {
        // There is no paragraph break after the last paragraph in the
        // last cell of an inline table.
        return visible_paragraph_end.DeepEquivalent();
      }

      // The paragraph break after the last paragraph in the last cell of
      // a block table ends at the start of the paragraph after the table,
      // not at the position just after the table.
      const VisiblePositionTemplate<Strategy> next =
          NextPositionOf(end, kCannotCrossEditingBoundary);
      if (next.IsNull())
        return visible_paragraph_end.DeepEquivalent();
      return next.DeepEquivalent();
    }
    case kDocumentBoundary:
      return EndOfDocument(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case kParagraphBoundary:
      return EndOfParagraph(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case kSentenceBoundary:
      return EndOfSentence(CreateVisiblePosition(passed_end)).DeepEquivalent();
  }
  NOTREACHED();
  return passed_end.GetPosition();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::UpdateSelectionType() {
  selection_type_ = ComputeSelectionType(start_, end_);

  // Affinity only makes sense for a caret
  if (selection_type_ != kCaretSelection)
    affinity_ = TextAffinity::kDownstream;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::Validate(TextGranularity granularity) {
  DCHECK(!NeedsLayoutTreeUpdate(base_));
  DCHECK(!NeedsLayoutTreeUpdate(extent_));
  // TODO(xiaochengh): Add a DocumentLifecycle::DisallowTransitionScope here.

  granularity_ = granularity;
  if (granularity_ != kWordGranularity)
    has_trailing_whitespace_ = false;
  SetBaseAndExtentToDeepEquivalents();
  if (base_.IsNull() || extent_.IsNull()) {
    base_ = extent_ = start_ = end_ = PositionTemplate<Strategy>();
    UpdateSelectionType();
    return;
  }

  const PositionTemplate<Strategy> start = base_is_first_ ? base_ : extent_;
  const PositionTemplate<Strategy> new_start =
      ComputeStartRespectingGranularity(
          PositionWithAffinityTemplate<Strategy>(start, affinity_),
          granularity);
  start_ = new_start.IsNotNull() ? new_start : start;

  const PositionTemplate<Strategy> end = base_is_first_ ? extent_ : base_;
  const PositionTemplate<Strategy> new_end = ComputeEndRespectingGranularity(
      start_, PositionWithAffinityTemplate<Strategy>(end, affinity_),
      granularity);
  end_ = new_end.IsNotNull() ? new_end : end;
  // TODO(editing-dev): Once we fix http://crbug.com/734481, we should use
  // |DCHECK_LE()|.
  CHECK_LE(start_, end_);

  AdjustSelectionToAvoidCrossingShadowBoundaries();
  // TODO(editing-dev): Once we fix http://crbug.com/734481, we should use
  // |DCHECK_LE()|.
  CHECK_LE(start_, end_);
  AdjustSelectionToAvoidCrossingEditingBoundaries();
  // TODO(editing-dev): Once we fix http://crbug.com/734481, we should use
  // |DCHECK_LE()|.
  CHECK_LE(start_, end_);
  UpdateSelectionType();

  if (GetSelectionType() == kRangeSelection) {
    // "Constrain" the selection to be the smallest equivalent range of
    // nodes. This is a somewhat arbitrary choice, but experience shows that
    // it is useful to make to make the selection "canonical" (if only for
    // purposes of comparing selections). This is an ideal point of the code
    // to do this operation, since all selection changes that result in a
    // RANGE come through here before anyone uses it.
    // TODO(yosin) Canonicalizing is good, but haven't we already done it
    // (when we set these two positions to |VisiblePosition|
    // |deepEquivalent()|s above)?
    start_ = MostForwardCaretPosition(start_);
    end_ = MostBackwardCaretPosition(end_);
    // TODO(editing-dev): Once we fix http://crbug.com/734481, we should use
    // |DCHECK_LE()|.
    CHECK_LE(start_, end_);
  }
  if (!has_trailing_whitespace_)
    return;
  AppendTrailingWhitespace();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsValidFor(
    const Document& document) const {
  if (IsNone())
    return true;

  return base_.GetDocument() == &document && !base_.IsOrphan() &&
         !extent_.IsOrphan() && !start_.IsOrphan() && !end_.IsOrphan();
}

// TODO(yosin) This function breaks the invariant of this class.
// But because we use VisibleSelection to store values in editing commands for
// use when undoing the command, we need to be able to create a selection that
// while currently invalid, will be valid once the changes are undone. This is a
// design problem. To fix it we either need to change the invariants of
// |VisibleSelection| or create a new class for editing to use that can
// manipulate selections that are not currently valid.
template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::SetWithoutValidation(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.IsNull() || extent.IsNull()) {
    base_ = extent_ = start_ = end_ = PositionTemplate<Strategy>();
    UpdateSelectionType();
    return;
  }

  base_ = base;
  extent_ = extent;
  base_is_first_ = base.CompareTo(extent) <= 0;
  if (base_is_first_) {
    start_ = base;
    end_ = extent;
  } else {
    start_ = extent;
    end_ = base;
  }
  selection_type_ = base == extent ? kCaretSelection : kRangeSelection;
  if (selection_type_ != kCaretSelection) {
    // Since |m_affinity| for non-|CaretSelection| is always |Downstream|,
    // we should keep this invariant. Note: This function can be called with
    // |m_affinity| is |TextAffinity::Upstream|.
    affinity_ = TextAffinity::kDownstream;
  }
}

template <typename Strategy>
void VisibleSelectionTemplate<
    Strategy>::AdjustSelectionToAvoidCrossingShadowBoundaries() {
  if (base_.IsNull() || start_.IsNull() || base_.IsNull())
    return;
  SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(this);
}

static Element* LowestEditableAncestor(Node* node) {
  while (node) {
    if (HasEditableStyle(*node))
      return RootEditableElement(*node);
    if (isHTMLBodyElement(*node))
      break;
    node = node->parentNode();
  }

  return nullptr;
}

template <typename Strategy>
void VisibleSelectionTemplate<
    Strategy>::AdjustSelectionToAvoidCrossingEditingBoundaries() {
  if (base_.IsNull() || start_.IsNull() || end_.IsNull())
    return;

  ContainerNode* base_root = HighestEditableRoot(base_);
  ContainerNode* start_root = HighestEditableRoot(start_);
  ContainerNode* end_root = HighestEditableRoot(end_);

  Element* base_editable_ancestor =
      LowestEditableAncestor(base_.ComputeContainerNode());

  // The base, start and end are all in the same region.  No adjustment
  // necessary.
  if (base_root == start_root && base_root == end_root)
    return;

  // The selection is based in editable content.
  if (base_root) {
    // If the start is outside the base's editable root, cap it at the start of
    // that root.
    // If the start is in non-editable content that is inside the base's
    // editable root, put it at the first editable position after start inside
    // the base's editable root.
    if (start_root != base_root) {
      const VisiblePositionTemplate<Strategy> first =
          FirstEditableVisiblePositionAfterPositionInRoot(start_, *base_root);
      start_ = first.DeepEquivalent();
      if (start_.IsNull()) {
        NOTREACHED();
        start_ = end_;
      }
    }
    // If the end is outside the base's editable root, cap it at the end of that
    // root.
    // If the end is in non-editable content that is inside the base's root, put
    // it at the last editable position before the end inside the base's root.
    if (end_root != base_root) {
      const VisiblePositionTemplate<Strategy> last =
          LastEditableVisiblePositionBeforePositionInRoot(end_, *base_root);
      end_ = last.DeepEquivalent();
      if (end_.IsNull())
        end_ = start_;
    }
    // The selection is based in non-editable content.
  } else {
    // FIXME: Non-editable pieces inside editable content should be atomic, in
    // the same way that editable pieces in non-editable content are atomic.

    // The selection ends in editable content or non-editable content inside a
    // different editable ancestor, move backward until non-editable content
    // inside the same lowest editable ancestor is reached.
    Element* end_editable_ancestor =
        LowestEditableAncestor(end_.ComputeContainerNode());
    if (end_root || end_editable_ancestor != base_editable_ancestor) {
      PositionTemplate<Strategy> p = PreviousVisuallyDistinctCandidate(end_);
      Element* shadow_ancestor =
          end_root ? end_root->OwnerShadowHost() : nullptr;
      if (p.IsNull() && shadow_ancestor)
        p = PositionTemplate<Strategy>::AfterNode(shadow_ancestor);
      while (p.IsNotNull() &&
             !(LowestEditableAncestor(p.ComputeContainerNode()) ==
                   base_editable_ancestor &&
               !IsEditablePosition(p))) {
        Element* root = RootEditableElementOf(p);
        shadow_ancestor = root ? root->OwnerShadowHost() : nullptr;
        p = IsAtomicNode(p.ComputeContainerNode())
                ? PositionTemplate<Strategy>::InParentBeforeNode(
                      *p.ComputeContainerNode())
                : PreviousVisuallyDistinctCandidate(p);
        if (p.IsNull() && shadow_ancestor)
          p = PositionTemplate<Strategy>::AfterNode(shadow_ancestor);
      }
      const VisiblePositionTemplate<Strategy> previous =
          CreateVisiblePosition(p);

      if (previous.IsNull()) {
        // The selection crosses an Editing boundary.  This is a
        // programmer error in the editing code.  Happy debugging!
        NOTREACHED();
        *this = VisibleSelectionTemplate<Strategy>();
        return;
      }
      end_ = previous.DeepEquivalent();
    }

    // The selection starts in editable content or non-editable content inside a
    // different editable ancestor, move forward until non-editable content
    // inside the same lowest editable ancestor is reached.
    Element* start_editable_ancestor =
        LowestEditableAncestor(start_.ComputeContainerNode());
    if (start_root || start_editable_ancestor != base_editable_ancestor) {
      PositionTemplate<Strategy> p = NextVisuallyDistinctCandidate(start_);
      Element* shadow_ancestor =
          start_root ? start_root->OwnerShadowHost() : nullptr;
      if (p.IsNull() && shadow_ancestor)
        p = PositionTemplate<Strategy>::BeforeNode(*shadow_ancestor);
      while (p.IsNotNull() &&
             !(LowestEditableAncestor(p.ComputeContainerNode()) ==
                   base_editable_ancestor &&
               !IsEditablePosition(p))) {
        Element* root = RootEditableElementOf(p);
        shadow_ancestor = root ? root->OwnerShadowHost() : nullptr;
        p = IsAtomicNode(p.ComputeContainerNode())
                ? PositionTemplate<Strategy>::InParentAfterNode(
                      *p.ComputeContainerNode())
                : NextVisuallyDistinctCandidate(p);
        if (p.IsNull() && shadow_ancestor)
          p = PositionTemplate<Strategy>::BeforeNode(*shadow_ancestor);
      }
      const VisiblePositionTemplate<Strategy> next = CreateVisiblePosition(p);

      if (next.IsNull()) {
        // The selection crosses an Editing boundary.  This is a
        // programmer error in the editing code.  Happy debugging!
        NOTREACHED();
        *this = VisibleSelectionTemplate<Strategy>();
        return;
      }
      start_ = next.DeepEquivalent();
    }
  }

  // Correct the extent if necessary.
  if (base_editable_ancestor !=
      LowestEditableAncestor(extent_.ComputeContainerNode()))
    extent_ = base_is_first_ ? end_ : start_;
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsContentEditable() const {
  return IsEditablePosition(Start());
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::HasEditableStyle() const {
  return IsEditablePosition(Start());
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsContentRichlyEditable() const {
  return IsRichlyEditablePosition(ToPositionInDOMTree(Start()));
}

template <typename Strategy>
Element* VisibleSelectionTemplate<Strategy>::RootEditableElement() const {
  return RootEditableElementOf(Start());
}

template <typename Strategy>
static bool EqualSelectionsAlgorithm(
    const VisibleSelectionTemplate<Strategy>& selection1,
    const VisibleSelectionTemplate<Strategy>& selection2) {
  if (selection1.Affinity() != selection2.Affinity() ||
      selection1.IsDirectional() != selection2.IsDirectional())
    return false;

  if (selection1.IsNone())
    return selection2.IsNone();

  const VisibleSelectionTemplate<Strategy> selection_wrapper1(selection1);
  const VisibleSelectionTemplate<Strategy> selection_wrapper2(selection2);

  return selection_wrapper1.Start() == selection_wrapper2.Start() &&
         selection_wrapper1.End() == selection_wrapper2.End() &&
         selection_wrapper1.Base() == selection_wrapper2.Base() &&
         selection_wrapper1.Extent() == selection_wrapper2.Extent();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::operator==(
    const VisibleSelectionTemplate<Strategy>& other) const {
  return EqualSelectionsAlgorithm<Strategy>(*this, other);
}

template <typename Strategy>
DEFINE_TRACE(VisibleSelectionTemplate<Strategy>) {
  visitor->Trace(base_);
  visitor->Trace(extent_);
  visitor->Trace(start_);
  visitor->Trace(end_);
}

#ifndef NDEBUG

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::ShowTreeForThis() const {
  if (!Start().AnchorNode())
    return;
  LOG(INFO) << "\n"
            << Start()
                   .AnchorNode()
                   ->ToMarkedTreeString(Start().AnchorNode(), "S",
                                        End().AnchorNode(), "E")
                   .Utf8()
                   .data()
            << "start: " << Start().ToAnchorTypeAndOffsetString().Utf8().data()
            << "\n"
            << "end: " << End().ToAnchorTypeAndOffsetString().Utf8().data();
}

#endif

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::PrintTo(
    const VisibleSelectionTemplate<Strategy>& selection,
    std::ostream* ostream) {
  if (selection.IsNone()) {
    *ostream << "VisibleSelection()";
    return;
  }
  *ostream << "VisibleSelection(base: " << selection.Base()
           << " extent:" << selection.Extent()
           << " start: " << selection.Start() << " end: " << selection.End()
           << ' ' << selection.Affinity() << ' '
           << (selection.IsDirectional() ? "Directional" : "NonDirectional")
           << ')';
}

template class CORE_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    VisibleSelectionTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const VisibleSelection& selection) {
  VisibleSelection::PrintTo(selection, &ostream);
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const VisibleSelectionInFlatTree& selection) {
  VisibleSelectionInFlatTree::PrintTo(selection, &ostream);
  return ostream;
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::VisibleSelection& sel) {
  sel.ShowTreeForThis();
}

void showTree(const blink::VisibleSelection* sel) {
  if (sel)
    sel->ShowTreeForThis();
}

void showTree(const blink::VisibleSelectionInFlatTree& sel) {
  sel.ShowTreeForThis();
}

void showTree(const blink::VisibleSelectionInFlatTree* sel) {
  if (sel)
    sel->ShowTreeForThis();
}
#endif
