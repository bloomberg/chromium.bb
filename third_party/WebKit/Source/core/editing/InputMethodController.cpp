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

#include "config.h"
#include "core/editing/InputMethodController.h"

#include "core/events/CompositionEvent.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/dom/UserTypingGestureIndicator.h"
#include "core/editing/Editor.h"
#include "core/editing/TypingCommand.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/page/EditorClient.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

PlainTextOffsets::PlainTextOffsets()
    : m_start(kNotFound)
    , m_end(kNotFound)
{
}

PlainTextOffsets::PlainTextOffsets(int start, int end)
    : m_start(start)
    , m_end(end)
{
    ASSERT(start != kNotFound);
    ASSERT(end != kNotFound);
    ASSERT(start <= end);
}

// ----------------------------

InputMethodController::SelectionOffsetsScope::SelectionOffsetsScope(InputMethodController* inputMethodController)
    : m_inputMethodController(inputMethodController)
    , m_offsets(inputMethodController->getSelectionOffsets())
{
}

InputMethodController::SelectionOffsetsScope::~SelectionOffsetsScope()
{
    m_inputMethodController->setSelectionOffsets(m_offsets);
}

// ----------------------------

PassOwnPtr<InputMethodController> InputMethodController::create(Frame* frame)
{
    return adoptPtr(new InputMethodController(frame));
}

InputMethodController::InputMethodController(Frame* frame)
    : m_frame(frame)
    , m_compositionStart(0)
    , m_compositionEnd(0)
{
}

InputMethodController::~InputMethodController()
{
}

inline Editor& InputMethodController::editor() const
{
    return m_frame->editor();
}

inline EditorClient& InputMethodController::editorClient() const
{
    return editor().client();
}

void InputMethodController::clear()
{
    m_compositionNode = 0;
    m_customCompositionUnderlines.clear();
}

bool InputMethodController::insertTextForConfirmedComposition(const String& text)
{
    return m_frame->eventHandler()->handleTextInputEvent(text, 0, TextEventInputComposition);
}

void InputMethodController::selectComposition() const
{
    RefPtr<Range> range = compositionRange();
    if (!range)
        return;

    // The composition can start inside a composed character sequence, so we have to override checks.
    // See <http://bugs.webkit.org/show_bug.cgi?id=15781>
    VisibleSelection selection;
    selection.setWithoutValidation(range->startPosition(), range->endPosition());
    m_frame->selection().setSelection(selection, 0);
}

void InputMethodController::confirmComposition()
{
    if (!m_compositionNode)
        return;
    finishComposition(m_compositionNode->data().substring(m_compositionStart, m_compositionEnd - m_compositionStart), ConfirmComposition);
}

void InputMethodController::confirmComposition(const String& text)
{
    finishComposition(text, ConfirmComposition);
}

void InputMethodController::confirmCompositionAndResetState()
{
    if (!hasComposition())
        return;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    RefPtr<Range> range = compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return;
    }

    // EditorClient::willSetInputMethodState() resets input method and the composition string is committed.
    editorClient().willSetInputMethodState();
}

void InputMethodController::cancelComposition()
{
    if (!m_compositionNode)
        return;
    finishComposition(emptyString(), CancelComposition);
}

void InputMethodController::cancelCompositionIfSelectionIsInvalid()
{
    if (!hasComposition() || editor().preventRevealSelection())
        return;

    // Check if selection start and selection end are valid.
    Position start = m_frame->selection().start();
    Position end = m_frame->selection().end();
    if (start.containerNode() == m_compositionNode
        && end.containerNode() == m_compositionNode
        && static_cast<unsigned>(start.computeOffsetInContainerNode()) >= m_compositionStart
        && static_cast<unsigned>(end.computeOffsetInContainerNode()) <= m_compositionEnd)
        return;

    cancelComposition();
    editorClient().didCancelCompositionOnSelectionChange();
}

void InputMethodController::finishComposition(const String& text, FinishCompositionMode mode)
{
    ASSERT(mode == ConfirmComposition || mode == CancelComposition);
    UserTypingGestureIndicator typingGestureIndicator(m_frame);

    Editor::RevealSelectionScope revealSelectionScope(&editor());

    if (mode == CancelComposition)
        ASSERT(text == emptyString());
    else
        selectComposition();

    if (m_frame->selection().isNone())
        return;

    // Dispatch a compositionend event to the focused node.
    // We should send this event before sending a TextEvent as written in Section 6.2.2 and 6.2.3 of
    // the DOM Event specification.
    if (Element* target = m_frame->document()->focusedElement()) {
        RefPtr<CompositionEvent> event = CompositionEvent::create(eventNames().compositionendEvent, m_frame->domWindow(), text);
        target->dispatchEvent(event, IGNORE_EXCEPTION);
    }

    // If text is empty, then delete the old composition here. If text is non-empty, InsertTextCommand::input
    // will delete the old composition with an optimized replace operation.
    if (text.isEmpty() && mode != CancelComposition) {
        ASSERT(m_frame->document());
        TypingCommand::deleteSelection(*m_frame->document(), 0);
    }

    m_compositionNode = 0;
    m_customCompositionUnderlines.clear();

    insertTextForConfirmedComposition(text);

    if (mode == CancelComposition) {
        // An open typing command that disagrees about current selection would cause issues with typing later on.
        TypingCommand::closeTyping(m_frame);
    }
}

void InputMethodController::setComposition(const String& text, const Vector<CompositionUnderline>& underlines, unsigned selectionStart, unsigned selectionEnd)
{
    UserTypingGestureIndicator typingGestureIndicator(m_frame);

    Editor::RevealSelectionScope revealSelectionScope(&editor());

    // Updates styles before setting selection for composition to prevent
    // inserting the previous composition text into text nodes oddly.
    // See https://bugs.webkit.org/show_bug.cgi?id=46868
    m_frame->document()->updateStyleIfNeeded();

    selectComposition();

    if (m_frame->selection().isNone())
        return;

    if (Element* target = m_frame->document()->focusedElement()) {
        // Dispatch an appropriate composition event to the focused node.
        // We check the composition status and choose an appropriate composition event since this
        // function is used for three purposes:
        // 1. Starting a new composition.
        //    Send a compositionstart and a compositionupdate event when this function creates
        //    a new composition node, i.e.
        //    m_compositionNode == 0 && !text.isEmpty().
        //    Sending a compositionupdate event at this time ensures that at least one
        //    compositionupdate event is dispatched.
        // 2. Updating the existing composition node.
        //    Send a compositionupdate event when this function updates the existing composition
        //    node, i.e. m_compositionNode != 0 && !text.isEmpty().
        // 3. Canceling the ongoing composition.
        //    Send a compositionend event when function deletes the existing composition node, i.e.
        //    m_compositionNode != 0 && test.isEmpty().
        RefPtr<CompositionEvent> event;
        if (!m_compositionNode) {
            // We should send a compositionstart event only when the given text is not empty because this
            // function doesn't create a composition node when the text is empty.
            if (!text.isEmpty()) {
                target->dispatchEvent(CompositionEvent::create(eventNames().compositionstartEvent, m_frame->domWindow(), m_frame->selectedText()));
                event = CompositionEvent::create(eventNames().compositionupdateEvent, m_frame->domWindow(), text);
            }
        } else {
            if (!text.isEmpty())
                event = CompositionEvent::create(eventNames().compositionupdateEvent, m_frame->domWindow(), text);
            else
                event = CompositionEvent::create(eventNames().compositionendEvent, m_frame->domWindow(), text);
        }
        if (event.get())
            target->dispatchEvent(event, IGNORE_EXCEPTION);
    }

    // If text is empty, then delete the old composition here. If text is non-empty, InsertTextCommand::input
    // will delete the old composition with an optimized replace operation.
    if (text.isEmpty()) {
        ASSERT(m_frame->document());
        TypingCommand::deleteSelection(*m_frame->document(), TypingCommand::PreventSpellChecking);
    }

    m_compositionNode = 0;
    m_customCompositionUnderlines.clear();

    if (!text.isEmpty()) {
        ASSERT(m_frame->document());
        TypingCommand::insertText(*m_frame->document(), text, TypingCommand::SelectInsertedText | TypingCommand::PreventSpellChecking, TypingCommand::TextCompositionUpdate);

        // Find out what node has the composition now.
        Position base = m_frame->selection().base().downstream();
        Position extent = m_frame->selection().extent();
        Node* baseNode = base.deprecatedNode();
        unsigned baseOffset = base.deprecatedEditingOffset();
        Node* extentNode = extent.deprecatedNode();
        unsigned extentOffset = extent.deprecatedEditingOffset();

        if (baseNode && baseNode == extentNode && baseNode->isTextNode() && baseOffset + text.length() == extentOffset) {
            m_compositionNode = toText(baseNode);
            m_compositionStart = baseOffset;
            m_compositionEnd = extentOffset;
            m_customCompositionUnderlines = underlines;
            size_t numUnderlines = m_customCompositionUnderlines.size();
            for (size_t i = 0; i < numUnderlines; ++i) {
                m_customCompositionUnderlines[i].startOffset += baseOffset;
                m_customCompositionUnderlines[i].endOffset += baseOffset;
            }
            if (baseNode->renderer())
                baseNode->renderer()->repaint();

            unsigned start = std::min(baseOffset + selectionStart, extentOffset);
            unsigned end = std::min(std::max(start, baseOffset + selectionEnd), extentOffset);
            RefPtr<Range> selectedRange = Range::create(baseNode->document(), baseNode, start, baseNode, end);
            m_frame->selection().setSelectedRange(selectedRange.get(), DOWNSTREAM, false);
        }
    }
}

void InputMethodController::setCompositionFromExistingText(const Vector<CompositionUnderline>& underlines, unsigned compositionStart, unsigned compositionEnd)
{
    Node* editable = m_frame->selection().rootEditableElement();
    Position base = m_frame->selection().base().downstream();
    Node* baseNode = base.anchorNode();
    if (editable->firstChild() == baseNode && editable->lastChild() == baseNode && baseNode->isTextNode()) {
        m_compositionNode = 0;
        m_customCompositionUnderlines.clear();

        if (base.anchorType() != Position::PositionIsOffsetInAnchor)
            return;
        if (!baseNode || baseNode != m_frame->selection().extent().anchorNode())
            return;

        m_compositionNode = toText(baseNode);
        m_compositionStart = compositionStart;
        m_compositionEnd = compositionEnd;
        m_customCompositionUnderlines = underlines;
        size_t numUnderlines = m_customCompositionUnderlines.size();
        for (size_t i = 0; i < numUnderlines; ++i) {
            m_customCompositionUnderlines[i].startOffset += compositionStart;
            m_customCompositionUnderlines[i].endOffset += compositionStart;
        }
        if (baseNode->renderer())
            baseNode->renderer()->repaint();
        return;
    }

    Editor::RevealSelectionScope revealSelectionScope(&editor());
    SelectionOffsetsScope selectionOffsetsScope(this);
    setSelectionOffsets(PlainTextOffsets(compositionStart, compositionEnd));
    setComposition(m_frame->selectedText(), underlines, 0, 0);
}

PassRefPtr<Range> InputMethodController::compositionRange() const
{
    if (!m_compositionNode)
        return 0;
    unsigned length = m_compositionNode->length();
    unsigned start = std::min(m_compositionStart, length);
    unsigned end = std::min(std::max(start, m_compositionEnd), length);
    if (start >= end)
        return 0;
    return Range::create(m_compositionNode->document(), m_compositionNode.get(), start, m_compositionNode.get(), end);
}

PlainTextOffsets InputMethodController::getSelectionOffsets() const
{
    RefPtr<Range> range = m_frame->selection().selection().firstRange();
    if (!range)
        return PlainTextOffsets();
    size_t location;
    size_t length;
    // FIXME: We should change TextIterator::getLocationAndLengthFromRange() returns PlainTextOffsets.
    if (TextIterator::getLocationAndLengthFromRange(m_frame->selection().rootEditableElementOrTreeScopeRootNode(), range.get(), location, length))
        return PlainTextOffsets(location, location + length);
    return PlainTextOffsets();
}

bool InputMethodController::setSelectionOffsets(const PlainTextOffsets& selectionOffsets)
{
    if (selectionOffsets.isNull())
        return false;
    // FIXME: We should move Editor::setSelectionOffsets() into InputMethodController class.
    return editor().setSelectionOffsets(selectionOffsets.start(), selectionOffsets.end());
}

} // namespace WebCore
