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
      is_directional_(false) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const SelectionTemplate<Strategy>& selection,
    TextGranularity granularity)
    : affinity_(selection.Affinity()),
      selection_type_(kNoSelection),
      is_directional_(selection.IsDirectional()) {
  Validate(selection, granularity);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::Create(
    const SelectionTemplate<Strategy>& selection) {
  return VisibleSelectionTemplate(selection, TextGranularity::kCharacter);
}

VisibleSelection CreateVisibleSelection(const SelectionInDOMTree& selection) {
  return VisibleSelection::Create(selection);
}

VisibleSelectionInFlatTree CreateVisibleSelection(
    const SelectionInFlatTree& selection) {
  return VisibleSelectionInFlatTree::Create(selection);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::CreateWithGranularity(
    const SelectionTemplate<Strategy>& selection,
    TextGranularity granularity) {
  return VisibleSelectionTemplate(selection, granularity);
}

VisibleSelection CreateVisibleSelectionWithGranularity(
    const SelectionInDOMTree& selection,
    TextGranularity granularity) {
  return VisibleSelection::CreateWithGranularity(selection, granularity);
}

VisibleSelectionInFlatTree CreateVisibleSelectionWithGranularity(
    const SelectionInFlatTree& selection,
    TextGranularity granularity) {
  return VisibleSelectionInFlatTree::CreateWithGranularity(selection,
                                                           granularity);
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
      is_directional_(other.is_directional_) {}

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
  return *this;
}

template <typename Strategy>
SelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::AsSelection()
    const {
  typename SelectionTemplate<Strategy>::Builder builder;
  if (base_.IsNotNull())
    builder.SetBaseAndExtent(base_, extent_);
  return builder.SetAffinity(affinity_)
      .SetIsDirectional(is_directional_)
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
VisibleSelectionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::AppendTrailingWhitespace() const {
  if (IsNone())
    return *this;
  if (!IsRange())
    return *this;
  const PositionTemplate<Strategy>& new_end = SkipWhitespace(end_);
  if (end_ == new_end)
    return *this;
  VisibleSelectionTemplate<Strategy> result = *this;
  result.end_ = new_end;
  return result;
}

template <typename Strategy>
static SelectionTemplate<Strategy> CanonicalizeSelection(
    const SelectionTemplate<Strategy>& selection) {
  if (selection.IsNone())
    return SelectionTemplate<Strategy>();
  const PositionTemplate<Strategy>& base =
      CreateVisiblePosition(selection.Base(), selection.Affinity())
          .DeepEquivalent();
  if (selection.IsCaret()) {
    if (base.IsNull())
      return SelectionTemplate<Strategy>();
    return
        typename SelectionTemplate<Strategy>::Builder().Collapse(base).Build();
  }
  const PositionTemplate<Strategy>& extent =
      CreateVisiblePosition(selection.Extent(), selection.Affinity())
          .DeepEquivalent();
  if (base.IsNotNull() && extent.IsNotNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .SetBaseAndExtent(base, extent)
        .Build();
  }
  if (base.IsNotNull()) {
    return
        typename SelectionTemplate<Strategy>::Builder().Collapse(base).Build();
  }
  if (extent.IsNotNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .Collapse(extent)
        .Build();
  }
  return SelectionTemplate<Strategy>();
}

template <typename Strategy>
static PositionTemplate<Strategy> ComputeStartRespectingGranularityAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& passed_start,
    TextGranularity granularity) {
  DCHECK(passed_start.IsNotNull());

  switch (granularity) {
    case TextGranularity::kCharacter:
      // Don't do any expansion.
      return passed_start.GetPosition();
    case TextGranularity::kWord: {
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
    case TextGranularity::kSentence:
      return StartOfSentence(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
    case TextGranularity::kLine:
      return StartOfLine(CreateVisiblePosition(passed_start)).DeepEquivalent();
    case TextGranularity::kLineBoundary:
      return StartOfLine(CreateVisiblePosition(passed_start)).DeepEquivalent();
    case TextGranularity::kParagraph: {
      const VisiblePositionTemplate<Strategy> pos =
          CreateVisiblePosition(passed_start);
      if (IsStartOfLine(pos) && IsEndOfEditableOrNonEditableContent(pos))
        return StartOfParagraph(PreviousPositionOf(pos)).DeepEquivalent();
      return StartOfParagraph(pos).DeepEquivalent();
    }
    case TextGranularity::kDocumentBoundary:
      return StartOfDocument(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
    case TextGranularity::kParagraphBoundary:
      return StartOfParagraph(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
    case TextGranularity::kSentenceBoundary:
      return StartOfSentence(CreateVisiblePosition(passed_start))
          .DeepEquivalent();
  }

  NOTREACHED();
  return passed_start.GetPosition();
}

static Position ComputeStartRespectingGranularity(
    const PositionWithAffinity& start,
    TextGranularity granularity) {
  return ComputeStartRespectingGranularityAlgorithm(start, granularity);
}

PositionInFlatTree ComputeStartRespectingGranularity(
    const PositionInFlatTreeWithAffinity& start,
    TextGranularity granularity) {
  return ComputeStartRespectingGranularityAlgorithm(start, granularity);
}

template <typename Strategy>
static PositionTemplate<Strategy> ComputeEndRespectingGranularityAlgorithm(
    const PositionTemplate<Strategy>& start,
    const PositionWithAffinityTemplate<Strategy>& passed_end,
    TextGranularity granularity) {
  DCHECK(passed_end.IsNotNull());

  switch (granularity) {
    case TextGranularity::kCharacter:
      // Don't do any expansion.
      return passed_end.GetPosition();
    case TextGranularity::kWord: {
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
    case TextGranularity::kSentence:
      return EndOfSentence(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case TextGranularity::kLine: {
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
    case TextGranularity::kLineBoundary:
      return EndOfLine(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case TextGranularity::kParagraph: {
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
    case TextGranularity::kDocumentBoundary:
      return EndOfDocument(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case TextGranularity::kParagraphBoundary:
      return EndOfParagraph(CreateVisiblePosition(passed_end)).DeepEquivalent();
    case TextGranularity::kSentenceBoundary:
      return EndOfSentence(CreateVisiblePosition(passed_end)).DeepEquivalent();
  }
  NOTREACHED();
  return passed_end.GetPosition();
}

static Position ComputeEndRespectingGranularity(const Position& start,
                                                const PositionWithAffinity& end,
                                                TextGranularity granularity) {
  return ComputeEndRespectingGranularityAlgorithm(start, end, granularity);
}

PositionInFlatTree ComputeEndRespectingGranularity(
    const PositionInFlatTree& start,
    const PositionInFlatTreeWithAffinity& end,
    TextGranularity granularity) {
  return ComputeEndRespectingGranularityAlgorithm(start, end, granularity);
}

// TODO(editing-dev): Once we move all static functions into anonymous
// namespace, we should get rid of this forward declaration.
template <typename Strategy>
static EphemeralRangeTemplate<Strategy>
AdjustSelectionToAvoidCrossingEditingBoundaries(
    const EphemeralRangeTemplate<Strategy>&,
    const PositionTemplate<Strategy>& base);

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::Validate(
    const SelectionTemplate<Strategy>& passed_selection,
    TextGranularity granularity) {
  DCHECK(!NeedsLayoutTreeUpdate(base_));
  DCHECK(!NeedsLayoutTreeUpdate(extent_));
  // TODO(xiaochengh): Add a DocumentLifecycle::DisallowTransitionScope here.

  const SelectionTemplate<Strategy>& canonicalized_selection =
      CanonicalizeSelection(passed_selection);

  if (canonicalized_selection.IsNone()) {
    base_ = extent_ = start_ = end_ = PositionTemplate<Strategy>();
    base_is_first_ = true;
    selection_type_ = kNoSelection;
    affinity_ = TextAffinity::kDownstream;
    return;
  }

  base_ = canonicalized_selection.Base();
  extent_ = canonicalized_selection.Extent();
  base_is_first_ = base_ <= extent_;

  const PositionTemplate<Strategy> start = base_is_first_ ? base_ : extent_;
  const PositionTemplate<Strategy> new_start =
      ComputeStartRespectingGranularity(
          PositionWithAffinityTemplate<Strategy>(start, affinity_),
          granularity);
  const PositionTemplate<Strategy> expanded_start =
      new_start.IsNotNull() ? new_start : start;

  const PositionTemplate<Strategy> end = base_is_first_ ? extent_ : base_;
  const PositionTemplate<Strategy> new_end = ComputeEndRespectingGranularity(
      expanded_start, PositionWithAffinityTemplate<Strategy>(end, affinity_),
      granularity);
  const PositionTemplate<Strategy> expanded_end =
      new_end.IsNotNull() ? new_end : end;

  const EphemeralRangeTemplate<Strategy> expanded_range(expanded_start,
                                                        expanded_end);

  const EphemeralRangeTemplate<Strategy> shadow_adjusted_range =
      base_is_first_
          ? EphemeralRangeTemplate<Strategy>(
                expanded_range.StartPosition(),
                SelectionAdjuster::
                    AdjustSelectionEndToAvoidCrossingShadowBoundaries(
                        expanded_range))
          : EphemeralRangeTemplate<Strategy>(
                SelectionAdjuster::
                    AdjustSelectionStartToAvoidCrossingShadowBoundaries(
                        expanded_range),
                expanded_range.EndPosition());

  const EphemeralRangeTemplate<Strategy> editing_adjusted_range =
      AdjustSelectionToAvoidCrossingEditingBoundaries(shadow_adjusted_range,
                                                      base_);
  selection_type_ = ComputeSelectionType(editing_adjusted_range.StartPosition(),
                                         editing_adjusted_range.EndPosition());

  // "Constrain" the selection to be the smallest equivalent range of
  // nodes. This is a somewhat arbitrary choice, but experience shows that
  // it is useful to make to make the selection "canonical" (if only for
  // purposes of comparing selections). This is an ideal point of the code
  // to do this operation, since all selection changes that result in a
  // RANGE come through here before anyone uses it.
  // TODO(yosin) Canonicalizing is good, but haven't we already done it
  // (when we set these two positions to |VisiblePosition|
  // |DeepEquivalent()|s above)?
  const EphemeralRangeTemplate<Strategy> range =
      selection_type_ == kRangeSelection
          ? EphemeralRangeTemplate<Strategy>(
                MostForwardCaretPosition(
                    editing_adjusted_range.StartPosition()),
                MostBackwardCaretPosition(editing_adjusted_range.EndPosition()))
          : editing_adjusted_range;
  if (selection_type_ != kCaretSelection) {
    // Affinity only makes sense for a caret
    affinity_ = TextAffinity::kDownstream;
  }
  start_ = range.StartPosition();
  end_ = range.EndPosition();
  base_ = base_is_first_ ? start_ : end_;
  extent_ = base_is_first_ ? end_ : start_;
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
VisibleSelectionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::CreateWithoutValidationDeprecated(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent,
    TextAffinity affinity) {
  DCHECK(base.IsNotNull());
  DCHECK(extent.IsNotNull());

  VisibleSelectionTemplate<Strategy> visible_selection;
  visible_selection.base_ = base;
  visible_selection.extent_ = extent;
  visible_selection.base_is_first_ = base.CompareTo(extent) <= 0;
  if (visible_selection.base_is_first_) {
    visible_selection.start_ = base;
    visible_selection.end_ = extent;
  } else {
    visible_selection.start_ = extent;
    visible_selection.end_ = base;
  }
  if (base == extent) {
    visible_selection.selection_type_ = kCaretSelection;
    visible_selection.affinity_ = affinity;
    return visible_selection;
  }
  // Since |affinity_| for non-|CaretSelection| is always |kDownstream|,
  // we should keep this invariant. Note: This function can be called with
  // |affinity_| is |kUpstream|.
  visible_selection.selection_type_ = kRangeSelection;
  visible_selection.affinity_ = TextAffinity::kDownstream;
  return visible_selection;
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

// Returns true if |position| is editable or its lowest editable root is not
// |base_editable_ancestor|.
template <typename Strategy>
static bool ShouldContinueSearchEditingBoundary(
    const PositionTemplate<Strategy>& position,
    Element* base_editable_ancestor) {
  if (position.IsNull())
    return false;
  if (IsEditablePosition(position))
    return true;
  return LowestEditableAncestor(position.ComputeContainerNode()) !=
         base_editable_ancestor;
}

template <typename Strategy>
static bool ShouldAdjustPositionToAvoidCrossingEditingBoundaries(
    const PositionTemplate<Strategy>& position,
    const ContainerNode* editable_root,
    const Element* base_editable_ancestor) {
  if (editable_root)
    return true;
  Element* const editable_ancestor =
      LowestEditableAncestor(position.ComputeContainerNode());
  return editable_ancestor != base_editable_ancestor;
}

// The selection ends in editable content or non-editable content inside a
// different editable ancestor, move backward until non-editable content inside
// the same lowest editable ancestor is reached.
template <typename Strategy>
PositionTemplate<Strategy> AdjustSelectionEndToAvoidCrossingEditingBoundaries(
    const PositionTemplate<Strategy>& end,
    ContainerNode* end_root,
    Element* base_editable_ancestor) {
  if (ShouldAdjustPositionToAvoidCrossingEditingBoundaries(
          end, end_root, base_editable_ancestor)) {
    PositionTemplate<Strategy> position =
        PreviousVisuallyDistinctCandidate(end);
    Element* shadow_ancestor = end_root ? end_root->OwnerShadowHost() : nullptr;
    if (position.IsNull() && shadow_ancestor)
      position = PositionTemplate<Strategy>::AfterNode(*shadow_ancestor);
    while (
        ShouldContinueSearchEditingBoundary(position, base_editable_ancestor)) {
      Element* root = RootEditableElementOf(position);
      shadow_ancestor = root ? root->OwnerShadowHost() : nullptr;
      position = IsAtomicNode(position.ComputeContainerNode())
                     ? PositionTemplate<Strategy>::InParentBeforeNode(
                           *position.ComputeContainerNode())
                     : PreviousVisuallyDistinctCandidate(position);
      if (position.IsNull() && shadow_ancestor)
        position = PositionTemplate<Strategy>::AfterNode(*shadow_ancestor);
    }
    return CreateVisiblePosition(position).DeepEquivalent();
  }
  return end;
}

// The selection starts in editable content or non-editable content inside a
// different editable ancestor, move forward until non-editable content inside
// the same lowest editable ancestor is reached.
template <typename Strategy>
PositionTemplate<Strategy> AdjustSelectionStartToAvoidCrossingEditingBoundaries(
    const PositionTemplate<Strategy>& start,
    ContainerNode* start_root,
    Element* base_editable_ancestor) {
  if (ShouldAdjustPositionToAvoidCrossingEditingBoundaries(
          start, start_root, base_editable_ancestor)) {
    PositionTemplate<Strategy> position = NextVisuallyDistinctCandidate(start);
    Element* shadow_ancestor =
        start_root ? start_root->OwnerShadowHost() : nullptr;
    if (position.IsNull() && shadow_ancestor)
      position = PositionTemplate<Strategy>::BeforeNode(*shadow_ancestor);
    while (
        ShouldContinueSearchEditingBoundary(position, base_editable_ancestor)) {
      Element* root = RootEditableElementOf(position);
      shadow_ancestor = root ? root->OwnerShadowHost() : nullptr;
      position = IsAtomicNode(position.ComputeContainerNode())
                     ? PositionTemplate<Strategy>::InParentAfterNode(
                           *position.ComputeContainerNode())
                     : NextVisuallyDistinctCandidate(position);
      if (position.IsNull() && shadow_ancestor)
        position = PositionTemplate<Strategy>::BeforeNode(*shadow_ancestor);
    }
    return CreateVisiblePosition(position).DeepEquivalent();
  }
  return start;
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy>
AdjustSelectionToAvoidCrossingEditingBoundaries(
    const EphemeralRangeTemplate<Strategy>& range,
    const PositionTemplate<Strategy>& base) {
  DCHECK(base.IsNotNull());
  DCHECK(range.IsNotNull());

  ContainerNode* base_root = HighestEditableRoot(base);
  ContainerNode* start_root = HighestEditableRoot(range.StartPosition());
  ContainerNode* end_root = HighestEditableRoot(range.EndPosition());

  Element* base_editable_ancestor =
      LowestEditableAncestor(base.ComputeContainerNode());

  // The base, start and end are all in the same region.  No adjustment
  // necessary.
  if (base_root == start_root && base_root == end_root)
    return range;

  // The selection is based in editable content.
  if (base_root) {
    // If the start is outside the base's editable root, cap it at the start of
    // that root.
    // If the start is in non-editable content that is inside the base's
    // editable root, put it at the first editable position after start inside
    // the base's editable root.
    PositionTemplate<Strategy> start = range.StartPosition();
    if (start_root != base_root) {
      const VisiblePositionTemplate<Strategy> first =
          FirstEditableVisiblePositionAfterPositionInRoot(start, *base_root);
      start = first.DeepEquivalent();
      if (start.IsNull()) {
        NOTREACHED();
        return {};
      }
    }
    // If the end is outside the base's editable root, cap it at the end of that
    // root.
    // If the end is in non-editable content that is inside the base's root, put
    // it at the last editable position before the end inside the base's root.
    PositionTemplate<Strategy> end = range.EndPosition();
    if (end_root != base_root) {
      const VisiblePositionTemplate<Strategy> last =
          LastEditableVisiblePositionBeforePositionInRoot(end, *base_root);
      end = last.DeepEquivalent();
      if (end.IsNull())
        end = start;
    }
    return {start, end};
  } else {
    // The selection is based in non-editable content.
    // FIXME: Non-editable pieces inside editable content should be atomic, in
    // the same way that editable pieces in non-editable content are atomic.
    const PositionTemplate<Strategy>& end =
        AdjustSelectionEndToAvoidCrossingEditingBoundaries(
            range.EndPosition(), end_root, base_editable_ancestor);
    if (end.IsNull()) {
      // The selection crosses an Editing boundary.  This is a
      // programmer error in the editing code.  Happy debugging!
      NOTREACHED();
      return {};
    }

    const PositionTemplate<Strategy>& start =
        AdjustSelectionStartToAvoidCrossingEditingBoundaries(
            range.StartPosition(), start_root, base_editable_ancestor);
    if (start.IsNull()) {
      // The selection crosses an Editing boundary.  This is a
      // programmer error in the editing code.  Happy debugging!
      NOTREACHED();
      return {};
    }
    return {start, end};
  }
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
