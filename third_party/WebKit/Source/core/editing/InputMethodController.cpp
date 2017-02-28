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
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
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

namespace blink {

namespace {

void dispatchCompositionUpdateEvent(LocalFrame& frame, const String& text) {
  Element* target = frame.document()->focusedElement();
  if (!target)
    return;

  CompositionEvent* event = CompositionEvent::create(
      EventTypeNames::compositionupdate, frame.domWindow(), text);
  target->dispatchEvent(event);
}

void dispatchCompositionEndEvent(LocalFrame& frame, const String& text) {
  Element* target = frame.document()->focusedElement();
  if (!target)
    return;

  CompositionEvent* event = CompositionEvent::create(
      EventTypeNames::compositionend, frame.domWindow(), text);
  target->dispatchEvent(event);
}

bool needsIncrementalInsertion(const LocalFrame& frame, const String& newText) {
  // No need to apply incremental insertion if it doesn't support formated text.
  if (!frame.editor().canEditRichly())
    return false;

  // No need to apply incremental insertion if the old text (text to be
  // replaced) or the new text (text to be inserted) is empty.
  if (frame.selectedText().isEmpty() || newText.isEmpty())
    return false;

  return true;
}

void dispatchBeforeInputFromComposition(EventTarget* target,
                                        InputEvent::InputType inputType,
                                        const String& data) {
  if (!RuntimeEnabledFeatures::inputEventEnabled())
    return;
  if (!target)
    return;
  // TODO(chongz): Pass appropriate |ranges| after it's defined on spec.
  // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
  InputEvent* beforeInputEvent = InputEvent::createBeforeInput(
      inputType, data, InputEvent::NotCancelable,
      InputEvent::EventIsComposing::IsComposing, nullptr);
  target->dispatchEvent(beforeInputEvent);
}

// Used to insert/replace text during composition update and confirm
// composition.
// Procedure:
//   1. Fire 'beforeinput' event for (TODO(chongz): deleted composed text) and
//      inserted text
//   2. Fire 'compositionupdate' event
//   3. Fire TextEvent and modify DOM
//   TODO(chongz): 4. Fire 'input' event
void insertTextDuringCompositionWithEvents(
    LocalFrame& frame,
    const String& text,
    TypingCommand::Options options,
    TypingCommand::TextCompositionType compositionType) {
  DCHECK(compositionType ==
             TypingCommand::TextCompositionType::TextCompositionUpdate ||
         compositionType ==
             TypingCommand::TextCompositionType::TextCompositionConfirm ||
         compositionType ==
             TypingCommand::TextCompositionType::TextCompositionCancel)
      << "compositionType should be TextCompositionUpdate or "
         "TextCompositionConfirm  or TextCompositionCancel, but got "
      << static_cast<int>(compositionType);
  if (!frame.document())
    return;

  Element* target = frame.document()->focusedElement();
  if (!target)
    return;

  dispatchBeforeInputFromComposition(
      target, InputEvent::InputType::InsertCompositionText, text);

  // 'beforeinput' event handler may destroy document.
  if (!frame.document())
    return;

  dispatchCompositionUpdateEvent(frame, text);
  // 'compositionupdate' event handler may destroy document.
  if (!frame.document())
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame.document()->updateStyleAndLayoutIgnorePendingStylesheets();

  const bool isIncrementalInsertion = needsIncrementalInsertion(frame, text);

  switch (compositionType) {
    case TypingCommand::TextCompositionType::TextCompositionUpdate:
    case TypingCommand::TextCompositionType::TextCompositionConfirm:
      // Calling |TypingCommand::insertText()| with empty text will result in an
      // incorrect ending selection. We need to delete selection first.
      // https://crbug.com/693481
      if (text.isEmpty())
        TypingCommand::deleteSelection(*frame.document(), 0);
      TypingCommand::insertText(*frame.document(), text, options,
                                compositionType, isIncrementalInsertion);
      break;
    case TypingCommand::TextCompositionType::TextCompositionCancel:
      // TODO(chongz): Use TypingCommand::insertText after TextEvent was
      // removed. (Removed from spec since 2012)
      // See TextEvent.idl.
      frame.eventHandler().handleTextInputEvent(text, 0,
                                                TextEventInputComposition);
      break;
    default:
      NOTREACHED();
  }
  // TODO(chongz): Fire 'input' event.
}

AtomicString getInputModeAttribute(Element* element) {
  if (!element)
    return AtomicString();

  bool queryAttribute = false;
  if (isHTMLInputElement(*element)) {
    queryAttribute = toHTMLInputElement(*element).supportsInputModeAttribute();
  } else if (isHTMLTextAreaElement(*element)) {
    queryAttribute = true;
  } else {
    element->document().updateStyleAndLayoutTree();
    if (hasEditableStyle(*element))
      queryAttribute = true;
  }

  if (!queryAttribute)
    return AtomicString();

  // TODO(dtapuska): We may wish to restrict this to a yet to be proposed
  // <contenteditable> or <richtext> element Mozilla discussed at TPAC 2016.
  return element->fastGetAttribute(HTMLNames::inputmodeAttr).lower();
}

constexpr int invalidDeletionLength = -1;
constexpr bool isInvalidDeletionLength(const int length) {
  return length == invalidDeletionLength;
}

int calculateBeforeDeletionLengthsInCodePoints(
    const String& text,
    const int beforeLengthInCodePoints,
    const int selectionStart) {
  DCHECK_GE(beforeLengthInCodePoints, 0);
  DCHECK_GE(selectionStart, 0);
  DCHECK_LE(selectionStart, static_cast<int>(text.length()));

  const UChar* uText = text.characters16();
  BackwardCodePointStateMachine backwardMachine;
  int counter = beforeLengthInCodePoints;
  int deletionStart = selectionStart;
  while (counter > 0 && deletionStart > 0) {
    const TextSegmentationMachineState state =
        backwardMachine.feedPrecedingCodeUnit(uText[deletionStart - 1]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::Invalid)
      return invalidDeletionLength;

    if (backwardMachine.atCodePointBoundary())
      --counter;
    --deletionStart;
  }
  if (!backwardMachine.atCodePointBoundary())
    return invalidDeletionLength;

  const int offset = backwardMachine.getBoundaryOffset();
  DCHECK_EQ(-offset, selectionStart - deletionStart);
  return -offset;
}

int calculateAfterDeletionLengthsInCodePoints(const String& text,
                                              const int afterLengthInCodePoints,
                                              const int selectionEnd) {
  DCHECK_GE(afterLengthInCodePoints, 0);
  DCHECK_GE(selectionEnd, 0);
  const int length = text.length();
  DCHECK_LE(selectionEnd, length);

  const UChar* uText = text.characters16();
  ForwardCodePointStateMachine forwardMachine;
  int counter = afterLengthInCodePoints;
  int deletionEnd = selectionEnd;
  while (counter > 0 && deletionEnd < length) {
    const TextSegmentationMachineState state =
        forwardMachine.feedFollowingCodeUnit(uText[deletionEnd]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::Invalid)
      return invalidDeletionLength;

    if (forwardMachine.atCodePointBoundary())
      --counter;
    ++deletionEnd;
  }
  if (!forwardMachine.atCodePointBoundary())
    return invalidDeletionLength;

  const int offset = forwardMachine.getBoundaryOffset();
  DCHECK_EQ(offset, deletionEnd - selectionEnd);
  return offset;
}

}  // anonymous namespace

InputMethodController* InputMethodController::create(LocalFrame& frame) {
  return new InputMethodController(frame);
}

InputMethodController::InputMethodController(LocalFrame& frame)
    : m_frame(&frame), m_hasComposition(false) {}

InputMethodController::~InputMethodController() = default;

bool InputMethodController::isAvailable() const {
  return frame().document();
}

Document& InputMethodController::document() const {
  DCHECK(isAvailable());
  return *frame().document();
}

bool InputMethodController::hasComposition() const {
  return m_hasComposition && !m_compositionRange->collapsed() &&
         m_compositionRange->isConnected();
}

inline Editor& InputMethodController::editor() const {
  return frame().editor();
}

void InputMethodController::clear() {
  m_hasComposition = false;
  if (m_compositionRange) {
    m_compositionRange->setStart(&document(), 0);
    m_compositionRange->collapse(true);
  }
  document().markers().removeMarkers(DocumentMarker::Composition);
}

void InputMethodController::contextDestroyed(Document*) {
  clear();
  m_compositionRange = nullptr;
}

void InputMethodController::documentAttached(Document* document) {
  DCHECK(document);
  setContext(document);
}

void InputMethodController::selectComposition() const {
  const EphemeralRange range = compositionEphemeralRange();
  if (range.isNull())
    return;

  // The composition can start inside a composed character sequence, so we have
  // to override checks. See <http://bugs.webkit.org/show_bug.cgi?id=15781>
  frame().selection().setSelection(
      SelectionInDOMTree::Builder().setBaseAndExtent(range).build(), 0);
}

bool InputMethodController::finishComposingText(
    ConfirmCompositionBehavior confirmBehavior) {
  if (!hasComposition())
    return false;

  const String& composing = composingText();

  if (confirmBehavior == KeepSelection) {
    // Do not dismiss handles even if we are moving selection, because we will
    // eventually move back to the old selection offsets.
    const bool isHandleVisible = frame().selection().isHandleVisible();

    const PlainTextRange& oldOffsets = getSelectionOffsets();
    Editor::RevealSelectionScope revealSelectionScope(&editor());

    clear();
    dispatchCompositionEndEvent(frame(), composing);

    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    const EphemeralRange& oldSelectionRange =
        ephemeralRangeForOffsets(oldOffsets);
    if (oldSelectionRange.isNull())
      return false;
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .setBaseAndExtent(oldSelectionRange)
            .setIsHandleVisible(isHandleVisible)
            .build();
    frame().selection().setSelection(selection, FrameSelection::CloseTyping);
    return true;
  }

  Element* rootEditableElement =
      frame()
          .selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .rootEditableElement();
  if (!rootEditableElement)
    return false;
  PlainTextRange compositionRange =
      PlainTextRange::create(*rootEditableElement, *m_compositionRange);
  if (compositionRange.isNull())
    return false;

  clear();

  if (!moveCaret(compositionRange.end()))
    return false;

  dispatchCompositionEndEvent(frame(), composing);
  return true;
}

bool InputMethodController::commitText(
    const String& text,
    const Vector<CompositionUnderline>& underlines,
    int relativeCaretPosition) {
  if (hasComposition()) {
    return replaceCompositionAndMoveCaret(text, relativeCaretPosition,
                                          underlines);
  }

  // We should do nothing in this case, because:
  // 1. No need to insert text when text is empty.
  // 2. Shouldn't move caret when relativeCaretPosition == 0 to avoid
  // duplicate selection change event.
  if (!text.length() && !relativeCaretPosition)
    return false;

  return insertTextAndMoveCaret(text, relativeCaretPosition, underlines);
}

bool InputMethodController::replaceComposition(const String& text) {
  if (!hasComposition())
    return false;

  // Select the text that will be deleted or replaced.
  selectComposition();

  if (frame().selection().computeVisibleSelectionInDOMTreeDeprecated().isNone())
    return false;

  if (!isAvailable())
    return false;

  clear();

  insertTextDuringCompositionWithEvents(
      frame(), text, 0,
      TypingCommand::TextCompositionType::TextCompositionConfirm);
  // Event handler might destroy document.
  if (!isAvailable())
    return false;

  // No DOM update after 'compositionend'.
  dispatchCompositionEndEvent(frame(), text);

  return true;
}

// relativeCaretPosition is relative to the end of the text.
static int computeAbsoluteCaretPosition(size_t textStart,
                                        size_t textLength,
                                        int relativeCaretPosition) {
  return textStart + textLength + relativeCaretPosition;
}

void InputMethodController::addCompositionUnderlines(
    const Vector<CompositionUnderline>& underlines,
    ContainerNode* baseElement,
    unsigned offsetInPlainChars) {
  for (const auto& underline : underlines) {
    unsigned underlineStart = offsetInPlainChars + underline.startOffset();
    unsigned underlineEnd = offsetInPlainChars + underline.endOffset();

    EphemeralRange ephemeralLineRange =
        PlainTextRange(underlineStart, underlineEnd).createRange(*baseElement);
    if (ephemeralLineRange.isNull())
      continue;

    document().markers().addCompositionMarker(
        ephemeralLineRange.startPosition(), ephemeralLineRange.endPosition(),
        underline.color(), underline.thick(), underline.backgroundColor());
  }
}

bool InputMethodController::replaceCompositionAndMoveCaret(
    const String& text,
    int relativeCaretPosition,
    const Vector<CompositionUnderline>& underlines) {
  Element* rootEditableElement =
      frame()
          .selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .rootEditableElement();
  if (!rootEditableElement)
    return false;
  DCHECK(hasComposition());
  PlainTextRange compositionRange =
      PlainTextRange::create(*rootEditableElement, *m_compositionRange);
  if (compositionRange.isNull())
    return false;
  int textStart = compositionRange.start();

  if (!replaceComposition(text))
    return false;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  addCompositionUnderlines(underlines, rootEditableElement, textStart);

  int absoluteCaretPosition = computeAbsoluteCaretPosition(
      textStart, text.length(), relativeCaretPosition);
  return moveCaret(absoluteCaretPosition);
}

bool InputMethodController::insertText(const String& text) {
  if (dispatchBeforeInputInsertText(document().focusedElement(), text) !=
      DispatchEventResult::NotCanceled)
    return false;
  editor().insertText(text, 0);
  return true;
}

bool InputMethodController::insertTextAndMoveCaret(
    const String& text,
    int relativeCaretPosition,
    const Vector<CompositionUnderline>& underlines) {
  PlainTextRange selectionRange = getSelectionOffsets();
  if (selectionRange.isNull())
    return false;
  int textStart = selectionRange.start();

  if (text.length()) {
    if (!insertText(text))
      return false;

    Element* rootEditableElement =
        frame()
            .selection()
            .computeVisibleSelectionInDOMTreeDeprecated()
            .rootEditableElement();
    if (rootEditableElement) {
      addCompositionUnderlines(underlines, rootEditableElement, textStart);
    }
  }

  int absoluteCaretPosition = computeAbsoluteCaretPosition(
      textStart, text.length(), relativeCaretPosition);
  return moveCaret(absoluteCaretPosition);
}

void InputMethodController::cancelComposition() {
  if (!hasComposition())
    return;

  Editor::RevealSelectionScope revealSelectionScope(&editor());

  if (frame().selection().computeVisibleSelectionInDOMTreeDeprecated().isNone())
    return;

  clear();

  insertTextDuringCompositionWithEvents(
      frame(), emptyString, 0,
      TypingCommand::TextCompositionType::TextCompositionCancel);
  // Event handler might destroy document.
  if (!isAvailable())
    return;

  // An open typing command that disagrees about current selection would cause
  // issues with typing later on.
  TypingCommand::closeTyping(m_frame);

  // No DOM update after 'compositionend'.
  dispatchCompositionEndEvent(frame(), emptyString);
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest left grapheme boundary.
static size_t computeDistanceToLeftGraphemeBoundary(const Position& position) {
  const Position& adjustedPosition = previousPositionOf(
      nextPositionOf(position, PositionMoveType::GraphemeCluster),
      PositionMoveType::GraphemeCluster);
  DCHECK_EQ(position.anchorNode(), adjustedPosition.anchorNode());
  DCHECK_GE(position.computeOffsetInContainerNode(),
            adjustedPosition.computeOffsetInContainerNode());
  return static_cast<size_t>(position.computeOffsetInContainerNode() -
                             adjustedPosition.computeOffsetInContainerNode());
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest right grapheme boundary.
static size_t computeDistanceToRightGraphemeBoundary(const Position& position) {
  const Position& adjustedPosition = nextPositionOf(
      previousPositionOf(position, PositionMoveType::GraphemeCluster),
      PositionMoveType::GraphemeCluster);
  DCHECK_EQ(position.anchorNode(), adjustedPosition.anchorNode());
  DCHECK_GE(adjustedPosition.computeOffsetInContainerNode(),
            position.computeOffsetInContainerNode());
  return static_cast<size_t>(adjustedPosition.computeOffsetInContainerNode() -
                             position.computeOffsetInContainerNode());
}

void InputMethodController::setComposition(
    const String& text,
    const Vector<CompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd) {
  Editor::RevealSelectionScope revealSelectionScope(&editor());

  // Updates styles before setting selection for composition to prevent
  // inserting the previous composition text into text nodes oddly.
  // See https://bugs.webkit.org/show_bug.cgi?id=46868
  document().updateStyleAndLayoutTree();

  selectComposition();

  if (frame().selection().computeVisibleSelectionInDOMTreeDeprecated().isNone())
    return;

  Element* target = document().focusedElement();
  if (!target)
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  PlainTextRange selectedRange = createSelectionRangeForSetComposition(
      selectionStart, selectionEnd, text.length());

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
  if (text.isEmpty()) {
    if (hasComposition()) {
      Editor::RevealSelectionScope revealSelectionScope(&editor());
      replaceComposition(emptyString);
    } else {
      // It's weird to call |setComposition()| with empty text outside
      // composition, however some IME (e.g. Japanese IBus-Anthy) did this, so
      // we simply delete selection without sending extra events.
      TypingCommand::deleteSelection(document(),
                                     TypingCommand::PreventSpellChecking);
    }

    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    setEditableSelectionOffsets(selectedRange);
    return;
  }

  // We should send a 'compositionstart' event only when the given text is not
  // empty because this function doesn't create a composition node when the text
  // is empty.
  if (!hasComposition()) {
    target->dispatchEvent(
        CompositionEvent::create(EventTypeNames::compositionstart,
                                 frame().domWindow(), frame().selectedText()));
    if (!isAvailable())
      return;
  }

  DCHECK(!text.isEmpty());

  clear();

  insertTextDuringCompositionWithEvents(
      frame(), text,
      TypingCommand::SelectInsertedText | TypingCommand::PreventSpellChecking,
      TypingCommand::TextCompositionUpdate);
  // Event handlers might destroy document.
  if (!isAvailable())
    return;

  // TODO(yosin): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Find out what node has the composition now.
  Position base = mostForwardCaretPosition(
      frame().selection().computeVisibleSelectionInDOMTree().base());
  Node* baseNode = base.anchorNode();
  if (!baseNode || !baseNode->isTextNode())
    return;

  Position extent =
      frame().selection().computeVisibleSelectionInDOMTree().extent();
  Node* extentNode = extent.anchorNode();

  unsigned extentOffset = extent.computeOffsetInContainerNode();
  unsigned baseOffset = base.computeOffsetInContainerNode();

  m_hasComposition = true;
  if (!m_compositionRange)
    m_compositionRange = Range::create(document());
  m_compositionRange->setStart(baseNode, baseOffset);
  m_compositionRange->setEnd(extentNode, extentOffset);

  if (baseNode->layoutObject())
    baseNode->layoutObject()->setShouldDoFullPaintInvalidation();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // We shouldn't close typing in the middle of setComposition.
  setEditableSelectionOffsets(selectedRange, NotUserTriggered);

  if (underlines.isEmpty()) {
    document().markers().addCompositionMarker(
        m_compositionRange->startPosition(), m_compositionRange->endPosition(),
        Color::black, false,
        LayoutTheme::theme().platformDefaultCompositionBackgroundColor());
    return;
  }

  const PlainTextRange compositionPlainTextRange =
      PlainTextRange::create(*baseNode->parentNode(), *m_compositionRange);
  addCompositionUnderlines(underlines, baseNode->parentNode(),
                           compositionPlainTextRange.start());
}

PlainTextRange InputMethodController::createSelectionRangeForSetComposition(
    int selectionStart,
    int selectionEnd,
    size_t textLength) const {
  const int selectionOffsetsStart =
      static_cast<int>(getSelectionOffsets().start());
  const int start = selectionOffsetsStart + selectionStart;
  const int end = selectionOffsetsStart + selectionEnd;
  return createRangeForSelection(start, end, textLength);
}

void InputMethodController::setCompositionFromExistingText(
    const Vector<CompositionUnderline>& underlines,
    unsigned compositionStart,
    unsigned compositionEnd) {
  Element* editable = frame()
                          .selection()
                          .computeVisibleSelectionInDOMTreeDeprecated()
                          .rootEditableElement();
  if (!editable)
    return;

  DCHECK(!document().needsLayoutTreeUpdate());

  const EphemeralRange range =
      PlainTextRange(compositionStart, compositionEnd).createRange(*editable);
  if (range.isNull())
    return;

  const Position start = range.startPosition();
  if (rootEditableElementOf(start) != editable)
    return;

  const Position end = range.endPosition();
  if (rootEditableElementOf(end) != editable)
    return;

  clear();

  addCompositionUnderlines(underlines, editable, compositionStart);

  m_hasComposition = true;
  if (!m_compositionRange)
    m_compositionRange = Range::create(document());
  m_compositionRange->setStart(range.startPosition());
  m_compositionRange->setEnd(range.endPosition());
}

EphemeralRange InputMethodController::compositionEphemeralRange() const {
  if (!hasComposition())
    return EphemeralRange();
  return EphemeralRange(m_compositionRange.get());
}

Range* InputMethodController::compositionRange() const {
  return hasComposition() ? m_compositionRange : nullptr;
}

String InputMethodController::composingText() const {
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document().lifecycle());
  return plainText(
      compositionEphemeralRange(),
      TextIteratorBehavior::Builder().setEmitsOriginalText(true).build());
}

PlainTextRange InputMethodController::getSelectionOffsets() const {
  EphemeralRange range = firstEphemeralRangeOf(
      frame().selection().computeVisibleSelectionInDOMTreeDeprecated());
  if (range.isNull())
    return PlainTextRange();
  ContainerNode* const editable = rootEditableElementOrTreeScopeRootNodeOf(
      frame().selection().computeVisibleSelectionInDOMTreeDeprecated());
  DCHECK(editable);
  return PlainTextRange::create(*editable, range);
}

EphemeralRange InputMethodController::ephemeralRangeForOffsets(
    const PlainTextRange& offsets) const {
  if (offsets.isNull())
    return EphemeralRange();
  Element* rootEditableElement =
      frame()
          .selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .rootEditableElement();
  if (!rootEditableElement)
    return EphemeralRange();

  DCHECK(!document().needsLayoutTreeUpdate());

  return offsets.createRange(*rootEditableElement);
}

bool InputMethodController::setSelectionOffsets(
    const PlainTextRange& selectionOffsets,
    FrameSelection::SetSelectionOptions options) {
  const EphemeralRange range = ephemeralRangeForOffsets(selectionOffsets);
  if (range.isNull())
    return false;

  return frame().selection().setSelectedRange(
      range, VP_DEFAULT_AFFINITY, SelectionDirectionalMode::NonDirectional,
      options);
}

bool InputMethodController::setEditableSelectionOffsets(
    const PlainTextRange& selectionOffsets,
    FrameSelection::SetSelectionOptions options) {
  if (!editor().canEdit())
    return false;
  return setSelectionOffsets(selectionOffsets, options);
}

PlainTextRange InputMethodController::createRangeForSelection(
    int start,
    int end,
    size_t textLength) const {
  // In case of exceeding the left boundary.
  start = std::max(start, 0);
  end = std::max(end, start);

  Element* rootEditableElement =
      frame()
          .selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .rootEditableElement();
  if (!rootEditableElement)
    return PlainTextRange();
  const EphemeralRange& range =
      EphemeralRange::rangeOfContents(*rootEditableElement);
  if (range.isNull())
    return PlainTextRange();

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .setEmitsObjectReplacementCharacter(true)
          .setEmitsCharactersBetweenAllVisiblePositions(true)
          .build();
  TextIterator it(range.startPosition(), range.endPosition(), behavior);

  int rightBoundary = 0;
  for (; !it.atEnd(); it.advance())
    rightBoundary += it.length();

  if (hasComposition())
    rightBoundary -= compositionRange()->text().length();

  rightBoundary += textLength;

  // In case of exceeding the right boundary.
  start = std::min(start, rightBoundary);
  end = std::min(end, rightBoundary);

  return PlainTextRange(start, end);
}

bool InputMethodController::moveCaret(int newCaretPosition) {
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  PlainTextRange selectedRange =
      createRangeForSelection(newCaretPosition, newCaretPosition, 0);
  if (selectedRange.isNull())
    return false;
  return setEditableSelectionOffsets(selectedRange);
}

void InputMethodController::extendSelectionAndDelete(int before, int after) {
  if (!editor().canEdit())
    return;
  PlainTextRange selectionOffsets(getSelectionOffsets());
  if (selectionOffsets.isNull())
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
    if (!setSelectionOffsets(PlainTextRange(
            std::max(static_cast<int>(selectionOffsets.start()) - before, 0),
            selectionOffsets.end() + after)))
      return;
    if (before == 0)
      break;
    ++before;
  } while (frame().selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .start() ==
               frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .end() &&
           before <= static_cast<int>(selectionOffsets.start()));
  // TODO(chongz): Find a way to distinguish Forward and Backward.
  Node* target = document().focusedElement();
  if (target) {
    dispatchBeforeInputEditorCommand(
        target, InputEvent::InputType::DeleteContentBackward,
        targetRangesForInputEvent(*target));
  }
  TypingCommand::deleteSelection(document());
}

// TODO(yabinh): We should reduce the number of selectionchange events.
void InputMethodController::deleteSurroundingText(int before, int after) {
  if (!editor().canEdit())
    return;
  const PlainTextRange selectionOffsets(getSelectionOffsets());
  if (selectionOffsets.isNull())
    return;
  Element* const rootEditableElement =
      frame()
          .selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .rootEditableElement();
  if (!rootEditableElement)
    return;
  int selectionStart = static_cast<int>(selectionOffsets.start());
  int selectionEnd = static_cast<int>(selectionOffsets.end());

  // Select the text to be deleted before selectionStart.
  if (before > 0 && selectionStart > 0) {
    // In case of exceeding the left boundary.
    const int start = std::max(selectionStart - before, 0);

    const EphemeralRange& range =
        PlainTextRange(0, start).createRange(*rootEditableElement);
    if (range.isNull())
      return;
    const Position& position = range.endPosition();

    // Adjust the start of selection for multi-code text(a grapheme cluster
    // contains more than one code point). TODO(yabinh): Adjustment should be
    // based on code point instead of grapheme cluster.
    const size_t diff = computeDistanceToLeftGraphemeBoundary(position);
    const int adjustedStart = start - static_cast<int>(diff);
    if (!setSelectionOffsets(PlainTextRange(adjustedStart, selectionStart)))
      return;
    TypingCommand::deleteSelection(document());

    selectionEnd = selectionEnd - (selectionStart - adjustedStart);
    selectionStart = adjustedStart;
  }

  // Select the text to be deleted after selectionEnd.
  if (after > 0) {
    // Adjust the deleted range in case of exceeding the right boundary.
    const PlainTextRange range(0, selectionEnd + after);
    if (range.isNull())
      return;
    const EphemeralRange& validRange = range.createRange(*rootEditableElement);
    if (validRange.isNull())
      return;
    const int end =
        PlainTextRange::create(*rootEditableElement, validRange).end();
    const Position& position = validRange.endPosition();

    // Adjust the end of selection for multi-code text. TODO(yabinh): Adjustment
    // should be based on code point instead of grapheme cluster.
    const size_t diff = computeDistanceToRightGraphemeBoundary(position);
    const int adjustedEnd = end + static_cast<int>(diff);
    if (!setSelectionOffsets(PlainTextRange(selectionEnd, adjustedEnd)))
      return;
    TypingCommand::deleteSelection(document());
  }

  setSelectionOffsets(PlainTextRange(selectionStart, selectionEnd));
}

void InputMethodController::deleteSurroundingTextInCodePoints(int before,
                                                              int after) {
  DCHECK_GE(before, 0);
  DCHECK_GE(after, 0);
  if (!editor().canEdit())
    return;
  const PlainTextRange selectionOffsets(getSelectionOffsets());
  if (selectionOffsets.isNull())
    return;
  Element* const rootEditableElement =
      frame().selection().rootEditableElementOrDocumentElement();
  if (!rootEditableElement)
    return;

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .setEmitsObjectReplacementCharacter(true)
          .build();
  const String& text = plainText(
      EphemeralRange::rangeOfContents(*rootEditableElement), behavior);

  // 8-bit characters are Latin-1 characters, so the deletion lengths are
  // trivial.
  if (text.is8Bit())
    return deleteSurroundingText(before, after);

  const int selectionStart = static_cast<int>(selectionOffsets.start());
  const int selectionEnd = static_cast<int>(selectionOffsets.end());

  const int beforeLength =
      calculateBeforeDeletionLengthsInCodePoints(text, before, selectionStart);
  if (isInvalidDeletionLength(beforeLength))
    return;
  const int afterLength =
      calculateAfterDeletionLengthsInCodePoints(text, after, selectionEnd);
  if (isInvalidDeletionLength(afterLength))
    return;

  return deleteSurroundingText(beforeLength, afterLength);
}

WebTextInputInfo InputMethodController::textInputInfo() const {
  WebTextInputInfo info;
  if (!isAvailable())
    return info;

  if (!frame().selection().isAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return info;
  }
  Element* element = frame()
                         .selection()
                         .computeVisibleSelectionInDOMTreeDeprecated()
                         .rootEditableElement();
  if (!element)
    return info;

  info.inputMode = inputModeOfFocusedElement();
  info.type = textInputType();
  info.flags = textInputFlags();
  if (info.type == WebTextInputTypeNone)
    return info;

  if (!frame().editor().canEdit())
    return info;

  // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document().lifecycle());

  // Emits an object replacement character for each replaced element so that
  // it is exposed to IME and thus could be deleted by IME on android.
  info.value = plainText(EphemeralRange::rangeOfContents(*element),
                         TextIteratorBehavior::Builder()
                             .setEmitsObjectReplacementCharacter(true)
                             .setEmitsSpaceForNbsp(true)
                             .build());

  if (info.value.isEmpty())
    return info;

  EphemeralRange firstRange = firstEphemeralRangeOf(
      frame().selection().computeVisibleSelectionInDOMTreeDeprecated());
  if (firstRange.isNotNull()) {
    PlainTextRange plainTextRange(PlainTextRange::create(*element, firstRange));
    if (plainTextRange.isNotNull()) {
      info.selectionStart = plainTextRange.start();
      info.selectionEnd = plainTextRange.end();
    }
  }

  EphemeralRange range = compositionEphemeralRange();
  if (range.isNotNull()) {
    PlainTextRange plainTextRange(PlainTextRange::create(*element, range));
    if (plainTextRange.isNotNull()) {
      info.compositionStart = plainTextRange.start();
      info.compositionEnd = plainTextRange.end();
    }
  }

  return info;
}

int InputMethodController::textInputFlags() const {
  Element* element = document().focusedElement();
  if (!element)
    return WebTextInputFlagNone;

  int flags = 0;

  const AtomicString& autocomplete =
      element->getAttribute(HTMLNames::autocompleteAttr);
  if (autocomplete == "on")
    flags |= WebTextInputFlagAutocompleteOn;
  else if (autocomplete == "off")
    flags |= WebTextInputFlagAutocompleteOff;

  const AtomicString& autocorrect =
      element->getAttribute(HTMLNames::autocorrectAttr);
  if (autocorrect == "on")
    flags |= WebTextInputFlagAutocorrectOn;
  else if (autocorrect == "off")
    flags |= WebTextInputFlagAutocorrectOff;

  SpellcheckAttributeState spellcheck = element->spellcheckAttributeState();
  if (spellcheck == SpellcheckAttributeTrue)
    flags |= WebTextInputFlagSpellcheckOn;
  else if (spellcheck == SpellcheckAttributeFalse)
    flags |= WebTextInputFlagSpellcheckOff;

  if (isTextControlElement(element)) {
    TextControlElement* textControl = toTextControlElement(element);
    if (textControl->supportsAutocapitalize()) {
      DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
      DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
      DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
      DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

      const AtomicString& autocapitalize = textControl->autocapitalize();
      if (autocapitalize == none)
        flags |= WebTextInputFlagAutocapitalizeNone;
      else if (autocapitalize == characters)
        flags |= WebTextInputFlagAutocapitalizeCharacters;
      else if (autocapitalize == words)
        flags |= WebTextInputFlagAutocapitalizeWords;
      else if (autocapitalize == sentences)
        flags |= WebTextInputFlagAutocapitalizeSentences;
      else
        NOTREACHED();
    }
  }

  return flags;
}

WebTextInputMode InputMethodController::inputModeOfFocusedElement() const {
  if (!RuntimeEnabledFeatures::inputModeAttributeEnabled())
    return kWebTextInputModeDefault;

  AtomicString mode = getInputModeAttribute(document().focusedElement());

  if (mode.isEmpty())
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

WebTextInputType InputMethodController::textInputType() const {
  if (!frame().selection().isAvailable()) {
    // "mouse-capture-inside-shadow.html" reaches here.
    return WebTextInputTypeNone;
  }

  // It's important to preserve the equivalence of textInputInfo().type and
  // textInputType(), so perform the same rootEditableElement() existence check
  // here for consistency.
  if (!frame()
           .selection()
           .computeVisibleSelectionInDOMTreeDeprecated()
           .rootEditableElement())
    return WebTextInputTypeNone;

  if (!isAvailable())
    return WebTextInputTypeNone;

  Element* element = document().focusedElement();
  if (!element)
    return WebTextInputTypeNone;

  if (isHTMLInputElement(*element)) {
    HTMLInputElement& input = toHTMLInputElement(*element);
    const AtomicString& type = input.type();

    if (input.isDisabledOrReadOnly())
      return WebTextInputTypeNone;

    if (type == InputTypeNames::password)
      return WebTextInputTypePassword;
    if (type == InputTypeNames::search)
      return WebTextInputTypeSearch;
    if (type == InputTypeNames::email)
      return WebTextInputTypeEmail;
    if (type == InputTypeNames::number)
      return WebTextInputTypeNumber;
    if (type == InputTypeNames::tel)
      return WebTextInputTypeTelephone;
    if (type == InputTypeNames::url)
      return WebTextInputTypeURL;
    if (type == InputTypeNames::text)
      return WebTextInputTypeText;

    return WebTextInputTypeNone;
  }

  if (isHTMLTextAreaElement(*element)) {
    if (toHTMLTextAreaElement(*element).isDisabledOrReadOnly())
      return WebTextInputTypeNone;
    return WebTextInputTypeTextArea;
  }

  if (element->isHTMLElement()) {
    if (toHTMLElement(element)->isDateTimeFieldElement())
      return WebTextInputTypeDateTimeField;
  }

  document().updateStyleAndLayoutTree();
  if (hasEditableStyle(*element))
    return WebTextInputTypeContentEditable;

  return WebTextInputTypeNone;
}

void InputMethodController::willChangeFocus() {
  finishComposingText(KeepSelection);
}

DEFINE_TRACE(InputMethodController) {
  visitor->trace(m_frame);
  visitor->trace(m_compositionRange);
  SynchronousMutationObserver::trace(visitor);
}

}  // namespace blink
