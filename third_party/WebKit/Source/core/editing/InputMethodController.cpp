/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/editing/InputMethodController.h"

#include "core/InputModeNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SetSelectionData.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/state_machines/BackwardCodePointStateMachine.h"
#include "core/editing/state_machines/ForwardCodePointStateMachine.h"
#include "core/events/CompositionEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

namespace {

void DispatchCompositionUpdateEvent(LocalFrame& frame, const String& text) {
  Element* target = frame.GetDocument()->FocusedElement();
  if (!target)
    return;

  CompositionEvent* event = CompositionEvent::Create(
      EventTypeNames::compositionupdate, frame.DomWindow(), text);
  target->DispatchEvent(event);
}

void DispatchCompositionEndEvent(LocalFrame& frame, const String& text) {
  Element* target = frame.GetDocument()->FocusedElement();
  if (!target)
    return;

  CompositionEvent* event = CompositionEvent::Create(
      EventTypeNames::compositionend, frame.DomWindow(), text);
  target->DispatchEvent(event);
}

bool NeedsIncrementalInsertion(const LocalFrame& frame,
                               const String& new_text) {
  // No need to apply incremental insertion if it doesn't support formated text.
  if (!frame.GetEditor().CanEditRichly())
    return false;

  // No need to apply incremental insertion if the old text (text to be
  // replaced) or the new text (text to be inserted) is empty.
  if (frame.SelectedText().IsEmpty() || new_text.IsEmpty())
    return false;

  return true;
}

void DispatchBeforeInputFromComposition(EventTarget* target,
                                        InputEvent::InputType input_type,
                                        const String& data) {
  if (!RuntimeEnabledFeatures::InputEventEnabled())
    return;
  if (!target)
    return;
  // TODO(chongz): Pass appropriate |ranges| after it's defined on spec.
  // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
  InputEvent* before_input_event = InputEvent::CreateBeforeInput(
      input_type, data, InputEvent::kNotCancelable,
      InputEvent::EventIsComposing::kIsComposing, nullptr);
  target->DispatchEvent(before_input_event);
}

// Used to insert/replace text during composition update and confirm
// composition.
// Procedure:
//   1. Fire 'beforeinput' event for (TODO(chongz): deleted composed text) and
//      inserted text
//   2. Fire 'compositionupdate' event
//   3. Fire TextEvent and modify DOM
//   TODO(chongz): 4. Fire 'input' event
void InsertTextDuringCompositionWithEvents(
    LocalFrame& frame,
    const String& text,
    TypingCommand::Options options,
    TypingCommand::TextCompositionType composition_type) {
  DCHECK(composition_type ==
             TypingCommand::TextCompositionType::kTextCompositionUpdate ||
         composition_type ==
             TypingCommand::TextCompositionType::kTextCompositionConfirm ||
         composition_type ==
             TypingCommand::TextCompositionType::kTextCompositionCancel)
      << "compositionType should be TextCompositionUpdate or "
         "TextCompositionConfirm  or TextCompositionCancel, but got "
      << static_cast<int>(composition_type);
  if (!frame.GetDocument())
    return;

  Element* target = frame.GetDocument()->FocusedElement();
  if (!target)
    return;

  DispatchBeforeInputFromComposition(
      target, InputEvent::InputType::kInsertCompositionText, text);

  // 'beforeinput' event handler may destroy document.
  if (!frame.GetDocument())
    return;

  DispatchCompositionUpdateEvent(frame, text);
  // 'compositionupdate' event handler may destroy document.
  if (!frame.GetDocument())
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  const bool is_incremental_insertion = NeedsIncrementalInsertion(frame, text);

  switch (composition_type) {
    case TypingCommand::TextCompositionType::kTextCompositionUpdate:
    case TypingCommand::TextCompositionType::kTextCompositionConfirm:
      // Calling |TypingCommand::insertText()| with empty text will result in an
      // incorrect ending selection. We need to delete selection first.
      // https://crbug.com/693481
      if (text.IsEmpty())
        TypingCommand::DeleteSelection(*frame.GetDocument(), 0);
      TypingCommand::InsertText(*frame.GetDocument(), text, options,
                                composition_type, is_incremental_insertion);
      break;
    case TypingCommand::TextCompositionType::kTextCompositionCancel:
      // TODO(chongz): Use TypingCommand::insertText after TextEvent was
      // removed. (Removed from spec since 2012)
      // See TextEvent.idl.
      frame.GetEventHandler().HandleTextInputEvent(text, 0,
                                                   kTextEventInputComposition);
      break;
    default:
      NOTREACHED();
  }
  // TODO(chongz): Fire 'input' event.
}

AtomicString GetInputModeAttribute(Element* element) {
  if (!element)
    return AtomicString();

  bool query_attribute = false;
  if (isHTMLInputElement(*element)) {
    query_attribute = toHTMLInputElement(*element).SupportsInputModeAttribute();
  } else if (isHTMLTextAreaElement(*element)) {
    query_attribute = true;
  } else {
    element->GetDocument().UpdateStyleAndLayoutTree();
    if (HasEditableStyle(*element))
      query_attribute = true;
  }

  if (!query_attribute)
    return AtomicString();

  // TODO(dtapuska): We may wish to restrict this to a yet to be proposed
  // <contenteditable> or <richtext> element Mozilla discussed at TPAC 2016.
  return element->FastGetAttribute(HTMLNames::inputmodeAttr).LowerASCII();
}

constexpr int kInvalidDeletionLength = -1;
constexpr bool IsInvalidDeletionLength(const int length) {
  return length == kInvalidDeletionLength;
}

int CalculateBeforeDeletionLengthsInCodePoints(
    const String& text,
    const int before_length_in_code_points,
    const int selection_start) {
  DCHECK_GE(before_length_in_code_points, 0);
  DCHECK_GE(selection_start, 0);
  DCHECK_LE(selection_start, static_cast<int>(text.length()));

  const UChar* u_text = text.Characters16();
  BackwardCodePointStateMachine backward_machine;
  int counter = before_length_in_code_points;
  int deletion_start = selection_start;
  while (counter > 0 && deletion_start > 0) {
    const TextSegmentationMachineState state =
        backward_machine.FeedPrecedingCodeUnit(u_text[deletion_start - 1]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::kInvalid)
      return kInvalidDeletionLength;

    if (backward_machine.AtCodePointBoundary())
      --counter;
    --deletion_start;
  }
  if (!backward_machine.AtCodePointBoundary())
    return kInvalidDeletionLength;

  const int offset = backward_machine.GetBoundaryOffset();
  DCHECK_EQ(-offset, selection_start - deletion_start);
  return -offset;
}

int CalculateAfterDeletionLengthsInCodePoints(
    const String& text,
    const int after_length_in_code_points,
    const int selection_end) {
  DCHECK_GE(after_length_in_code_points, 0);
  DCHECK_GE(selection_end, 0);
  const int length = text.length();
  DCHECK_LE(selection_end, length);

  const UChar* u_text = text.Characters16();
  ForwardCodePointStateMachine forward_machine;
  int counter = after_length_in_code_points;
  int deletion_end = selection_end;
  while (counter > 0 && deletion_end < length) {
    const TextSegmentationMachineState state =
        forward_machine.FeedFollowingCodeUnit(u_text[deletion_end]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::kInvalid)
      return kInvalidDeletionLength;

    if (forward_machine.AtCodePointBoundary())
      --counter;
    ++deletion_end;
  }
  if (!forward_machine.AtCodePointBoundary())
    return kInvalidDeletionLength;

  const int offset = forward_machine.GetBoundaryOffset();
  DCHECK_EQ(offset, deletion_end - selection_end);
  return offset;
}

Element* RootEditableElementOfSelection(const FrameSelection& frameSelection) {
  const SelectionInDOMTree& selection = frameSelection.GetSelectionInDOMTree();
  if (selection.IsNone())
    return nullptr;
  // To avoid update layout, we attempt to get root editable element from
  // a position where script/user specified.
  if (Element* editable = RootEditableElementOf(selection.Base()))
    return editable;

  // This is work around for applications assumes a position before editable
  // element as editable[1]
  // [1] http://crbug.com/712761

  // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  frameSelection.GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  const VisibleSelection& visibleSeleciton =
      frameSelection.ComputeVisibleSelectionInDOMTree();
  return RootEditableElementOf(visibleSeleciton.Start());
}

}  // anonymous namespace

InputMethodController* InputMethodController::Create(LocalFrame& frame) {
  return new InputMethodController(frame);
}

InputMethodController::InputMethodController(LocalFrame& frame)
    : frame_(&frame), has_composition_(false) {}

InputMethodController::~InputMethodController() = default;

bool InputMethodController::IsAvailable() const {
  return GetFrame().GetDocument();
}

Document& InputMethodController::GetDocument() const {
  DCHECK(IsAvailable());
  return *GetFrame().GetDocument();
}

bool InputMethodController::HasComposition() const {
  return has_composition_ && !composition_range_->collapsed() &&
         composition_range_->IsConnected();
}

inline Editor& InputMethodController::GetEditor() const {
  return GetFrame().GetEditor();
}

void InputMethodController::Clear() {
  has_composition_ = false;
  if (composition_range_) {
    composition_range_->setStart(&GetDocument(), 0);
    composition_range_->collapse(true);
  }
  GetDocument().Markers().RemoveMarkersOfTypes(DocumentMarker::kComposition);
}

void InputMethodController::ContextDestroyed(Document*) {
  Clear();
  composition_range_ = nullptr;
}

void InputMethodController::DocumentAttached(Document* document) {
  DCHECK(document);
  SetContext(document);
}

void InputMethodController::SelectComposition() const {
  const EphemeralRange range = CompositionEphemeralRange();
  if (range.IsNull())
    return;

  // The composition can start inside a composed character sequence, so we have
  // to override checks. See <http://bugs.webkit.org/show_bug.cgi?id=15781>
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build());
}

bool IsTextTooLongAt(const Position& position) {
  const Element* element = EnclosingTextControl(position);
  if (!element)
    return false;
  if (isHTMLInputElement(element))
    return toHTMLInputElement(element)->TooLong();
  if (isHTMLTextAreaElement(element))
    return toHTMLTextAreaElement(element)->TooLong();
  return false;
}

bool InputMethodController::FinishComposingText(
    ConfirmCompositionBehavior confirm_behavior) {
  if (!HasComposition())
    return false;

  // If text is longer than maxlength, give input/textarea's handler a chance to
  // clamp the text by replacing the composition with the same value.
  const bool is_too_long = IsTextTooLongAt(composition_range_->StartPosition());

  const String& composing = ComposingText();

  if (confirm_behavior == kKeepSelection) {
    // Do not dismiss handles even if we are moving selection, because we will
    // eventually move back to the old selection offsets.
    const bool is_handle_visible = GetFrame().Selection().IsHandleVisible();

    const PlainTextRange& old_offsets = GetSelectionOffsets();
    Editor::RevealSelectionScope reveal_selection_scope(&GetEditor());

    if (is_too_long) {
      ReplaceComposition(ComposingText());
    } else {
      Clear();
      DispatchCompositionEndEvent(GetFrame(), composing);
    }

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    const EphemeralRange& old_selection_range =
        EphemeralRangeForOffsets(old_offsets);
    if (old_selection_range.IsNull())
      return false;
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .SetBaseAndExtent(old_selection_range)
            .Build();
    GetFrame().Selection().SetSelection(
        selection, SetSelectionData::Builder()
                       .SetShouldCloseTyping(true)
                       .SetShouldShowHandle(is_handle_visible)
                       .Build());
    return true;
  }

  Element* root_editable_element =
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();
  if (!root_editable_element)
    return false;
  PlainTextRange composition_range =
      PlainTextRange::Create(*root_editable_element, *composition_range_);
  if (composition_range.IsNull())
    return false;

  if (is_too_long) {
    ReplaceComposition(ComposingText());
  } else {
    Clear();
  }

  if (!MoveCaret(composition_range.End()))
    return false;

  DispatchCompositionEndEvent(GetFrame(), composing);
  return true;
}

bool InputMethodController::CommitText(
    const String& text,
    const Vector<CompositionUnderline>& underlines,
    int relative_caret_position) {
  if (HasComposition()) {
    return ReplaceCompositionAndMoveCaret(text, relative_caret_position,
                                          underlines);
  }

  return InsertTextAndMoveCaret(text, relative_caret_position, underlines);
}

bool InputMethodController::ReplaceComposition(const String& text) {
  if (!HasComposition())
    return false;

  // Select the text that will be deleted or replaced.
  SelectComposition();

  if (GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone())
    return false;

  if (!IsAvailable())
    return false;

  Clear();

  InsertTextDuringCompositionWithEvents(
      GetFrame(), text, 0,
      TypingCommand::TextCompositionType::kTextCompositionConfirm);
  // Event handler might destroy document.
  if (!IsAvailable())
    return false;

  // No DOM update after 'compositionend'.
  DispatchCompositionEndEvent(GetFrame(), text);

  return true;
}

// relativeCaretPosition is relative to the end of the text.
static int ComputeAbsoluteCaretPosition(size_t text_start,
                                        size_t text_length,
                                        int relative_caret_position) {
  return text_start + text_length + relative_caret_position;
}

void InputMethodController::AddCompositionUnderlines(
    const Vector<CompositionUnderline>& underlines,
    ContainerNode* base_element,
    unsigned offset_in_plain_chars) {
  for (const auto& underline : underlines) {
    unsigned underline_start = offset_in_plain_chars + underline.StartOffset();
    unsigned underline_end = offset_in_plain_chars + underline.EndOffset();

    EphemeralRange ephemeral_line_range =
        PlainTextRange(underline_start, underline_end)
            .CreateRange(*base_element);
    if (ephemeral_line_range.IsNull())
      continue;

    GetDocument().Markers().AddCompositionMarker(
        ephemeral_line_range, underline.GetColor(),
        underline.Thick() ? StyleableMarker::Thickness::kThick
                          : StyleableMarker::Thickness::kThin,
        underline.BackgroundColor());
  }
}

bool InputMethodController::ReplaceCompositionAndMoveCaret(
    const String& text,
    int relative_caret_position,
    const Vector<CompositionUnderline>& underlines) {
  Element* root_editable_element =
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();
  if (!root_editable_element)
    return false;
  DCHECK(HasComposition());
  PlainTextRange composition_range =
      PlainTextRange::Create(*root_editable_element, *composition_range_);
  if (composition_range.IsNull())
    return false;
  int text_start = composition_range.Start();

  if (!ReplaceComposition(text))
    return false;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  AddCompositionUnderlines(underlines, root_editable_element, text_start);

  int absolute_caret_position = ComputeAbsoluteCaretPosition(
      text_start, text.length(), relative_caret_position);
  return MoveCaret(absolute_caret_position);
}

bool InputMethodController::InsertText(const String& text) {
  if (DispatchBeforeInputInsertText(GetDocument().FocusedElement(), text) !=
      DispatchEventResult::kNotCanceled)
    return false;
  GetEditor().InsertText(text, 0);
  return true;
}

bool InputMethodController::InsertTextAndMoveCaret(
    const String& text,
    int relative_caret_position,
    const Vector<CompositionUnderline>& underlines) {
  PlainTextRange selection_range = GetSelectionOffsets();
  if (selection_range.IsNull())
    return false;
  int text_start = selection_range.Start();

  if (!InsertText(text))
    return false;
  Element* root_editable_element =
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();
  if (root_editable_element) {
    AddCompositionUnderlines(underlines, root_editable_element, text_start);
  }

  int absolute_caret_position = ComputeAbsoluteCaretPosition(
      text_start, text.length(), relative_caret_position);
  return MoveCaret(absolute_caret_position);
}

void InputMethodController::CancelComposition() {
  if (!HasComposition())
    return;

  Editor::RevealSelectionScope reveal_selection_scope(&GetEditor());

  if (GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone())
    return;

  Clear();

  InsertTextDuringCompositionWithEvents(
      GetFrame(), g_empty_string, 0,
      TypingCommand::TextCompositionType::kTextCompositionCancel);
  // Event handler might destroy document.
  if (!IsAvailable())
    return;

  // An open typing command that disagrees about current selection would cause
  // issues with typing later on.
  TypingCommand::CloseTyping(frame_);

  // No DOM update after 'compositionend'.
  DispatchCompositionEndEvent(GetFrame(), g_empty_string);
}

void InputMethodController::SetComposition(
    const String& text,
    const Vector<CompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  Editor::RevealSelectionScope reveal_selection_scope(&GetEditor());

  // Updates styles before setting selection for composition to prevent
  // inserting the previous composition text into text nodes oddly.
  // See https://bugs.webkit.org/show_bug.cgi?id=46868
  GetDocument().UpdateStyleAndLayoutTree();

  SelectComposition();

  if (GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone())
    return;

  Element* target = GetDocument().FocusedElement();
  if (!target)
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  PlainTextRange selected_range = CreateSelectionRangeForSetComposition(
      selection_start, selection_end, text.length());

  // Dispatch an appropriate composition event to the focused node.
  // We check the composition status and choose an appropriate composition event
  // since this function is used for three purposes:
  // 1. Starting a new composition.
  //    Send a compositionstart and a compositionupdate event when this function
  //    creates a new composition node, i.e. !hasComposition() &&
  //    !text.isEmpty().
  //    Sending a compositionupdate event at this time ensures that at least one
  //    compositionupdate event is dispatched.
  // 2. Updating the existing composition node.
  //    Send a compositionupdate event when this function updates the existing
  //    composition node, i.e. hasComposition() && !text.isEmpty().
  // 3. Canceling the ongoing composition.
  //    Send a compositionend event when function deletes the existing
  //    composition node, i.e. !hasComposition() && test.isEmpty().
  if (text.IsEmpty()) {
    if (HasComposition()) {
      Editor::RevealSelectionScope reveal_selection_scope(&GetEditor());
      ReplaceComposition(g_empty_string);
    } else {
      // It's weird to call |setComposition()| with empty text outside
      // composition, however some IME (e.g. Japanese IBus-Anthy) did this, so
      // we simply delete selection without sending extra events.
      TypingCommand::DeleteSelection(GetDocument(),
                                     TypingCommand::kPreventSpellChecking);
    }

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    SetEditableSelectionOffsets(selected_range);
    return;
  }

  // We should send a 'compositionstart' event only when the given text is not
  // empty because this function doesn't create a composition node when the text
  // is empty.
  if (!HasComposition()) {
    target->DispatchEvent(CompositionEvent::Create(
        EventTypeNames::compositionstart, GetFrame().DomWindow(),
        GetFrame().SelectedText()));
    if (!IsAvailable())
      return;
  }

  DCHECK(!text.IsEmpty());

  Clear();

  InsertTextDuringCompositionWithEvents(
      GetFrame(), text,
      TypingCommand::kSelectInsertedText | TypingCommand::kPreventSpellChecking,
      TypingCommand::kTextCompositionUpdate);
  // Event handlers might destroy document.
  if (!IsAvailable())
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Find out what node has the composition now.
  Position base = MostForwardCaretPosition(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree().Base());
  Node* base_node = base.AnchorNode();
  if (!base_node || !base_node->IsTextNode())
    return;

  Position extent =
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree().Extent();
  Node* extent_node = extent.AnchorNode();

  unsigned extent_offset = extent.ComputeOffsetInContainerNode();
  unsigned base_offset = base.ComputeOffsetInContainerNode();

  has_composition_ = true;
  if (!composition_range_)
    composition_range_ = Range::Create(GetDocument());
  composition_range_->setStart(base_node, base_offset);
  composition_range_->setEnd(extent_node, extent_offset);

  if (base_node->GetLayoutObject())
    base_node->GetLayoutObject()->SetShouldDoFullPaintInvalidation();

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // We shouldn't close typing in the middle of setComposition.
  SetEditableSelectionOffsets(selected_range, TypingContinuation::kContinue);

  if (underlines.IsEmpty()) {
    GetDocument().Markers().AddCompositionMarker(
        EphemeralRange(composition_range_), Color::kBlack,
        StyleableMarker::Thickness::kThin,
        LayoutTheme::GetTheme().PlatformDefaultCompositionBackgroundColor());
    return;
  }

  const PlainTextRange composition_plain_text_range =
      PlainTextRange::Create(*base_node->parentNode(), *composition_range_);
  AddCompositionUnderlines(underlines, base_node->parentNode(),
                           composition_plain_text_range.Start());
}

PlainTextRange InputMethodController::CreateSelectionRangeForSetComposition(
    int selection_start,
    int selection_end,
    size_t text_length) const {
  const int selection_offsets_start =
      static_cast<int>(GetSelectionOffsets().Start());
  const int start = selection_offsets_start + selection_start;
  const int end = selection_offsets_start + selection_end;
  return CreateRangeForSelection(start, end, text_length);
}

void InputMethodController::SetCompositionFromExistingText(
    const Vector<CompositionUnderline>& underlines,
    unsigned composition_start,
    unsigned composition_end) {
  Element* editable = GetFrame()
                          .Selection()
                          .ComputeVisibleSelectionInDOMTreeDeprecated()
                          .RootEditableElement();
  if (!editable)
    return;

  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  const EphemeralRange range =
      PlainTextRange(composition_start, composition_end).CreateRange(*editable);
  if (range.IsNull())
    return;

  const Position start = range.StartPosition();
  if (RootEditableElementOf(start) != editable)
    return;

  const Position end = range.EndPosition();
  if (RootEditableElementOf(end) != editable)
    return;

  Clear();

  AddCompositionUnderlines(underlines, editable, composition_start);

  has_composition_ = true;
  if (!composition_range_)
    composition_range_ = Range::Create(GetDocument());
  composition_range_->setStart(range.StartPosition());
  composition_range_->setEnd(range.EndPosition());
}

EphemeralRange InputMethodController::CompositionEphemeralRange() const {
  if (!HasComposition())
    return EphemeralRange();
  return EphemeralRange(composition_range_.Get());
}

Range* InputMethodController::CompositionRange() const {
  return HasComposition() ? composition_range_ : nullptr;
}

String InputMethodController::ComposingText() const {
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());
  return PlainText(
      CompositionEphemeralRange(),
      TextIteratorBehavior::Builder().SetEmitsOriginalText(true).Build());
}

PlainTextRange InputMethodController::GetSelectionOffsets() const {
  EphemeralRange range = FirstEphemeralRangeOf(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTreeDeprecated());
  if (range.IsNull())
    return PlainTextRange();
  ContainerNode* const editable = RootEditableElementOrTreeScopeRootNodeOf(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree().Base());
  DCHECK(editable);
  return PlainTextRange::Create(*editable, range);
}

EphemeralRange InputMethodController::EphemeralRangeForOffsets(
    const PlainTextRange& offsets) const {
  if (offsets.IsNull())
    return EphemeralRange();
  Element* root_editable_element =
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();
  if (!root_editable_element)
    return EphemeralRange();

  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  return offsets.CreateRange(*root_editable_element);
}

bool InputMethodController::SetSelectionOffsets(
    const PlainTextRange& selection_offsets) {
  return SetSelectionOffsets(selection_offsets, TypingContinuation::kEnd);
}

bool InputMethodController::SetSelectionOffsets(
    const PlainTextRange& selection_offsets,
    TypingContinuation typing_continuation) {
  const EphemeralRange range = EphemeralRangeForOffsets(selection_offsets);
  if (range.IsNull())
    return false;

  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      SetSelectionData::Builder()
          .SetShouldCloseTyping(typing_continuation == TypingContinuation::kEnd)
          .Build());
  return true;
}

bool InputMethodController::SetEditableSelectionOffsets(
    const PlainTextRange& selection_offsets) {
  return SetEditableSelectionOffsets(selection_offsets,
                                     TypingContinuation::kEnd);
}

bool InputMethodController::SetEditableSelectionOffsets(
    const PlainTextRange& selection_offsets,
    TypingContinuation typing_continuation) {
  if (!GetEditor().CanEdit())
    return false;
  return SetSelectionOffsets(selection_offsets, typing_continuation);
}

PlainTextRange InputMethodController::CreateRangeForSelection(
    int start,
    int end,
    size_t text_length) const {
  // In case of exceeding the left boundary.
  start = std::max(start, 0);
  end = std::max(end, start);

  Element* root_editable_element =
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();
  if (!root_editable_element)
    return PlainTextRange();
  const EphemeralRange& range =
      EphemeralRange::RangeOfContents(*root_editable_element);
  if (range.IsNull())
    return PlainTextRange();

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .SetEmitsCharactersBetweenAllVisiblePositions(true)
          .Build();
  TextIterator it(range.StartPosition(), range.EndPosition(), behavior);

  int right_boundary = 0;
  for (; !it.AtEnd(); it.Advance())
    right_boundary += it.length();

  if (HasComposition())
    right_boundary -= CompositionRange()->GetText().length();

  right_boundary += text_length;

  // In case of exceeding the right boundary.
  start = std::min(start, right_boundary);
  end = std::min(end, right_boundary);

  return PlainTextRange(start, end);
}

bool InputMethodController::MoveCaret(int new_caret_position) {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  PlainTextRange selected_range =
      CreateRangeForSelection(new_caret_position, new_caret_position, 0);
  if (selected_range.IsNull())
    return false;
  return SetEditableSelectionOffsets(selected_range);
}

void InputMethodController::ExtendSelectionAndDelete(int before, int after) {
  if (!GetEditor().CanEdit())
    return;
  PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull())
    return;

  // A common call of before=1 and after=0 will fail if the last character
  // is multi-code-word UTF-16, including both multi-16bit code-points and
  // Unicode combining character sequences of multiple single-16bit code-
  // points (officially called "compositions"). Try more until success.
  // http://crbug.com/355995
  //
  // FIXME: Note that this is not an ideal solution when this function is
  // called to implement "backspace". In that case, there should be some call
  // that will not delete a full multi-code-point composition but rather
  // only the last code-point so that it's possible for a user to correct
  // a composition without starting it from the beginning.
  // http://crbug.com/37993
  do {
    if (!SetSelectionOffsets(PlainTextRange(
            std::max(static_cast<int>(selection_offsets.Start()) - before, 0),
            selection_offsets.End() + after)))
      return;
    if (before == 0)
      break;
    ++before;
  } while (GetFrame()
                   .Selection()
                   .ComputeVisibleSelectionInDOMTreeDeprecated()
                   .Start() == GetFrame()
                                   .Selection()
                                   .ComputeVisibleSelectionInDOMTreeDeprecated()
                                   .End() &&
           before <= static_cast<int>(selection_offsets.Start()));
  // TODO(chongz): Find a way to distinguish Forward and Backward.
  Node* target = GetDocument().FocusedElement();
  if (target) {
    DispatchBeforeInputEditorCommand(
        target, InputEvent::InputType::kDeleteContentBackward,
        TargetRangesForInputEvent(*target));
  }
  TypingCommand::DeleteSelection(GetDocument());
}

// TODO(yabinh): We should reduce the number of selectionchange events.
void InputMethodController::DeleteSurroundingText(int before, int after) {
  if (!GetEditor().CanEdit())
    return;
  const PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull())
    return;
  Element* const root_editable_element =
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();
  if (!root_editable_element)
    return;
  int selection_start = static_cast<int>(selection_offsets.Start());
  int selection_end = static_cast<int>(selection_offsets.End());

  // Select the text to be deleted before SelectionState::kStart.
  if (before > 0 && selection_start > 0) {
    // In case of exceeding the left boundary.
    const int start = std::max(selection_start - before, 0);

    const EphemeralRange& range =
        PlainTextRange(0, start).CreateRange(*root_editable_element);
    if (range.IsNull())
      return;
    const Position& position = range.EndPosition();

    // Adjust the start of selection for multi-code text(a grapheme cluster
    // contains more than one code point). TODO(yabinh): Adjustment should be
    // based on code point instead of grapheme cluster.
    const size_t diff = ComputeDistanceToLeftGraphemeBoundary(position);
    const int adjusted_start = start - static_cast<int>(diff);
    if (!SetSelectionOffsets(PlainTextRange(adjusted_start, selection_start)))
      return;
    TypingCommand::DeleteSelection(GetDocument());

    selection_end = selection_end - (selection_start - adjusted_start);
    selection_start = adjusted_start;
  }

  // Select the text to be deleted after SelectionState::kEnd.
  if (after > 0) {
    // Adjust the deleted range in case of exceeding the right boundary.
    const PlainTextRange range(0, selection_end + after);
    if (range.IsNull())
      return;
    const EphemeralRange& valid_range =
        range.CreateRange(*root_editable_element);
    if (valid_range.IsNull())
      return;
    const int end =
        PlainTextRange::Create(*root_editable_element, valid_range).End();
    const Position& position = valid_range.EndPosition();

    // Adjust the end of selection for multi-code text. TODO(yabinh): Adjustment
    // should be based on code point instead of grapheme cluster.
    const size_t diff = ComputeDistanceToRightGraphemeBoundary(position);
    const int adjusted_end = end + static_cast<int>(diff);
    if (!SetSelectionOffsets(PlainTextRange(selection_end, adjusted_end)))
      return;
    TypingCommand::DeleteSelection(GetDocument());
  }

  SetSelectionOffsets(PlainTextRange(selection_start, selection_end));
}

void InputMethodController::DeleteSurroundingTextInCodePoints(int before,
                                                              int after) {
  DCHECK_GE(before, 0);
  DCHECK_GE(after, 0);
  if (!GetEditor().CanEdit())
    return;
  const PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull())
    return;
  Element* const root_editable_element =
      GetFrame().Selection().RootEditableElementOrDocumentElement();
  if (!root_editable_element)
    return;

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .Build();
  const String& text = PlainText(
      EphemeralRange::RangeOfContents(*root_editable_element), behavior);

  // 8-bit characters are Latin-1 characters, so the deletion lengths are
  // trivial.
  if (text.Is8Bit())
    return DeleteSurroundingText(before, after);

  const int selection_start = static_cast<int>(selection_offsets.Start());
  const int selection_end = static_cast<int>(selection_offsets.End());

  const int before_length =
      CalculateBeforeDeletionLengthsInCodePoints(text, before, selection_start);
  if (IsInvalidDeletionLength(before_length))
    return;
  const int after_length =
      CalculateAfterDeletionLengthsInCodePoints(text, after, selection_end);
  if (IsInvalidDeletionLength(after_length))
    return;

  return DeleteSurroundingText(before_length, after_length);
}

WebTextInputInfo InputMethodController::TextInputInfo() const {
  WebTextInputInfo info;
  if (!IsAvailable())
    return info;

  if (!GetFrame().Selection().IsAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return info;
  }
  Element* element = RootEditableElementOfSelection(GetFrame().Selection());
  if (!element)
    return info;

  info.input_mode = InputModeOfFocusedElement();
  info.type = TextInputType();
  info.flags = TextInputFlags();
  if (info.type == kWebTextInputTypeNone)
    return info;

  if (!GetFrame().GetEditor().CanEdit())
    return info;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  // Emits an object replacement character for each replaced element so that
  // it is exposed to IME and thus could be deleted by IME on android.
  info.value = PlainText(EphemeralRange::RangeOfContents(*element),
                         TextIteratorBehavior::Builder()
                             .SetEmitsObjectReplacementCharacter(true)
                             .SetEmitsSpaceForNbsp(true)
                             .Build());

  if (info.value.IsEmpty())
    return info;

  EphemeralRange first_range = FirstEphemeralRangeOf(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTreeDeprecated());
  if (first_range.IsNotNull()) {
    PlainTextRange plain_text_range(
        PlainTextRange::Create(*element, first_range));
    if (plain_text_range.IsNotNull()) {
      info.selection_start = plain_text_range.Start();
      info.selection_end = plain_text_range.End();
    }
  }

  EphemeralRange range = CompositionEphemeralRange();
  if (range.IsNotNull()) {
    PlainTextRange plain_text_range(PlainTextRange::Create(*element, range));
    if (plain_text_range.IsNotNull()) {
      info.composition_start = plain_text_range.Start();
      info.composition_end = plain_text_range.End();
    }
  }

  return info;
}

int InputMethodController::TextInputFlags() const {
  Element* element = GetDocument().FocusedElement();
  if (!element)
    return kWebTextInputFlagNone;

  int flags = 0;

  const AtomicString& autocomplete =
      element->getAttribute(HTMLNames::autocompleteAttr);
  if (autocomplete == "on")
    flags |= kWebTextInputFlagAutocompleteOn;
  else if (autocomplete == "off")
    flags |= kWebTextInputFlagAutocompleteOff;

  const AtomicString& autocorrect =
      element->getAttribute(HTMLNames::autocorrectAttr);
  if (autocorrect == "on")
    flags |= kWebTextInputFlagAutocorrectOn;
  else if (autocorrect == "off")
    flags |= kWebTextInputFlagAutocorrectOff;

  SpellcheckAttributeState spellcheck = element->GetSpellcheckAttributeState();
  if (spellcheck == kSpellcheckAttributeTrue)
    flags |= kWebTextInputFlagSpellcheckOn;
  else if (spellcheck == kSpellcheckAttributeFalse)
    flags |= kWebTextInputFlagSpellcheckOff;

  if (IsTextControlElement(element)) {
    TextControlElement* text_control = ToTextControlElement(element);
    if (text_control->SupportsAutocapitalize()) {
      DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
      DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
      DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
      DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

      const AtomicString& autocapitalize = text_control->autocapitalize();
      if (autocapitalize == none)
        flags |= kWebTextInputFlagAutocapitalizeNone;
      else if (autocapitalize == characters)
        flags |= kWebTextInputFlagAutocapitalizeCharacters;
      else if (autocapitalize == words)
        flags |= kWebTextInputFlagAutocapitalizeWords;
      else if (autocapitalize == sentences)
        flags |= kWebTextInputFlagAutocapitalizeSentences;
      else
        NOTREACHED();
    }
  }

  return flags;
}

int InputMethodController::ComputeWebTextInputNextPreviousFlags() const {
  Element* const element = GetDocument().FocusedElement();
  if (!element)
    return kWebTextInputFlagNone;

  Page* page = GetDocument().GetPage();
  if (!page)
    return kWebTextInputFlagNone;

  int flags = kWebTextInputFlagNone;
  if (page->GetFocusController().NextFocusableElementInForm(
          element, kWebFocusTypeForward))
    flags |= kWebTextInputFlagHaveNextFocusableElement;

  if (page->GetFocusController().NextFocusableElementInForm(
          element, kWebFocusTypeBackward))
    flags |= kWebTextInputFlagHavePreviousFocusableElement;

  return flags;
}

WebTextInputMode InputMethodController::InputModeOfFocusedElement() const {
  if (!RuntimeEnabledFeatures::InputModeAttributeEnabled())
    return kWebTextInputModeDefault;

  AtomicString mode = GetInputModeAttribute(GetDocument().FocusedElement());

  if (mode.IsEmpty())
    return kWebTextInputModeDefault;
  if (mode == InputModeNames::verbatim)
    return kWebTextInputModeVerbatim;
  if (mode == InputModeNames::latin)
    return kWebTextInputModeLatin;
  if (mode == InputModeNames::latin_name)
    return kWebTextInputModeLatinName;
  if (mode == InputModeNames::latin_prose)
    return kWebTextInputModeLatinProse;
  if (mode == InputModeNames::full_width_latin)
    return kWebTextInputModeFullWidthLatin;
  if (mode == InputModeNames::kana)
    return kWebTextInputModeKana;
  if (mode == InputModeNames::kana_name)
    return kWebTextInputModeKanaName;
  if (mode == InputModeNames::katakana)
    return kWebTextInputModeKataKana;
  if (mode == InputModeNames::numeric)
    return kWebTextInputModeNumeric;
  if (mode == InputModeNames::tel)
    return kWebTextInputModeTel;
  if (mode == InputModeNames::email)
    return kWebTextInputModeEmail;
  if (mode == InputModeNames::url)
    return kWebTextInputModeUrl;
  return kWebTextInputModeDefault;
}

WebTextInputType InputMethodController::TextInputType() const {
  if (!GetFrame().Selection().IsAvailable()) {
    // "mouse-capture-inside-shadow.html" reaches here.
    return kWebTextInputTypeNone;
  }

  // It's important to preserve the equivalence of textInputInfo().type and
  // textInputType(), so perform the same rootEditableElement() existence check
  // here for consistency.
  if (!RootEditableElementOfSelection(GetFrame().Selection()))
    return kWebTextInputTypeNone;

  if (!IsAvailable())
    return kWebTextInputTypeNone;

  Element* element = GetDocument().FocusedElement();
  if (!element)
    return kWebTextInputTypeNone;

  if (isHTMLInputElement(*element)) {
    HTMLInputElement& input = toHTMLInputElement(*element);
    const AtomicString& type = input.type();

    if (input.IsDisabledOrReadOnly())
      return kWebTextInputTypeNone;

    if (type == InputTypeNames::password)
      return kWebTextInputTypePassword;
    if (type == InputTypeNames::search)
      return kWebTextInputTypeSearch;
    if (type == InputTypeNames::email)
      return kWebTextInputTypeEmail;
    if (type == InputTypeNames::number)
      return kWebTextInputTypeNumber;
    if (type == InputTypeNames::tel)
      return kWebTextInputTypeTelephone;
    if (type == InputTypeNames::url)
      return kWebTextInputTypeURL;
    if (type == InputTypeNames::text)
      return kWebTextInputTypeText;

    return kWebTextInputTypeNone;
  }

  if (isHTMLTextAreaElement(*element)) {
    if (toHTMLTextAreaElement(*element).IsDisabledOrReadOnly())
      return kWebTextInputTypeNone;
    return kWebTextInputTypeTextArea;
  }

  if (element->IsHTMLElement()) {
    if (ToHTMLElement(element)->IsDateTimeFieldElement())
      return kWebTextInputTypeDateTimeField;
  }

  GetDocument().UpdateStyleAndLayoutTree();
  if (HasEditableStyle(*element))
    return kWebTextInputTypeContentEditable;

  return kWebTextInputTypeNone;
}

void InputMethodController::WillChangeFocus() {
  FinishComposingText(kKeepSelection);
}

DEFINE_TRACE(InputMethodController) {
  visitor->Trace(frame_);
  visitor->Trace(composition_range_);
  SynchronousMutationObserver::Trace(visitor);
}

}  // namespace blink
