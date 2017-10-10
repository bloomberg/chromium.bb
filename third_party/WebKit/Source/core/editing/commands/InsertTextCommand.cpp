/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/InsertTextCommand.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLSpanElement.h"

namespace blink {

InsertTextCommand::InsertTextCommand(Document& document,
                                     const String& text,
                                     bool select_inserted_text,
                                     RebalanceType rebalance_type)
    : CompositeEditCommand(document),
      text_(text),
      select_inserted_text_(select_inserted_text),
      rebalance_type_(rebalance_type) {}

String InsertTextCommand::TextDataForInputEvent() const {
  return text_;
}

Position InsertTextCommand::PositionInsideTextNode(
    const Position& p,
    EditingState* editing_state) {
  Position pos = p;
  if (IsTabHTMLSpanElementTextNode(pos.AnchorNode())) {
    Text* text_node = GetDocument().CreateEditingTextNode("");
    InsertNodeAtTabSpanPosition(text_node, pos, editing_state);
    if (editing_state->IsAborted())
      return Position();
    return Position::FirstPositionInNode(*text_node);
  }

  // Prepare for text input by looking at the specified position.
  // It may be necessary to insert a text node to receive characters.
  if (!pos.ComputeContainerNode()->IsTextNode()) {
    Text* text_node = GetDocument().CreateEditingTextNode("");
    InsertNodeAt(text_node, pos, editing_state);
    if (editing_state->IsAborted())
      return Position();
    return Position::FirstPositionInNode(*text_node);
  }

  return pos;
}

void InsertTextCommand::SetEndingSelectionWithoutValidation(
    const Position& start_position,
    const Position& end_position) {
  // We could have inserted a part of composed character sequence,
  // so we are basically treating ending selection as a range to avoid
  // validation. <http://bugs.webkit.org/show_bug.cgi?id=15781>
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(start_position)
          .Extend(end_position)
          .SetIsDirectional(EndingSelection().IsDirectional())
          .Build()));
}

// This avoids the expense of a full fledged delete operation, and avoids a
// layout that typically results from text removal.
bool InsertTextCommand::PerformTrivialReplace(const String& text,
                                              bool select_inserted_text) {
  // We may need to manipulate neighboring whitespace if we're deleting text.
  // This case is tested in
  // InsertTextCommandTest_InsertEmptyTextAfterWhitespaceThatNeedsFixup.
  if (text.IsEmpty())
    return false;

  if (!EndingSelection().IsRange())
    return false;

  if (text.Contains('\t') || text.Contains(' ') || text.Contains('\n'))
    return false;

  Position start = EndingVisibleSelection().Start();
  Position end_position = ReplaceSelectedTextInNode(text);
  if (end_position.IsNull())
    return false;

  SetEndingSelectionWithoutValidation(start, end_position);
  if (select_inserted_text)
    return true;
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(EndingVisibleSelection().End())
          .SetIsDirectional(EndingSelection().IsDirectional())
          .Build()));
  return true;
}

bool InsertTextCommand::PerformOverwrite(const String& text,
                                         bool select_inserted_text) {
  Position start = EndingVisibleSelection().Start();
  if (start.IsNull() || !start.IsOffsetInAnchor() ||
      !start.ComputeContainerNode()->IsTextNode())
    return false;
  Text* text_node = ToText(start.ComputeContainerNode());
  if (!text_node)
    return false;

  unsigned count = std::min(
      text.length(), text_node->length() - start.OffsetInContainerNode());
  if (!count)
    return false;

  ReplaceTextInNode(text_node, start.OffsetInContainerNode(), count, text);

  Position end_position =
      Position(text_node, start.OffsetInContainerNode() + text.length());
  SetEndingSelectionWithoutValidation(start, end_position);
  if (select_inserted_text || EndingSelection().IsNone())
    return true;
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(EndingVisibleSelection().End())
          .SetIsDirectional(EndingSelection().IsDirectional())
          .Build()));
  return true;
}

void InsertTextCommand::DoApply(EditingState* editing_state) {
  DCHECK_EQ(text_.find('\n'), kNotFound);

  // TODO(editing-dev): We shouldn't construct an InsertTextCommand with none or
  // invalid selection.
  const VisibleSelection& visible_selection = EndingVisibleSelection();
  if (visible_selection.IsNone() ||
      !visible_selection.IsValidFor(GetDocument()))
    return;

  // Delete the current selection.
  // FIXME: This delete operation blows away the typing style.
  if (EndingSelection().IsRange()) {
    if (PerformTrivialReplace(text_, select_inserted_text_))
      return;
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
    bool end_of_selection_was_at_start_of_block =
        IsStartOfBlock(EndingVisibleSelection().VisibleEnd());
    DeleteSelection(editing_state, false, true, false, false);
    if (editing_state->IsAborted())
      return;
    // deleteSelection eventually makes a new endingSelection out of a Position.
    // If that Position doesn't have a layoutObject (e.g. it is on a <frameset>
    // in the DOM), the VisibleSelection cannot be canonicalized to anything
    // other than NoSelection. The rest of this function requires a real
    // endingSelection, so bail out.
    if (EndingSelection().IsNone())
      return;
    if (end_of_selection_was_at_start_of_block) {
      if (EditingStyle* typing_style =
              GetDocument().GetFrame()->GetEditor().TypingStyle())
        typing_style->RemoveBlockProperties();
    }
  } else if (GetDocument().GetFrame()->GetEditor().IsOverwriteModeEnabled()) {
    if (PerformOverwrite(text_, select_inserted_text_))
      return;
  }

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  Position start_position(EndingVisibleSelection().Start());

  Position placeholder;
  // We want to remove preserved newlines and brs that will collapse (and thus
  // become unnecessary) when content is inserted just before them.
  // FIXME: We shouldn't really have to do this, but removing placeholders is a
  // workaround for 9661.
  // If the caret is just before a placeholder, downstream will normalize the
  // caret to it.
  Position downstream(MostForwardCaretPosition(start_position));
  if (LineBreakExistsAtPosition(downstream)) {
    // FIXME: This doesn't handle placeholders at the end of anonymous blocks.
    VisiblePosition caret = CreateVisiblePosition(start_position);
    if (IsEndOfBlock(caret) && IsStartOfParagraph(caret))
      placeholder = downstream;
    // Don't remove the placeholder yet, otherwise the block we're inserting
    // into would collapse before we get a chance to insert into it.  We check
    // for a placeholder now, though, because doing so requires the creation of
    // a VisiblePosition, and if we did that post-insertion it would force a
    // layout.
  }

  // Insert the character at the leftmost candidate.
  start_position = MostBackwardCaretPosition(start_position);

  // It is possible for the node that contains startPosition to contain only
  // unrendered whitespace, and so deleteInsignificantText could remove it.
  // Save the position before the node in case that happens.
  DCHECK(start_position.ComputeContainerNode()) << start_position;
  Position position_before_start_node(
      Position::InParentBeforeNode(*start_position.ComputeContainerNode()));
  DeleteInsignificantText(start_position,
                          MostForwardCaretPosition(start_position));

  // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (!start_position.IsConnected())
    start_position = position_before_start_node;
  if (!IsVisuallyEquivalentCandidate(start_position))
    start_position = MostForwardCaretPosition(start_position);

  start_position =
      PositionAvoidingSpecialElementBoundary(start_position, editing_state);
  if (editing_state->IsAborted())
    return;

  Position end_position;

  if (text_ == "\t" && IsRichlyEditablePosition(start_position)) {
    end_position = InsertTab(start_position, editing_state);
    if (editing_state->IsAborted())
      return;
    start_position =
        PreviousPositionOf(end_position, PositionMoveType::kGraphemeCluster);
    if (placeholder.IsNotNull())
      RemovePlaceholderAt(placeholder);
  } else {
    // Make sure the document is set up to receive m_text
    start_position = PositionInsideTextNode(start_position, editing_state);
    if (editing_state->IsAborted())
      return;
    DCHECK(start_position.IsOffsetInAnchor()) << start_position;
    DCHECK(start_position.ComputeContainerNode()) << start_position;
    DCHECK(start_position.ComputeContainerNode()->IsTextNode())
        << start_position;
    if (placeholder.IsNotNull())
      RemovePlaceholderAt(placeholder);
    Text* text_node = ToText(start_position.ComputeContainerNode());
    const unsigned offset = start_position.OffsetInContainerNode();

    InsertTextIntoNode(text_node, offset, text_);
    end_position = Position(text_node, offset + text_.length());

    if (rebalance_type_ == kRebalanceLeadingAndTrailingWhitespaces) {
      // The insertion may require adjusting adjacent whitespace, if it is
      // present.
      RebalanceWhitespaceAt(end_position);
      // Rebalancing on both sides isn't necessary if we've inserted only
      // spaces.
      if (!text_.ContainsOnlyWhitespace())
        RebalanceWhitespaceAt(start_position);
    } else {
      DCHECK_EQ(rebalance_type_, kRebalanceAllWhitespaces);
      if (CanRebalance(start_position) && CanRebalance(end_position))
        RebalanceWhitespaceOnTextSubstring(
            text_node, start_position.OffsetInContainerNode(),
            end_position.OffsetInContainerNode());
    }
  }

  SetEndingSelectionWithoutValidation(start_position, end_position);

  // Handle the case where there is a typing style.
  if (EditingStyle* typing_style =
          GetDocument().GetFrame()->GetEditor().TypingStyle()) {
    typing_style->PrepareToApplyAt(end_position,
                                   EditingStyle::kPreserveWritingDirection);
    if (!typing_style->IsEmpty() && !EndingSelection().IsNone()) {
      ApplyStyle(typing_style, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  }

  if (!select_inserted_text_) {
    SelectionInDOMTree::Builder builder;
    const VisibleSelection& selection = EndingVisibleSelection();
    builder.SetAffinity(selection.Affinity());
    builder.SetIsDirectional(EndingSelection().IsDirectional());
    if (selection.End().IsNotNull())
      builder.Collapse(selection.End());
    SetEndingSelection(SelectionForUndoStep::From(builder.Build()));
  }
}

Position InsertTextCommand::InsertTab(const Position& pos,
                                      EditingState* editing_state) {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  Position insert_pos = CreateVisiblePosition(pos).DeepEquivalent();
  if (insert_pos.IsNull())
    return pos;

  Node* node = insert_pos.ComputeContainerNode();
  unsigned offset = node->IsTextNode() ? insert_pos.OffsetInContainerNode() : 0;

  // keep tabs coalesced in tab span
  if (IsTabHTMLSpanElementTextNode(node)) {
    Text* text_node = ToText(node);
    InsertTextIntoNode(text_node, offset, "\t");
    return Position(text_node, offset + 1);
  }

  // create new tab span
  HTMLSpanElement* span_element = CreateTabSpanElement(GetDocument());

  // place it
  if (!node->IsTextNode()) {
    InsertNodeAt(span_element, insert_pos, editing_state);
  } else {
    Text* text_node = ToText(node);
    if (offset >= text_node->length()) {
      InsertNodeAfter(span_element, text_node, editing_state);
    } else {
      // split node to make room for the span
      // NOTE: splitTextNode uses textNode for the
      // second node in the split, so we need to
      // insert the span before it.
      if (offset > 0)
        SplitTextNode(text_node, offset);
      InsertNodeBefore(span_element, text_node, editing_state);
    }
  }
  if (editing_state->IsAborted())
    return Position();

  // return the position following the new tab
  return Position::LastPositionInNode(*span_element);
}

}  // namespace blink
