/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include "core/editing/commands/TypingCommand.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/events/ScopedEventQueue.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/SelectionModifier.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/BreakBlockquoteCommand.h"
#include "core/editing/commands/DeleteSelectionCommand.h"
#include "core/editing/commands/InsertIncrementalTextCommand.h"
#include "core/editing/commands/InsertLineBreakCommand.h"
#include "core/editing/commands/InsertParagraphSeparatorCommand.h"
#include "core/editing/commands/InsertTextCommand.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/TextEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBRElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutObject.h"

namespace blink {

namespace {

bool IsValidDocument(const Document& document) {
  return document.GetFrame() && document.GetFrame()->GetDocument() == &document;
}

String DispatchBeforeTextInsertedEvent(const String& text,
                                       const VisibleSelection& selection,
                                       EditingState* editing_state) {
  Node* start_node = selection.Start().ComputeContainerNode();
  if (!start_node || !RootEditableElement(*start_node))
    return text;

  // Send BeforeTextInsertedEvent. The event handler will update text if
  // necessary.
  const Document& document = start_node->GetDocument();
  BeforeTextInsertedEvent* evt = BeforeTextInsertedEvent::Create(text);
  RootEditableElement(*start_node)->DispatchEvent(evt);
  if (IsValidDocument(document) && selection.IsValidFor(document))
    return evt->GetText();
  // editing/inserting/webkitBeforeTextInserted-removes-frame.html
  // and
  // editing/inserting/webkitBeforeTextInserted-disconnects-selection.html
  // reaches here.
  editing_state->Abort();
  return String();
}

DispatchEventResult DispatchTextInputEvent(LocalFrame* frame,
                                           const String& text,
                                           EditingState* editing_state) {
  const Document& document = *frame->GetDocument();
  Element* target = document.FocusedElement();
  if (!target)
    return DispatchEventResult::kCanceledBeforeDispatch;

  // Send TextInputEvent. Unlike BeforeTextInsertedEvent, there is no need to
  // update text for TextInputEvent as it doesn't have the API to modify text.
  TextEvent* event = TextEvent::Create(frame->DomWindow(), text,
                                       kTextEventInputIncrementalInsertion);
  event->SetUnderlyingEvent(nullptr);
  DispatchEventResult result = target->DispatchEvent(event);
  if (IsValidDocument(document))
    return result;
  // editing/inserting/insert-text-remove-iframe-on-textInput-event.html
  // reaches here.
  editing_state->Abort();
  return result;
}

PlainTextRange GetSelectionOffsets(LocalFrame* frame) {
  EphemeralRange range = FirstEphemeralRangeOf(
      frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated());
  if (range.IsNull())
    return PlainTextRange();
  ContainerNode* const editable = RootEditableElementOrTreeScopeRootNodeOf(
      frame->Selection().ComputeVisibleSelectionInDOMTree().Base());
  DCHECK(editable);
  return PlainTextRange::Create(*editable, range);
}

SelectionInDOMTree CreateSelection(const size_t start,
                                   const size_t end,
                                   const bool is_directional,
                                   Element* element) {
  const EphemeralRange& start_range =
      PlainTextRange(0, static_cast<int>(start)).CreateRange(*element);
  DCHECK(start_range.IsNotNull());
  const Position& start_position = start_range.EndPosition();

  const EphemeralRange& end_range =
      PlainTextRange(0, static_cast<int>(end)).CreateRange(*element);
  DCHECK(end_range.IsNotNull());
  const Position& end_position = end_range.EndPosition();

  const SelectionInDOMTree& selection =
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(start_position, end_position)
          .SetIsDirectional(is_directional)
          .Build();
  return selection;
}

bool CanAppendNewLineFeedToSelection(const VisibleSelection& selection,
                                     EditingState* editing_state) {
  Element* element = selection.RootEditableElement();
  if (!element)
    return false;

  const Document& document = element->GetDocument();
  BeforeTextInsertedEvent* event =
      BeforeTextInsertedEvent::Create(String("\n"));
  element->DispatchEvent(event);
  // event may invalidate frame or selection
  if (IsValidDocument(document) && selection.IsValidFor(document))
    return event->GetText().length();
  // editing/inserting/webkitBeforeTextInserted-removes-frame.html
  // and
  // editing/inserting/webkitBeforeTextInserted-disconnects-selection.html
  // reaches here.
  editing_state->Abort();
  return false;
}

}  // anonymous namespace

using namespace HTMLNames;

TypingCommand::TypingCommand(Document& document,
                             ETypingCommand command_type,
                             const String& text_to_insert,
                             Options options,
                             TextGranularity granularity,
                             TextCompositionType composition_type)
    : CompositeEditCommand(document),
      command_type_(command_type),
      text_to_insert_(text_to_insert),
      open_for_more_typing_(true),
      select_inserted_text_(options & kSelectInsertedText),
      smart_delete_(options & kSmartDelete),
      granularity_(granularity),
      composition_type_(composition_type),
      kill_ring_(options & kKillRing),
      opened_by_backward_delete_(false),
      should_retain_autocorrection_indicator_(options &
                                              kRetainAutocorrectionIndicator),
      should_prevent_spell_checking_(options & kPreventSpellChecking) {
  UpdatePreservesTypingStyle(command_type_);
}

void TypingCommand::DeleteSelection(Document& document, Options options) {
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);

  if (!frame->Selection()
           .ComputeVisibleSelectionInDOMTreeDeprecated()
           .IsRange())
    return;

  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame)) {
    UpdateSelectionIfDifferentFromCurrentSelection(last_typing_command, frame);

    last_typing_command->SetShouldPreventSpellChecking(options &
                                                       kPreventSpellChecking);
    // InputMethodController uses this function to delete composition
    // selection.  It won't be aborted.
    last_typing_command->DeleteSelection(options & kSmartDelete,
                                         ASSERT_NO_EDITING_ABORT);
    return;
  }

  TypingCommand::Create(document, kDeleteSelection, "", options)->Apply();
}

void TypingCommand::DeleteSelectionIfRange(const VisibleSelection& selection,
                                           EditingState* editing_state,
                                           bool smart_delete,
                                           bool merge_blocks_after_delete,
                                           bool expand_for_special_elements,
                                           bool sanitize_markup) {
  if (!selection.IsRange())
    return;
  ApplyCommandToComposite(
      DeleteSelectionCommand::Create(
          selection, smart_delete, merge_blocks_after_delete,
          expand_for_special_elements, sanitize_markup),
      editing_state);
}

void TypingCommand::DeleteKeyPressed(Document& document,
                                     Options options,
                                     TextGranularity granularity) {
  if (granularity == TextGranularity::kCharacter) {
    LocalFrame* frame = document.GetFrame();
    if (TypingCommand* last_typing_command =
            LastTypingCommandIfStillOpenForTyping(frame)) {
      // If the last typing command is not Delete, open a new typing command.
      // We need to group continuous delete commands alone in a single typing
      // command.
      if (last_typing_command->CommandTypeOfOpenCommand() == kDeleteKey) {
        UpdateSelectionIfDifferentFromCurrentSelection(last_typing_command,
                                                       frame);
        last_typing_command->SetShouldPreventSpellChecking(
            options & kPreventSpellChecking);
        EditingState editing_state;
        last_typing_command->DeleteKeyPressed(granularity, options & kKillRing,
                                              &editing_state);
        return;
      }
    }
  }

  TypingCommand::Create(document, kDeleteKey, "", options, granularity)
      ->Apply();
}

void TypingCommand::ForwardDeleteKeyPressed(Document& document,
                                            EditingState* editing_state,
                                            Options options,
                                            TextGranularity granularity) {
  // FIXME: Forward delete in TextEdit appears to open and close a new typing
  // command.
  if (granularity == TextGranularity::kCharacter) {
    LocalFrame* frame = document.GetFrame();
    if (TypingCommand* last_typing_command =
            LastTypingCommandIfStillOpenForTyping(frame)) {
      UpdateSelectionIfDifferentFromCurrentSelection(last_typing_command,
                                                     frame);
      last_typing_command->SetShouldPreventSpellChecking(options &
                                                         kPreventSpellChecking);
      last_typing_command->ForwardDeleteKeyPressed(
          granularity, options & kKillRing, editing_state);
      return;
    }
  }

  TypingCommand::Create(document, kForwardDeleteKey, "", options, granularity)
      ->Apply();
}

String TypingCommand::TextDataForInputEvent() const {
  if (commands_.IsEmpty() || IsIncrementalInsertion())
    return text_to_insert_;
  return commands_.back()->TextDataForInputEvent();
}

void TypingCommand::UpdateSelectionIfDifferentFromCurrentSelection(
    TypingCommand* typing_command,
    LocalFrame* frame) {
  DCHECK(frame);
  const SelectionInDOMTree& current_selection =
      frame->Selection().GetSelectionInDOMTree();
  if (current_selection == typing_command->EndingSelection().AsSelection())
    return;

  typing_command->SetStartingSelection(
      SelectionForUndoStep::From(current_selection));
  typing_command->SetEndingSelection(
      SelectionForUndoStep::From(current_selection));
}

void TypingCommand::InsertText(Document& document,
                               const String& text,
                               Options options,
                               TextCompositionType composition,
                               const bool is_incremental_insertion) {
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);
  EditingState editing_state;
  InsertText(document, text, frame->Selection().GetSelectionInDOMTree(),
             options, &editing_state, composition, is_incremental_insertion);
}

void TypingCommand::AdjustSelectionAfterIncrementalInsertion(
    LocalFrame* frame,
    const size_t selection_start,
    const size_t text_length,
    EditingState* editing_state) {
  if (!IsIncrementalInsertion())
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  Element* element = frame->Selection()
                         .ComputeVisibleSelectionInDOMTreeDeprecated()
                         .RootEditableElement();

  // TODO(editing-dev): The text insertion should probably always leave the
  // selection in an editable region, but we know of at least one case where it
  // doesn't (see test case in crbug.com/767599). Return early in this case to
  // avoid a crash.
  if (!element) {
    editing_state->Abort();
    return;
  }

  const size_t end = selection_start + text_length;
  const size_t start =
      CompositionType() == kTextCompositionUpdate ? selection_start : end;
  const SelectionInDOMTree& selection =
      CreateSelection(start, end, EndingSelection().IsDirectional(), element);

  if (selection == frame->Selection()
                       .ComputeVisibleSelectionInDOMTreeDeprecated()
                       .AsSelection())
    return;

  SetEndingSelection(SelectionForUndoStep::From(selection));
  frame->Selection().SetSelection(selection);
}

// FIXME: We shouldn't need to take selectionForInsertion. It should be
// identical to FrameSelection's current selection.
void TypingCommand::InsertText(
    Document& document,
    const String& text,
    const SelectionInDOMTree& passed_selection_for_insertion,
    Options options,
    EditingState* editing_state,
    TextCompositionType composition_type,
    const bool is_incremental_insertion,
    InputEvent::InputType input_type) {
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);

  const VisibleSelection& current_selection =
      frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  const VisibleSelection& selection_for_insertion =
      CreateVisibleSelection(passed_selection_for_insertion);

  String new_text = text;
  if (composition_type != kTextCompositionUpdate) {
    new_text = DispatchBeforeTextInsertedEvent(text, selection_for_insertion,
                                               editing_state);
    if (editing_state->IsAborted())
      return;
  }

  if (composition_type == kTextCompositionConfirm) {
    if (DispatchTextInputEvent(frame, new_text, editing_state) !=
        DispatchEventResult::kNotCanceled)
      return;
    // event handler might destroy document.
    if (editing_state->IsAborted())
      return;
    // editing/inserting/insert-text-nodes-disconnect-on-textinput-event.html
    // hits true for ABORT_EDITING_COMMAND_IF macro.
    ABORT_EDITING_COMMAND_IF(!selection_for_insertion.IsValidFor(document));
  }

  // Do nothing if no need to delete and insert.
  if (selection_for_insertion.IsCaret() && new_text.IsEmpty())
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document.UpdateStyleAndLayoutIgnorePendingStylesheets();

  const PlainTextRange selection_offsets = GetSelectionOffsets(frame);
  if (selection_offsets.IsNull())
    return;
  const size_t selection_start = selection_offsets.Start();

  // Set the starting and ending selection appropriately if we are using a
  // selection that is different from the current selection.  In the future, we
  // should change EditCommand to deal with custom selections in a general way
  // that can be used by all of the commands.
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame)) {
    if (last_typing_command->EndingVisibleSelection() !=
        selection_for_insertion) {
      const SelectionForUndoStep& selection_for_insertion_as_undo_step =
          SelectionForUndoStep::From(selection_for_insertion.AsSelection());
      last_typing_command->SetStartingSelection(
          selection_for_insertion_as_undo_step);
      last_typing_command->SetEndingSelection(
          selection_for_insertion_as_undo_step);
    }

    last_typing_command->SetCompositionType(composition_type);
    last_typing_command->SetShouldRetainAutocorrectionIndicator(
        options & kRetainAutocorrectionIndicator);
    last_typing_command->SetShouldPreventSpellChecking(options &
                                                       kPreventSpellChecking);
    last_typing_command->is_incremental_insertion_ = is_incremental_insertion;
    last_typing_command->selection_start_ = selection_start;
    last_typing_command->input_type_ = input_type;

    EventQueueScope event_queue_scope;
    last_typing_command->InsertText(new_text, options & kSelectInsertedText,
                                    editing_state);
    return;
  }

  TypingCommand* command = TypingCommand::Create(
      document, kInsertText, new_text, options, composition_type);
  bool change_selection = selection_for_insertion != current_selection;
  if (change_selection) {
    const SelectionForUndoStep& selection_for_insertion_as_undo_step =
        SelectionForUndoStep::From(selection_for_insertion.AsSelection());
    command->SetStartingSelection(selection_for_insertion_as_undo_step);
    command->SetEndingSelection(selection_for_insertion_as_undo_step);
  }
  command->is_incremental_insertion_ = is_incremental_insertion;
  command->selection_start_ = selection_start;
  command->input_type_ = input_type;
  ABORT_EDITING_COMMAND_IF(!command->Apply());

  if (change_selection) {
    const SelectionInDOMTree& current_selection_as_dom =
        current_selection.AsSelection();
    command->SetEndingSelection(
        SelectionForUndoStep::From(current_selection_as_dom));
    frame->Selection().SetSelection(current_selection_as_dom);
  }
}

bool TypingCommand::InsertLineBreak(Document& document) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(document.GetFrame())) {
    last_typing_command->SetShouldRetainAutocorrectionIndicator(false);
    EditingState editing_state;
    EventQueueScope event_queue_scope;
    last_typing_command->InsertLineBreak(&editing_state);
    return !editing_state.IsAborted();
  }

  return TypingCommand::Create(document, kInsertLineBreak, "", 0)->Apply();
}

bool TypingCommand::InsertParagraphSeparatorInQuotedContent(
    Document& document) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(document.GetFrame())) {
    EditingState editing_state;
    EventQueueScope event_queue_scope;
    last_typing_command->InsertParagraphSeparatorInQuotedContent(
        &editing_state);
    return !editing_state.IsAborted();
  }

  return TypingCommand::Create(document,
                               kInsertParagraphSeparatorInQuotedContent)
      ->Apply();
}

bool TypingCommand::InsertParagraphSeparator(Document& document) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(document.GetFrame())) {
    last_typing_command->SetShouldRetainAutocorrectionIndicator(false);
    EditingState editing_state;
    EventQueueScope event_queue_scope;
    last_typing_command->InsertParagraphSeparator(&editing_state);
    return !editing_state.IsAborted();
  }

  return TypingCommand::Create(document, kInsertParagraphSeparator, "", 0)
      ->Apply();
}

TypingCommand* TypingCommand::LastTypingCommandIfStillOpenForTyping(
    LocalFrame* frame) {
  DCHECK(frame);

  CompositeEditCommand* last_edit_command =
      frame->GetEditor().LastEditCommand();
  if (!last_edit_command || !last_edit_command->IsTypingCommand() ||
      !static_cast<TypingCommand*>(last_edit_command)->IsOpenForMoreTyping())
    return nullptr;

  return static_cast<TypingCommand*>(last_edit_command);
}

void TypingCommand::CloseTyping(LocalFrame* frame) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame))
    last_typing_command->CloseTyping();
}

void TypingCommand::DoApply(EditingState* editing_state) {
  if (EndingSelection().IsNone() ||
      !EndingSelection().IsValidFor(GetDocument()))
    return;

  if (command_type_ == kDeleteKey) {
    if (commands_.IsEmpty())
      opened_by_backward_delete_ = true;
  }

  switch (command_type_) {
    case kDeleteSelection:
      DeleteSelection(smart_delete_, editing_state);
      return;
    case kDeleteKey:
      DeleteKeyPressed(granularity_, kill_ring_, editing_state);
      return;
    case kForwardDeleteKey:
      ForwardDeleteKeyPressed(granularity_, kill_ring_, editing_state);
      return;
    case kInsertLineBreak:
      InsertLineBreak(editing_state);
      return;
    case kInsertParagraphSeparator:
      InsertParagraphSeparator(editing_state);
      return;
    case kInsertParagraphSeparatorInQuotedContent:
      InsertParagraphSeparatorInQuotedContent(editing_state);
      return;
    case kInsertText:
      InsertText(text_to_insert_, select_inserted_text_, editing_state);
      return;
  }

  NOTREACHED();
}

InputEvent::InputType TypingCommand::GetInputType() const {
  using InputType = InputEvent::InputType;

  if (composition_type_ != kTextCompositionNone)
    return InputType::kInsertCompositionText;

  if (input_type_ != InputType::kNone)
    return input_type_;

  switch (command_type_) {
    // TODO(chongz): |DeleteSelection| is used by IME but we don't have
    // direction info.
    case kDeleteSelection:
      return InputType::kDeleteContentBackward;
    case kDeleteKey:
      return DeletionInputTypeFromTextGranularity(DeleteDirection::kBackward,
                                                  granularity_);
    case kForwardDeleteKey:
      return DeletionInputTypeFromTextGranularity(DeleteDirection::kForward,
                                                  granularity_);
    case kInsertText:
      return InputType::kInsertText;
    case kInsertLineBreak:
      return InputType::kInsertLineBreak;
    case kInsertParagraphSeparator:
    case kInsertParagraphSeparatorInQuotedContent:
      return InputType::kInsertParagraph;
    default:
      return InputType::kNone;
  }
}

void TypingCommand::TypingAddedToOpenCommand(
    ETypingCommand command_type_for_added_typing) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  UpdatePreservesTypingStyle(command_type_for_added_typing);
  UpdateCommandTypeOfOpenCommand(command_type_for_added_typing);

  frame->GetEditor().AppliedEditing(this);
}

void TypingCommand::InsertText(const String& text,
                               bool select_inserted_text,
                               EditingState* editing_state) {
  text_to_insert_ = text;

  if (text.IsEmpty()) {
    InsertTextRunWithoutNewlines(text, select_inserted_text, editing_state);
    return;
  }
  size_t selection_start = selection_start_;
  // FIXME: Need to implement selectInsertedText for cases where more than one
  // insert is involved. This requires support from insertTextRunWithoutNewlines
  // and insertParagraphSeparator for extending an existing selection; at the
  // moment they can either put the caret after what's inserted or select what's
  // inserted, but there's no way to "extend selection" to include both an old
  // selection that ends just before where we want to insert text and the newly
  // inserted text.
  unsigned offset = 0;
  size_t newline;
  while ((newline = text.find('\n', offset)) != kNotFound) {
    if (newline > offset) {
      const size_t insertion_length = newline - offset;
      InsertTextRunWithoutNewlines(text.Substring(offset, insertion_length),
                                   false, editing_state);
      if (editing_state->IsAborted())
        return;

      AdjustSelectionAfterIncrementalInsertion(GetDocument().GetFrame(),
                                               selection_start,
                                               insertion_length, editing_state);
      selection_start += insertion_length;
    }

    InsertParagraphSeparator(editing_state);
    if (editing_state->IsAborted())
      return;

    offset = newline + 1;
    ++selection_start;
  }

  if (!offset) {
    InsertTextRunWithoutNewlines(text, select_inserted_text, editing_state);
    if (editing_state->IsAborted())
      return;

    AdjustSelectionAfterIncrementalInsertion(GetDocument().GetFrame(),
                                             selection_start, text.length(),
                                             editing_state);
    return;
  }

  if (text.length() > offset) {
    const size_t insertion_length = text.length() - offset;
    InsertTextRunWithoutNewlines(text.Substring(offset, insertion_length),
                                 select_inserted_text, editing_state);
    if (editing_state->IsAborted())
      return;

    AdjustSelectionAfterIncrementalInsertion(GetDocument().GetFrame(),
                                             selection_start, insertion_length,
                                             editing_state);
  }
}

void TypingCommand::InsertTextRunWithoutNewlines(const String& text,
                                                 bool select_inserted_text,
                                                 EditingState* editing_state) {
  CompositeEditCommand* command;
  if (IsIncrementalInsertion()) {
    command = InsertIncrementalTextCommand::Create(
        GetDocument(), text, select_inserted_text,
        composition_type_ == kTextCompositionNone
            ? InsertIncrementalTextCommand::
                  kRebalanceLeadingAndTrailingWhitespaces
            : InsertIncrementalTextCommand::kRebalanceAllWhitespaces);
  } else {
    command = InsertTextCommand::Create(
        GetDocument(), text, select_inserted_text,
        composition_type_ == kTextCompositionNone
            ? InsertTextCommand::kRebalanceLeadingAndTrailingWhitespaces
            : InsertTextCommand::kRebalanceAllWhitespaces);
  }

  ApplyCommandToComposite(command, EndingSelection(), editing_state);
  if (editing_state->IsAborted())
    return;

  TypingAddedToOpenCommand(kInsertText);
}

void TypingCommand::InsertLineBreak(EditingState* editing_state) {
  if (!CanAppendNewLineFeedToSelection(EndingVisibleSelection(), editing_state))
    return;

  ApplyCommandToComposite(InsertLineBreakCommand::Create(GetDocument()),
                          editing_state);
  if (editing_state->IsAborted())
    return;
  TypingAddedToOpenCommand(kInsertLineBreak);
}

void TypingCommand::InsertParagraphSeparator(EditingState* editing_state) {
  if (!CanAppendNewLineFeedToSelection(EndingVisibleSelection(), editing_state))
    return;

  ApplyCommandToComposite(
      InsertParagraphSeparatorCommand::Create(GetDocument()), editing_state);
  if (editing_state->IsAborted())
    return;
  TypingAddedToOpenCommand(kInsertParagraphSeparator);
}

void TypingCommand::InsertParagraphSeparatorInQuotedContent(
    EditingState* editing_state) {
  // If the selection starts inside a table, just insert the paragraph separator
  // normally Breaking the blockquote would also break apart the table, which is
  // unecessary when inserting a newline
  if (EnclosingNodeOfType(EndingSelection().Start(), &IsTableStructureNode)) {
    InsertParagraphSeparator(editing_state);
    return;
  }

  ApplyCommandToComposite(BreakBlockquoteCommand::Create(GetDocument()),
                          editing_state);
  if (editing_state->IsAborted())
    return;
  TypingAddedToOpenCommand(kInsertParagraphSeparatorInQuotedContent);
}

bool TypingCommand::MakeEditableRootEmpty(EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  Element* root = RootEditableElementOf(EndingSelection().Base());
  if (!root || !root->HasChildren())
    return false;

  if (root->firstChild() == root->lastChild()) {
    if (IsHTMLBRElement(root->firstChild())) {
      // If there is a single child and it could be a placeholder, leave it
      // alone.
      if (root->GetLayoutObject() &&
          root->GetLayoutObject()->IsLayoutBlockFlow())
        return false;
    }
  }

  while (Node* child = root->firstChild()) {
    RemoveNode(child, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  AddBlockPlaceholderIfNeeded(root, editing_state);
  if (editing_state->IsAborted())
    return false;
  const SelectionInDOMTree& selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*root))
          .SetIsDirectional(EndingSelection().IsDirectional())
          .Build();
  SetEndingSelection(SelectionForUndoStep::From(selection));

  return true;
}

// If there are multiple Unicode code points to be deleted, adjust the
// range to match platform conventions.
static VisibleSelection AdjustSelectionForBackwardDelete(
    const VisibleSelection& selection) {
  if (selection.End().ComputeContainerNode() !=
      selection.Start().ComputeContainerNode())
    return selection;
  if (selection.End().ComputeOffsetInContainerNode() -
          selection.Start().ComputeOffsetInContainerNode() <=
      1)
    return selection;
  return VisibleSelection::CreateWithoutValidationDeprecated(
      selection.End(),
      PreviousPositionOf(selection.End(), PositionMoveType::kBackwardDeletion),
      selection.Affinity());
}

void TypingCommand::DeleteKeyPressed(TextGranularity granularity,
                                     bool kill_ring,
                                     EditingState* editing_state) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  if (EndingSelection().IsRange()) {
    DeleteKeyPressedInternal(EndingVisibleSelection(), EndingSelection(),
                             kill_ring, editing_state);
    return;
  }

  if (!EndingSelection().IsCaret()) {
    NOTREACHED();
    return;
  }

  // After breaking out of an empty mail blockquote, we still want continue
  // with the deletion so actual content will get deleted, and not just the
  // quote style.
  const bool break_out_result =
      BreakOutOfEmptyMailBlockquotedParagraph(editing_state);
  if (editing_state->IsAborted())
    return;
  if (break_out_result)
    TypingAddedToOpenCommand(kDeleteKey);

  smart_delete_ = false;
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  SelectionModifier selection_modifier(*frame, EndingVisibleSelection());
  selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                            SelectionModifyDirection::kBackward, granularity);
  if (kill_ring && selection_modifier.Selection().IsCaret() &&
      granularity != TextGranularity::kCharacter) {
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kBackward,
                              TextGranularity::kCharacter);
  }

  const VisiblePosition& visible_start(EndingVisibleSelection().VisibleStart());
  const VisiblePosition& previous_position =
      PreviousPositionOf(visible_start, kCannotCrossEditingBoundary);
  const Node* enclosing_table_cell =
      EnclosingNodeOfType(visible_start.DeepEquivalent(), &IsTableCell);
  const Node* enclosing_table_cell_for_previous_position =
      EnclosingNodeOfType(previous_position.DeepEquivalent(), &IsTableCell);
  if (previous_position.IsNull() ||
      enclosing_table_cell != enclosing_table_cell_for_previous_position) {
    // When the caret is at the start of the editable area, or cell, in an
    // empty list item, break out of the list item.
    const bool break_out_of_empty_list_item_result =
        BreakOutOfEmptyListItem(editing_state);
    if (editing_state->IsAborted())
      return;
    if (break_out_of_empty_list_item_result) {
      TypingAddedToOpenCommand(kDeleteKey);
      return;
    }
  }
  if (previous_position.IsNull()) {
    // When there are no visible positions in the editing root, delete its
    // entire contents.
    if (NextPositionOf(visible_start, kCannotCrossEditingBoundary).IsNull() &&
        MakeEditableRootEmpty(editing_state)) {
      TypingAddedToOpenCommand(kDeleteKey);
      return;
    }
    if (editing_state->IsAborted())
      return;
  }

  // If we have a caret selection at the beginning of a cell, we have
  // nothing to do.
  if (enclosing_table_cell && visible_start.DeepEquivalent() ==
                                  VisiblePosition::FirstPositionInNode(
                                      *const_cast<Node*>(enclosing_table_cell))
                                      .DeepEquivalent())
    return;

  // If the caret is at the start of a paragraph after a table, move content
  // into the last table cell.
  if (IsStartOfParagraph(visible_start) &&
      TableElementJustBefore(
          PreviousPositionOf(visible_start, kCannotCrossEditingBoundary))) {
    // Unless the caret is just before a table.  We don't want to move a
    // table into the last table cell.
    if (TableElementJustAfter(visible_start))
      return;
    // Extend the selection backward into the last cell, then deletion will
    // handle the move.
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kBackward, granularity);
    // If the caret is just after a table, select the table and don't delete
    // anything.
  } else if (Element* table = TableElementJustBefore(visible_start)) {
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .Collapse(Position::BeforeNode(*table))
            .Extend(EndingSelection().Start())
            .SetIsDirectional(EndingSelection().IsDirectional())
            .Build();
    SetEndingSelection(SelectionForUndoStep::From(selection));
    TypingAddedToOpenCommand(kDeleteKey);
    return;
  }

  const VisibleSelection& selection_to_delete =
      granularity == TextGranularity::kCharacter
          ? AdjustSelectionForBackwardDelete(selection_modifier.Selection())
          : selection_modifier.Selection();

  if (!StartingSelection().IsRange() ||
      selection_to_delete.Base() != StartingSelection().Start()) {
    DeleteKeyPressedInternal(
        selection_to_delete,
        SelectionForUndoStep::From(selection_to_delete.AsSelection()),
        kill_ring, editing_state);
    return;
  }
  // Note: |StartingSelection().End()| can be disconnected.
  // See editing/deleting/delete_list_item.html on MacOS.
  const SelectionForUndoStep selection_after_undo =
      SelectionForUndoStep::Builder()
          .SetBaseAndExtentAsBackwardSelection(
              StartingSelection().End(),
              CreateVisiblePosition(selection_to_delete.Extent())
                  .DeepEquivalent())
          .Build();
  DeleteKeyPressedInternal(selection_to_delete, selection_after_undo, kill_ring,
                           editing_state);
}

void TypingCommand::DeleteKeyPressedInternal(
    const VisibleSelection& selection_to_delete,
    const SelectionForUndoStep& selection_after_undo,
    bool kill_ring,
    EditingState* editing_state) {
  DCHECK(!selection_to_delete.IsNone());
  if (selection_to_delete.IsNone())
    return;

  if (selection_to_delete.IsCaret())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(frame);

  if (kill_ring)
    frame->GetEditor().AddToKillRing(
        selection_to_delete.ToNormalizedEphemeralRange());
  // On Mac, make undo select everything that has been deleted, unless an undo
  // will undo more than just this deletion.
  // FIXME: This behaves like TextEdit except for the case where you open with
  // text insertion and then delete more text than you insert.  In that case all
  // of the text that was around originally should be selected.
  if (frame->GetEditor().Behavior().ShouldUndoOfDeleteSelectText() &&
      opened_by_backward_delete_)
    SetStartingSelection(selection_after_undo);
  DeleteSelectionIfRange(selection_to_delete, editing_state, smart_delete_);
  if (editing_state->IsAborted())
    return;
  SetSmartDelete(false);
  TypingAddedToOpenCommand(kDeleteKey);
}

static Position ComputeExtentForForwardDeleteUndo(
    const VisibleSelection& selection,
    const Position& extent) {
  if (extent.ComputeContainerNode() != selection.End().ComputeContainerNode())
    return selection.Extent();
  const int extra_characters =
      selection.Start().ComputeContainerNode() ==
              selection.End().ComputeContainerNode()
          ? selection.End().ComputeOffsetInContainerNode() -
                selection.Start().ComputeOffsetInContainerNode()
          : selection.End().ComputeOffsetInContainerNode();
  return Position(extent.ComputeContainerNode(),
                  extent.ComputeOffsetInContainerNode() + extra_characters);
}

void TypingCommand::ForwardDeleteKeyPressed(TextGranularity granularity,
                                            bool kill_ring,
                                            EditingState* editing_state) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  if (EndingSelection().IsRange()) {
    ForwardDeleteKeyPressedInternal(EndingVisibleSelection(), EndingSelection(),
                                    kill_ring, editing_state);
    return;
  }

  if (!EndingSelection().IsCaret()) {
    NOTREACHED();
    return;
  }

  smart_delete_ = false;
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Handle delete at beginning-of-block case.
  // Do nothing in the case that the caret is at the start of a
  // root editable element or at the start of a document.
  SelectionModifier selection_modifier(*frame, EndingVisibleSelection());
  selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                            SelectionModifyDirection::kForward, granularity);
  if (kill_ring && selection_modifier.Selection().IsCaret() &&
      granularity != TextGranularity::kCharacter) {
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kForward,
                              TextGranularity::kCharacter);
  }

  Position downstream_end = MostForwardCaretPosition(EndingSelection().End());
  VisiblePosition visible_end = EndingVisibleSelection().VisibleEnd();
  Node* enclosing_table_cell =
      EnclosingNodeOfType(visible_end.DeepEquivalent(), &IsTableCell);
  if (enclosing_table_cell &&
      visible_end.DeepEquivalent() ==
          VisiblePosition::LastPositionInNode(*enclosing_table_cell)
              .DeepEquivalent())
    return;
  if (visible_end.DeepEquivalent() ==
      EndOfParagraph(visible_end).DeepEquivalent()) {
    downstream_end = MostForwardCaretPosition(
        NextPositionOf(visible_end, kCannotCrossEditingBoundary)
            .DeepEquivalent());
  }
  // When deleting tables: Select the table first, then perform the deletion
  if (IsDisplayInsideTable(downstream_end.ComputeContainerNode()) &&
      downstream_end.ComputeOffsetInContainerNode() <=
          CaretMinOffset(downstream_end.ComputeContainerNode())) {
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .SetBaseAndExtentDeprecated(
                EndingSelection().End(),
                Position::AfterNode(*downstream_end.ComputeContainerNode()))
            .SetIsDirectional(EndingSelection().IsDirectional())
            .Build();
    SetEndingSelection(SelectionForUndoStep::From(selection));
    TypingAddedToOpenCommand(kForwardDeleteKey);
    return;
  }

  // deleting to end of paragraph when at end of paragraph needs to merge
  // the next paragraph (if any)
  if (granularity == TextGranularity::kParagraphBoundary &&
      selection_modifier.Selection().IsCaret() &&
      IsEndOfParagraph(selection_modifier.Selection().VisibleEnd())) {
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kForward,
                              TextGranularity::kCharacter);
  }

  const VisibleSelection& selection_to_delete = selection_modifier.Selection();
  if (!StartingSelection().IsRange() ||
      MostBackwardCaretPosition(selection_to_delete.Base()) !=
          StartingSelection().Start()) {
    ForwardDeleteKeyPressedInternal(
        selection_to_delete,
        SelectionForUndoStep::From(selection_to_delete.AsSelection()),
        kill_ring, editing_state);
    return;
  }
  // Note: |StartingSelection().Start()| can be disconnected.
  const SelectionForUndoStep selection_after_undo =
      SelectionForUndoStep::Builder()
          .SetBaseAndExtentAsForwardSelection(
              StartingSelection().Start(),
              ComputeExtentForForwardDeleteUndo(selection_to_delete,
                                                StartingSelection().End()))
          .Build();
  ForwardDeleteKeyPressedInternal(selection_to_delete, selection_after_undo,
                                  kill_ring, editing_state);
}

void TypingCommand::ForwardDeleteKeyPressedInternal(
    const VisibleSelection& selection_to_delete,
    const SelectionForUndoStep& selection_after_undo,
    bool kill_ring,
    EditingState* editing_state) {
  DCHECK(!selection_to_delete.IsNone());
  if (selection_to_delete.IsNone())
    return;

  if (selection_to_delete.IsCaret())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(frame);

  if (kill_ring)
    frame->GetEditor().AddToKillRing(
        selection_to_delete.ToNormalizedEphemeralRange());
  // Make undo select what was deleted on Mac alone
  if (frame->GetEditor().Behavior().ShouldUndoOfDeleteSelectText())
    SetStartingSelection(selection_after_undo);
  DeleteSelectionIfRange(selection_to_delete, editing_state, smart_delete_);
  if (editing_state->IsAborted())
    return;
  SetSmartDelete(false);
  TypingAddedToOpenCommand(kForwardDeleteKey);
}

void TypingCommand::DeleteSelection(bool smart_delete,
                                    EditingState* editing_state) {
  if (!CompositeEditCommand::DeleteSelection(editing_state, smart_delete))
    return;
  TypingAddedToOpenCommand(kDeleteSelection);
}

void TypingCommand::UpdatePreservesTypingStyle(ETypingCommand command_type) {
  switch (command_type) {
    case kDeleteSelection:
    case kDeleteKey:
    case kForwardDeleteKey:
    case kInsertParagraphSeparator:
    case kInsertLineBreak:
      preserves_typing_style_ = true;
      return;
    case kInsertParagraphSeparatorInQuotedContent:
    case kInsertText:
      preserves_typing_style_ = false;
      return;
  }
  NOTREACHED();
  preserves_typing_style_ = false;
}

bool TypingCommand::IsTypingCommand() const {
  return true;
}

}  // namespace blink
