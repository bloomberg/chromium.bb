/*
 * Copyright (C) 2013 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/TouchController.h"

#include "core/dom/Document.h"
#include "core/events/EventNames.h"
#include "core/events/TouchEvent.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"

namespace WebCore {

TouchController::TouchController(Document* document)
    : DOMWindowLifecycleObserver(document->domWindow())
    , DocumentLifecycleObserver(document)
{
}

TouchController::~TouchController()
{
}

const char* TouchController::supplementName()
{
    return "TouchController";
}

TouchController* TouchController::from(Document* document)
{
    TouchController* controller = static_cast<TouchController*>(Supplement<ScriptExecutionContext>::from(document, supplementName()));
    if (!controller) {
        controller = new TouchController(document);
        Supplement<ScriptExecutionContext>::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return controller;
}

void TouchController::didAddTouchEventHandler(Document* document, Node* handler)
{
    if (!m_touchEventTargets)
        m_touchEventTargets = adoptPtr(new TouchEventTargetSet);
    m_touchEventTargets->add(handler);
    if (Document* parent = document->parentDocument()) {
        TouchController::from(parent)->didAddTouchEventHandler(parent, document);
        return;
    }
    if (Page* page = document->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->touchEventTargetRectsDidChange(document);
        if (m_touchEventTargets->size() == 1)
            page->chrome().client().needTouchEvents(true);
    }
}

void TouchController::didRemoveTouchEventHandler(Document* document, Node* handler)
{
    if (!m_touchEventTargets)
        return;
    ASSERT(m_touchEventTargets->contains(handler));
    m_touchEventTargets->remove(handler);
    if (Document* parent = document->parentDocument()) {
        TouchController::from(parent)->didRemoveTouchEventHandler(parent, document);
        return;
    }

    Page* page = document->page();
    if (!page)
        return;
    if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
        scrollingCoordinator->touchEventTargetRectsDidChange(document);
    if (m_touchEventTargets->size())
        return;
    for (const Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document() && TouchController::from(frame->document())->hasTouchEventHandlers())
            return;
    }
    page->chrome().client().needTouchEvents(false);
}

void TouchController::didRemoveEventTargetNode(Document* document, Node* handler)
{
    if (m_touchEventTargets && !m_touchEventTargets->isEmpty()) {
        if (handler == document)
            m_touchEventTargets->clear();
        else
            m_touchEventTargets->removeAll(handler);
        Document* parent = document->parentDocument();
        if (m_touchEventTargets->isEmpty() && parent)
            TouchController::from(parent)->didRemoveEventTargetNode(parent, document);
    }
}

void TouchController::didAddEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventNames().isTouchEventType(eventType)) {
        Document* document = window->document();
        didAddTouchEventHandler(document, document);
    }
}

void TouchController::didRemoveEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventNames().isTouchEventType(eventType)) {
        Document* document = window->document();
        didRemoveTouchEventHandler(document, document);
    }
}

void TouchController::didRemoveAllEventListeners(DOMWindow* window)
{
    if (Document* document = window->document())
        didRemoveEventTargetNode(document, document);
}

void TouchController::documentWasDetached()
{
    Document* document = static_cast<Document*>(scriptExecutionContext());
    Document* parentDocument = document->parentDocument();

    if (parentDocument) {
        TouchController* parentController = TouchController::from(parentDocument);
        if (parentController->hasTouchEventHandlers())
            parentController->didRemoveEventTargetNode(parentDocument, document);
    }
}

void TouchController::documentBeingDestroyed()
{
    Document* document = static_cast<Document*>(scriptExecutionContext());

    if (Document* ownerDocument = document->ownerDocument())
        TouchController::from(ownerDocument)->didRemoveEventTargetNode(ownerDocument, document);
}

} // namespace WebCore
