/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/InsertLineBreakCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"

namespace blink {

using namespace HTMLNames;

InsertLineBreakCommand::InsertLineBreakCommand(Document& document)
    : CompositeEditCommand(document) {}

bool InsertLineBreakCommand::PreservesTypingStyle() const {
  return true;
}

// Whether we should insert a break element or a '\n'.
bool InsertLineBreakCommand::ShouldUseBreakElement(
    const Position& insertion_pos) {
  // An editing position like [input, 0] actually refers to the position before
  // the input element, and in that case we need to check the input element's
  // parent's layoutObject.
  Position p(insertion_pos.ParentAnchoredEquivalent());
  return IsRichlyEditablePosition(p) && p.AnchorNode()->GetLayoutObject() &&
         !p.AnchorNode()->GetLayoutObject()->Style()->PreserveNewline();
}

void InsertLineBreakCommand::DoApply(EditingState* editing_state) {
  DeleteSelection(editing_state);
  if (editing_state->IsAborted())
    return;

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  VisibleSelection selection = EndingSelection();
  if (!selection.IsNonOrphanedCaretOrRange())
    return;

  // TODO(xiaochengh): Stop storing VisiblePositions through mutations.
  VisiblePosition caret(selection.VisibleStart());
  // FIXME: If the node is hidden, we should still be able to insert text. For
  // now, we return to avoid a crash.
  // https://bugs.webkit.org/show_bug.cgi?id=40342
  if (caret.IsNull())
    return;

  Position pos(caret.DeepEquivalent());

  pos = PositionAvoidingSpecialElementBoundary(pos, editing_state);
  if (editing_state->IsAborted())
    return;

  pos = PositionOutsideTabSpan(pos);

  Node* node_to_insert = nullptr;
  if (ShouldUseBreakElement(pos))
    node_to_insert = HTMLBRElement::Create(GetDocument());
  else
    node_to_insert = GetDocument().createTextNode("\n");

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // FIXME: Need to merge text nodes when inserting just after or before text.

  if (IsEndOfParagraph(CreateVisiblePosition(caret.ToPositionWithAffinity())) &&
      !LineBreakExistsAtVisiblePosition(caret)) {
    bool need_extra_line_break = !isHTMLHRElement(*pos.AnchorNode()) &&
                                 !isHTMLTableElement(*pos.AnchorNode());

    InsertNodeAt(node_to_insert, pos, editing_state);
    if (editing_state->IsAborted())
      return;

    if (need_extra_line_break) {
      Node* extra_node;
      // TODO(tkent): Can we remove TextControlElement dependency?
      if (TextControlElement* text_control =
              EnclosingTextControl(node_to_insert)) {
        extra_node = text_control->CreatePlaceholderBreakElement();
        // The placeholder BR should be the last child.  There might be
        // empty Text nodes at |pos|.
        AppendNode(extra_node, node_to_insert->parentNode(), editing_state);
      } else {
        extra_node = node_to_insert->cloneNode(false);
        InsertNodeAfter(extra_node, node_to_insert, editing_state);
      }
      if (editing_state->IsAborted())
        return;
      node_to_insert = extra_node;
    }

    SetEndingSelection(SelectionInDOMTree::Builder()
                           .Collapse(Position::BeforeNode(*node_to_insert))
                           .SetIsDirectional(EndingSelection().IsDirectional())
                           .Build());
  } else if (pos.ComputeEditingOffset() <= CaretMinOffset(pos.AnchorNode())) {
    InsertNodeAt(node_to_insert, pos, editing_state);
    if (editing_state->IsAborted())
      return;
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    // Insert an extra br or '\n' if the just inserted one collapsed.
    if (!IsStartOfParagraph(VisiblePosition::BeforeNode(*node_to_insert))) {
      InsertNodeBefore(node_to_insert->cloneNode(false), node_to_insert,
                       editing_state);
      if (editing_state->IsAborted())
        return;
    }

    SetEndingSelection(
        SelectionInDOMTree::Builder()
            .Collapse(Position::InParentAfterNode(*node_to_insert))
            .SetIsDirectional(EndingSelection().IsDirectional())
            .Build());
    // If we're inserting after all of the rendered text in a text node, or into
    // a non-text node, a simple insertion is sufficient.
  } else if (!pos.AnchorNode()->IsTextNode() ||
             pos.ComputeOffsetInContainerNode() >=
                 CaretMaxOffset(pos.AnchorNode())) {
    InsertNodeAt(node_to_insert, pos, editing_state);
    if (editing_state->IsAborted())
      return;
    SetEndingSelection(
        SelectionInDOMTree::Builder()
            .Collapse(Position::InParentAfterNode(*node_to_insert))
            .SetIsDirectional(EndingSelection().IsDirectional())
            .Build());
  } else if (pos.AnchorNode()->IsTextNode()) {
    // Split a text node
    Text* text_node = ToText(pos.AnchorNode());
    SplitTextNode(text_node, pos.ComputeOffsetInContainerNode());
    InsertNodeBefore(node_to_insert, text_node, editing_state);
    if (editing_state->IsAborted())
      return;
    Position ending_position = Position::FirstPositionInNode(*text_node);

    // Handle whitespace that occurs after the split
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
    // TODO(yosin) |isRenderedCharacter()| should be removed, and we should
    // use |VisiblePosition::characterAfter()|.
    if (!IsRenderedCharacter(ending_position)) {
      Position position_before_text_node(
          Position::InParentBeforeNode(*text_node));
      // Clear out all whitespace and insert one non-breaking space
      DeleteInsignificantTextDownstream(ending_position);
      // Deleting insignificant whitespace will remove textNode if it contains
      // nothing but insignificant whitespace.
      if (text_node->isConnected()) {
        InsertTextIntoNode(text_node, 0, NonBreakingSpaceString());
      } else {
        Text* nbsp_node =
            GetDocument().createTextNode(NonBreakingSpaceString());
        InsertNodeAt(nbsp_node, position_before_text_node, editing_state);
        if (editing_state->IsAborted())
          return;
        ending_position = Position::FirstPositionInNode(*nbsp_node);
      }
    }

    SetEndingSelection(SelectionInDOMTree::Builder()
                           .Collapse(ending_position)
                           .SetIsDirectional(EndingSelection().IsDirectional())
                           .Build());
  }

  // Handle the case where there is a typing style.

  EditingStyle* typing_style =
      GetDocument().GetFrame()->GetEditor().TypingStyle();

  if (typing_style && !typing_style->IsEmpty()) {
    // Apply the typing style to the inserted line break, so that if the
    // selection leaves and then comes back, new input will have the right
    // style.
    // FIXME: We shouldn't always apply the typing style to the line break here,
    // see <rdar://problem/5794462>.
    ApplyStyle(typing_style, FirstPositionInOrBeforeNode(node_to_insert),
               LastPositionInOrAfterNode(node_to_insert), editing_state);
    if (editing_state->IsAborted())
      return;
    // Even though this applyStyle operates on a Range, it still sets an
    // endingSelection(). It tries to set a VisibleSelection around the content
    // it operated on. So, that VisibleSelection will either
    //   (a) select the line break we inserted, or it will
    //   (b) be a caret just before the line break (if the line break is at the
    //       end of a block it isn't selectable).
    // So, this next call sets the endingSelection() to a caret just after the
    // line break that we inserted, or just before it if it's at the end of a
    // block.
    SetEndingSelection(SelectionInDOMTree::Builder()
                           .Collapse(EndingSelection().End())
                           .Build());
  }

  RebalanceWhitespace();
}

}  // namespace blink
