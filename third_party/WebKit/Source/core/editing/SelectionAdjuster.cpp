/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
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

#include "core/editing/SelectionAdjuster.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"

namespace blink {

namespace {

Node* EnclosingShadowHost(Node* node) {
  for (Node* runner = node; runner;
       runner = FlatTreeTraversal::Parent(*runner)) {
    if (IsShadowHost(runner))
      return runner;
  }
  return nullptr;
}

bool IsEnclosedBy(const PositionInFlatTree& position, const Node& node) {
  DCHECK(position.IsNotNull());
  Node* anchor_node = position.AnchorNode();
  if (anchor_node == node)
    return !position.IsAfterAnchor() && !position.IsBeforeAnchor();

  return FlatTreeTraversal::IsDescendantOf(*anchor_node, node);
}

bool IsSelectionBoundary(const Node& node) {
  return IsHTMLTextAreaElement(node) || IsHTMLInputElement(node) ||
         IsHTMLSelectElement(node);
}

Node* EnclosingShadowHostForStart(const PositionInFlatTree& position) {
  Node* node = position.NodeAsRangeFirstNode();
  if (!node)
    return nullptr;
  Node* shadow_host = EnclosingShadowHost(node);
  if (!shadow_host)
    return nullptr;
  if (!IsEnclosedBy(position, *shadow_host))
    return nullptr;
  return IsSelectionBoundary(*shadow_host) ? shadow_host : nullptr;
}

Node* EnclosingShadowHostForEnd(const PositionInFlatTree& position) {
  Node* node = position.NodeAsRangeLastNode();
  if (!node)
    return nullptr;
  Node* shadow_host = EnclosingShadowHost(node);
  if (!shadow_host)
    return nullptr;
  if (!IsEnclosedBy(position, *shadow_host))
    return nullptr;
  return IsSelectionBoundary(*shadow_host) ? shadow_host : nullptr;
}

PositionInFlatTree AdjustPositionInFlatTreeForStart(
    const PositionInFlatTree& position,
    Node* shadow_host) {
  if (IsEnclosedBy(position, *shadow_host)) {
    if (position.IsBeforeChildren())
      return PositionInFlatTree::BeforeNode(*shadow_host);
    return PositionInFlatTree::AfterNode(*shadow_host);
  }

  // We use |firstChild|'s after instead of beforeAllChildren for backward
  // compatibility. The positions are same but the anchors would be different,
  // and selection painting uses anchor nodes.
  if (Node* first_child = FlatTreeTraversal::FirstChild(*shadow_host))
    return PositionInFlatTree::BeforeNode(*first_child);
  return PositionInFlatTree();
}

Position AdjustPositionForEnd(const Position& current_position,
                              Node* start_container_node) {
  TreeScope& tree_scope = start_container_node->GetTreeScope();

  DCHECK(current_position.ComputeContainerNode()->GetTreeScope() != tree_scope);

  if (Node* ancestor = tree_scope.AncestorInThisScope(
          current_position.ComputeContainerNode())) {
    if (ancestor->contains(start_container_node))
      return Position::AfterNode(*ancestor);
    return Position::BeforeNode(*ancestor);
  }

  if (Node* last_child = tree_scope.RootNode().lastChild())
    return Position::AfterNode(*last_child);

  return Position();
}

PositionInFlatTree AdjustPositionInFlatTreeForEnd(
    const PositionInFlatTree& position,
    Node* shadow_host) {
  if (IsEnclosedBy(position, *shadow_host)) {
    if (position.IsAfterChildren())
      return PositionInFlatTree::AfterNode(*shadow_host);
    return PositionInFlatTree::BeforeNode(*shadow_host);
  }

  // We use |lastChild|'s after instead of afterAllChildren for backward
  // compatibility. The positions are same but the anchors would be different,
  // and selection painting uses anchor nodes.
  if (Node* last_child = FlatTreeTraversal::LastChild(*shadow_host))
    return PositionInFlatTree::AfterNode(*last_child);
  return PositionInFlatTree();
}

Position AdjustPositionForStart(const Position& current_position,
                                Node* end_container_node) {
  TreeScope& tree_scope = end_container_node->GetTreeScope();

  DCHECK(current_position.ComputeContainerNode()->GetTreeScope() != tree_scope);

  if (Node* ancestor = tree_scope.AncestorInThisScope(
          current_position.ComputeContainerNode())) {
    if (ancestor->contains(end_container_node))
      return Position::BeforeNode(*ancestor);
    return Position::AfterNode(*ancestor);
  }

  if (Node* first_child = tree_scope.RootNode().firstChild())
    return Position::BeforeNode(*first_child);

  return Position();
}

// TODO(hajimehoshi): Checking treeScope is wrong when a node is
// distributed, but we leave it as it is for backward compatibility.
bool IsCrossingShadowBoundaries(const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  return range.StartPosition().AnchorNode()->GetTreeScope() !=
         range.EndPosition().AnchorNode()->GetTreeScope();
}

}  // namespace

Position SelectionAdjuster::AdjustSelectionStartToAvoidCrossingShadowBoundaries(
    const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  if (!IsCrossingShadowBoundaries(range))
    return range.StartPosition();
  return AdjustPositionForStart(range.StartPosition(),
                                range.EndPosition().ComputeContainerNode());
}

Position SelectionAdjuster::AdjustSelectionEndToAvoidCrossingShadowBoundaries(
    const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  if (!IsCrossingShadowBoundaries(range))
    return range.EndPosition();
  return AdjustPositionForEnd(range.EndPosition(),
                              range.StartPosition().ComputeContainerNode());
}

PositionInFlatTree
SelectionAdjuster::AdjustSelectionStartToAvoidCrossingShadowBoundaries(
    const EphemeralRangeInFlatTree& range) {
  Node* const shadow_host_start =
      EnclosingShadowHostForStart(range.StartPosition());
  Node* const shadow_host_end = EnclosingShadowHostForEnd(range.EndPosition());
  if (shadow_host_start == shadow_host_end)
    return range.StartPosition();
  Node* const shadow_host =
      shadow_host_end ? shadow_host_end : shadow_host_start;
  return AdjustPositionInFlatTreeForStart(range.StartPosition(), shadow_host);
}

PositionInFlatTree
SelectionAdjuster::AdjustSelectionEndToAvoidCrossingShadowBoundaries(
    const EphemeralRangeInFlatTree& range) {
  Node* const shadow_host_start =
      EnclosingShadowHostForStart(range.StartPosition());
  Node* const shadow_host_end = EnclosingShadowHostForEnd(range.EndPosition());
  if (shadow_host_start == shadow_host_end)
    return range.EndPosition();
  Node* const shadow_host =
      shadow_host_start ? shadow_host_start : shadow_host_end;
  return AdjustPositionInFlatTreeForEnd(range.EndPosition(), shadow_host);
}

class GranularityAdjuster final {
  STATIC_ONLY(GranularityAdjuster);

 public:
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
        // (kPreviousWordIfOnBoundary).
        // Edge case: If the caret is after the last word in a paragraph, select
        // from the the end of the last word to the line break (also
        // kNextWordIfOnBoundary);
        const VisiblePositionTemplate<Strategy> visible_start =
            CreateVisiblePosition(passed_start);
        return StartOfWord(visible_start, ChooseWordSide(visible_start))
            .DeepEquivalent();
      }
      case TextGranularity::kSentence:
        return StartOfSentence(CreateVisiblePosition(passed_start))
            .DeepEquivalent();
      case TextGranularity::kLine:
        return StartOfLine(CreateVisiblePosition(passed_start))
            .DeepEquivalent();
      case TextGranularity::kLineBoundary:
        return StartOfLine(CreateVisiblePosition(passed_start))
            .DeepEquivalent();
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
        // (|kPreviousWordIfOnBoundary|).
        // Edge case: If the caret is after the last word in a paragraph, select
        // from the the end of the last word to the line break (also
        // |kNextWordIfOnBoundary|);
        const VisiblePositionTemplate<Strategy> original_end =
            CreateVisiblePosition(passed_end);
        const VisiblePositionTemplate<Strategy> word_end =
            EndOfWord(original_end, ChooseWordSide(original_end));
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
        return EndOfSentence(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
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
        return EndOfDocument(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
      case TextGranularity::kParagraphBoundary:
        return EndOfParagraph(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
      case TextGranularity::kSentenceBoundary:
        return EndOfSentence(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
    }
    NOTREACHED();
    return passed_end.GetPosition();
  }

  template <typename Strategy>
  static SelectionTemplate<Strategy> AdjustSelection(
      const SelectionTemplate<Strategy>& canonicalized_selection,
      TextGranularity granularity) {
    const TextAffinity affinity = canonicalized_selection.Affinity();

    const PositionTemplate<Strategy> start =
        canonicalized_selection.ComputeStartPosition();
    const PositionTemplate<Strategy> new_start =
        ComputeStartRespectingGranularityAlgorithm(
            PositionWithAffinityTemplate<Strategy>(start, affinity),
            granularity);
    const PositionTemplate<Strategy> expanded_start =
        new_start.IsNotNull() ? new_start : start;

    const PositionTemplate<Strategy> end =
        canonicalized_selection.ComputeEndPosition();
    const PositionTemplate<Strategy> new_end =
        ComputeEndRespectingGranularityAlgorithm(
            expanded_start,
            PositionWithAffinityTemplate<Strategy>(end, affinity), granularity);
    const PositionTemplate<Strategy> expanded_end =
        new_end.IsNotNull() ? new_end : end;

    const EphemeralRangeTemplate<Strategy> expanded_range(expanded_start,
                                                          expanded_end);
    typename SelectionTemplate<Strategy>::Builder builder;
    return canonicalized_selection.IsBaseFirst()
               ? builder.SetAsForwardSelection(expanded_range).Build()
               : builder.SetAsBackwardSelection(expanded_range).Build();
  }

 private:
  template <typename Strategy>
  static EWordSide ChooseWordSide(
      const VisiblePositionTemplate<Strategy>& position) {
    return IsEndOfEditableOrNonEditableContent(position) ||
                   (IsEndOfLine(position) && !IsStartOfLine(position) &&
                    !IsEndOfParagraph(position))
               ? kPreviousWordIfOnBoundary
               : kNextWordIfOnBoundary;
  }
};

PositionInFlatTree ComputeStartRespectingGranularity(
    const PositionInFlatTreeWithAffinity& start,
    TextGranularity granularity) {
  return GranularityAdjuster::ComputeStartRespectingGranularityAlgorithm(
      start, granularity);
}

PositionInFlatTree ComputeEndRespectingGranularity(
    const PositionInFlatTree& start,
    const PositionInFlatTreeWithAffinity& end,
    TextGranularity granularity) {
  return GranularityAdjuster::ComputeEndRespectingGranularityAlgorithm(
      start, end, granularity);
}

SelectionInDOMTree SelectionAdjuster::AdjustSelectionRespectingGranularity(
    const SelectionInDOMTree& selection,
    TextGranularity granularity) {
  return GranularityAdjuster::AdjustSelection(selection, granularity);
}

SelectionInFlatTree SelectionAdjuster::AdjustSelectionRespectingGranularity(
    const SelectionInFlatTree& selection,
    TextGranularity granularity) {
  return GranularityAdjuster::AdjustSelection(selection, granularity);
}

}  // namespace blink
