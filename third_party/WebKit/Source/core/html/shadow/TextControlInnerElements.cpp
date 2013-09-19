/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "core/html/shadow/TextControlInnerElements.h"

#include "HTMLNames.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/EventNames.h"
#include "core/dom/MouseEvent.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/TextEvent.h"
#include "core/dom/TextEventInputType.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/SpeechInput.h"
#include "core/page/SpeechInputEvent.h"
#include "core/rendering/RenderTextControlSingleLine.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

using namespace HTMLNames;

TextControlInnerContainer::TextControlInnerContainer(Document& document)
    : HTMLDivElement(divTag, document)
{
}

PassRefPtr<TextControlInnerContainer> TextControlInnerContainer::create(Document& document)
{
    return adoptRef(new TextControlInnerContainer(document));
}

RenderObject* TextControlInnerContainer::createRenderer(RenderStyle*)
{
    return new RenderTextControlInnerContainer(this);
}

// ---------------------------

EditingViewPortElement::EditingViewPortElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleCallbacks();
}

PassRefPtr<EditingViewPortElement> EditingViewPortElement::create(Document& document)
{
    return adoptRef(new EditingViewPortElement(document));
}

PassRefPtr<RenderStyle> EditingViewPortElement::customStyleForRenderer()
{
    // FXIME: Move these styles to html.css.

    RefPtr<RenderStyle> style = RenderStyle::create();
    style->inheritFrom(shadowHost()->renderStyle());

    style->setFlexGrow(1);
    // min-width: 0; is needed for correct shrinking.
    // FIXME: Remove this line when https://bugs.webkit.org/show_bug.cgi?id=111790 is fixed.
    style->setMinWidth(Length(0, Fixed));
    style->setDisplay(BLOCK);
    style->setDirection(LTR);

    // We don't want the shadow dom to be editable, so we set this block to
    // read-only in case the input itself is editable.
    style->setUserModify(READ_ONLY);

    return style.release();
}

// ---------------------------

inline TextControlInnerTextElement::TextControlInnerTextElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleCallbacks();
}

PassRefPtr<TextControlInnerTextElement> TextControlInnerTextElement::create(Document& document)
{
    return adoptRef(new TextControlInnerTextElement(document));
}

void TextControlInnerTextElement::defaultEventHandler(Event* event)
{
    // FIXME: In the future, we should add a way to have default event listeners.
    // Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    // Or possibly we could just use a normal event listener.
    if (event->isBeforeTextInsertedEvent() || event->type() == eventNames().webkitEditableContentChangedEvent) {
        Element* shadowAncestor = shadowHost();
        // A TextControlInnerTextElement can have no host if its been detached,
        // but kept alive by an EditCommand. In this case, an undo/redo can
        // cause events to be sent to the TextControlInnerTextElement. To
        // prevent an infinite loop, we must check for this case before sending
        // the event up the chain.
        if (shadowAncestor)
            shadowAncestor->defaultEventHandler(event);
    }
    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

RenderObject* TextControlInnerTextElement::createRenderer(RenderStyle*)
{
    return new RenderTextControlInnerBlock(this);
}

PassRefPtr<RenderStyle> TextControlInnerTextElement::customStyleForRenderer()
{
    RenderObject* parentRenderer = shadowHost()->renderer();
    if (!parentRenderer || !parentRenderer->isTextControl())
        return originalStyleForRenderer();
    RenderTextControl* textControlRenderer = toRenderTextControl(parentRenderer);
    return textControlRenderer->createInnerTextStyle(textControlRenderer->style());
}

// ----------------------------

inline SearchFieldDecorationElement::SearchFieldDecorationElement(Document& document)
    : HTMLDivElement(divTag, document)
{
}

PassRefPtr<SearchFieldDecorationElement> SearchFieldDecorationElement::create(Document& document)
{
    RefPtr<SearchFieldDecorationElement> element = adoptRef(new SearchFieldDecorationElement(document));
    element->setAttribute(idAttr, ShadowElementNames::searchDecoration());
    return element.release();
}

const AtomicString& SearchFieldDecorationElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, resultsDecorationId, ("-webkit-search-results-decoration", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, decorationId, ("-webkit-search-decoration", AtomicString::ConstructFromLiteral));
    Element* host = shadowHost();
    if (!host)
        return resultsDecorationId;
    if (host->hasTagName(inputTag)) {
        if (toHTMLInputElement(host)->maxResults() < 0)
            return decorationId;
        return resultsDecorationId;
    }
    return resultsDecorationId;
}

void SearchFieldDecorationElement::defaultEventHandler(Event* event)
{
    // On mousedown, focus the search field
    HTMLInputElement* input = toHTMLInputElement(shadowHost());
    if (input && event->type() == eventNames().mousedownEvent && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        input->focus();
        input->select();
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool SearchFieldDecorationElement::willRespondToMouseClickEvents()
{
    return true;
}

// ----------------------------

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document& document)
    : HTMLDivElement(divTag, document)
    , m_capturing(false)
{
}

PassRefPtr<SearchFieldCancelButtonElement> SearchFieldCancelButtonElement::create(Document& document)
{
    RefPtr<SearchFieldCancelButtonElement> element = adoptRef(new SearchFieldCancelButtonElement(document));
    element->setPart(AtomicString("-webkit-search-cancel-button", AtomicString::ConstructFromLiteral));
    element->setAttribute(idAttr, ShadowElementNames::clearButton());
    return element.release();
}

void SearchFieldCancelButtonElement::detach(const AttachContext& context)
{
    if (m_capturing) {
        if (Frame* frame = document().frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
    }
    HTMLDivElement::detach(context);
}


void SearchFieldCancelButtonElement::defaultEventHandler(Event* event)
{
    // If the element is visible, on mouseup, clear the value, and set selection
    RefPtr<HTMLInputElement> input(toHTMLInputElement(shadowHost()));
    if (!input || input->isDisabledOrReadOnly()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        if (renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document().frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
        }
        input->focus();
        input->select();
        event->setDefaultHandled();
    }
    if (event->type() == eventNames().mouseupEvent && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        if (m_capturing) {
            if (Frame* frame = document().frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(0);
                m_capturing = false;
            }
            if (hovered()) {
                String oldValue = input->value();
                input->setValueForUser("");
                input->onSearch();
                event->setDefaultHandled();
            }
        }
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool SearchFieldCancelButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = toHTMLInputElement(shadowHost());
    if (input && !input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

// ----------------------------

#if ENABLE(INPUT_SPEECH)

inline InputFieldSpeechButtonElement::InputFieldSpeechButtonElement(Document& document)
    : HTMLDivElement(divTag, document)
    , m_capturing(false)
    , m_state(Idle)
    , m_listenerId(0)
{
}

InputFieldSpeechButtonElement::~InputFieldSpeechButtonElement()
{
    SpeechInput* speech = speechInput();
    if (speech && m_listenerId)  { // Could be null when page is unloading.
        if (m_state != Idle)
            speech->cancelRecognition(m_listenerId);
        speech->unregisterListener(m_listenerId);
    }
}

PassRefPtr<InputFieldSpeechButtonElement> InputFieldSpeechButtonElement::create(Document& document)
{
    RefPtr<InputFieldSpeechButtonElement> element = adoptRef(new InputFieldSpeechButtonElement(document));
    element->setPart(AtomicString("-webkit-input-speech-button", AtomicString::ConstructFromLiteral));
    element->setAttribute(idAttr, ShadowElementNames::speechButton());
    return element.release();
}

void InputFieldSpeechButtonElement::defaultEventHandler(Event* event)
{
    // For privacy reasons, only allow clicks directly coming from the user.
    if (!ScriptController::processingUserGesture()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // The call to focus() below dispatches a focus event, and an event handler in the page might
    // remove the input element from DOM. To make sure it remains valid until we finish our work
    // here, we take a temporary reference.
    RefPtr<HTMLInputElement> input(toHTMLInputElement(shadowHost()));

    if (!input || input->isDisabledOrReadOnly()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // On mouse down, select the text and set focus.
    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        if (renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document().frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
        }
        RefPtr<InputFieldSpeechButtonElement> holdRefButton(this);
        input->focus();
        input->select();
        event->setDefaultHandled();
    }
    // On mouse up, release capture cleanly.
    if (event->type() == eventNames().mouseupEvent && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        if (m_capturing && renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document().frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(0);
                m_capturing = false;
            }
        }
    }

    if (event->type() == eventNames().clickEvent && m_listenerId) {
        switch (m_state) {
        case Idle:
            startSpeechInput();
            break;
        case Recording:
            stopSpeechInput();
            break;
        case Recognizing:
            // Nothing to do here, we will continue to wait for results.
            break;
        }
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool InputFieldSpeechButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = toHTMLInputElement(shadowHost());
    if (input && !input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

void InputFieldSpeechButtonElement::setState(SpeechInputState state)
{
    if (m_state != state) {
        m_state = state;
        shadowHost()->renderer()->repaint();
    }
}

SpeechInput* InputFieldSpeechButtonElement::speechInput()
{
    return SpeechInput::from(document().page());
}

void InputFieldSpeechButtonElement::didCompleteRecording(int)
{
    setState(Recognizing);
}

void InputFieldSpeechButtonElement::didCompleteRecognition(int)
{
    setState(Idle);
}

void InputFieldSpeechButtonElement::setRecognitionResult(int, const SpeechInputResultArray& results)
{
    m_results = results;

    // The call to setValue() below dispatches an event, and an event handler in the page might
    // remove the input element from DOM. To make sure it remains valid until we finish our work
    // here, we take a temporary reference.
    RefPtr<HTMLInputElement> input(toHTMLInputElement(shadowHost()));
    if (!input || input->isDisabledOrReadOnly())
        return;

    RefPtr<InputFieldSpeechButtonElement> holdRefButton(this);
    if (document().domWindow()) {
        // Call selectionChanged, causing the element to cache the selection,
        // so that the text event inserts the text in this element even if
        // focus has moved away from it.
        input->selectionChanged(false);
        input->dispatchEvent(TextEvent::create(document().domWindow(), results.isEmpty() ? "" : results[0]->utterance(), TextEventInputOther));
    }

    // This event is sent after the text event so the website can perform actions using the input field content immediately.
    // It provides alternative recognition hypotheses and notifies that the results come from speech input.
    input->dispatchEvent(SpeechInputEvent::create(eventNames().webkitspeechchangeEvent, results));

    // Check before accessing the renderer as the above event could have potentially turned off
    // speech in the input element, hence removing this button and renderer from the hierarchy.
    if (renderer())
        renderer()->repaint();
}

void InputFieldSpeechButtonElement::attach(const AttachContext& context)
{
    ASSERT(!m_listenerId);
    if (SpeechInput* input = SpeechInput::from(document().page()))
        m_listenerId = input->registerListener(this);
    HTMLDivElement::attach(context);
}

void InputFieldSpeechButtonElement::detach(const AttachContext& context)
{
    if (m_capturing) {
        if (Frame* frame = document().frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
    }

    if (m_listenerId) {
        if (m_state != Idle)
            speechInput()->cancelRecognition(m_listenerId);
        speechInput()->unregisterListener(m_listenerId);
        m_listenerId = 0;
    }

    HTMLDivElement::detach(context);
}

void InputFieldSpeechButtonElement::startSpeechInput()
{
    if (m_state != Idle)
        return;

    RefPtr<HTMLInputElement> input = toHTMLInputElement(shadowHost());
    AtomicString language = input->computeInheritedLanguage();
    String grammar = input->getAttribute(webkitgrammarAttr);
    IntRect rect = document().view()->contentsToRootView(pixelSnappedBoundingBox());
    if (speechInput()->startRecognition(m_listenerId, rect, language, grammar, document().securityOrigin()))
        setState(Recording);
}

void InputFieldSpeechButtonElement::stopSpeechInput()
{
    if (m_state == Recording)
        speechInput()->stopRecording(m_listenerId);
}
#endif // ENABLE(INPUT_SPEECH)

}
