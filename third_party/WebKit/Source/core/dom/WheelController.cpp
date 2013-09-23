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
#include "core/dom/WheelController.h"

#include "core/dom/Document.h"
#include "core/events/EventNames.h"
#include "core/events/WheelEvent.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"

namespace WebCore {

WheelController::WheelController(Document* document)
    : DOMWindowLifecycleObserver(document->domWindow())
    , m_wheelEventHandlerCount(0)
{
}

WheelController::~WheelController()
{
}

const char* WheelController::supplementName()
{
    return "WheelController";
}

WheelController* WheelController::from(Document* document)
{
    WheelController* controller = static_cast<WheelController*>(Supplement<ScriptExecutionContext>::from(document, supplementName()));
    if (!controller) {
        controller = new WheelController(document);
        Supplement<ScriptExecutionContext>::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return controller;
}

static void wheelEventHandlerCountChanged(Document* document)
{
    Page* page = document->page();
    if (!page)
        return;

    ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return;

    FrameView* frameView = document->view();
    if (!frameView)
        return;

    scrollingCoordinator->frameViewWheelEventHandlerCountChanged(frameView);
}

void WheelController::didAddWheelEventHandler(Document* document)
{
    ++m_wheelEventHandlerCount;
    Page* page = document->page();
    Frame* mainFrame = page ? page->mainFrame() : 0;
    if (mainFrame)
        mainFrame->notifyChromeClientWheelEventHandlerCountChanged();

    wheelEventHandlerCountChanged(document);
}

void WheelController::didRemoveWheelEventHandler(Document* document)
{
    ASSERT(m_wheelEventHandlerCount > 0);
    --m_wheelEventHandlerCount;
    Page* page = document->page();
    Frame* mainFrame = page ? page->mainFrame() : 0;
    if (mainFrame)
        mainFrame->notifyChromeClientWheelEventHandlerCountChanged();

    wheelEventHandlerCountChanged(document);
}

void WheelController::didAddEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType != eventNames().wheelEvent && eventType != eventNames().mousewheelEvent)
        return;

    Document* document = window->document();
    didAddWheelEventHandler(document);
}

void WheelController::didRemoveEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType != eventNames().wheelEvent && eventType != eventNames().mousewheelEvent)
        return;

    Document* document = window->document();
    didRemoveWheelEventHandler(document);
}

} // namespace WebCore
