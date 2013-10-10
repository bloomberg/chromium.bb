/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/dom/ScriptedAnimationController.h"

#include "core/dom/Document.h"
#include "core/dom/RequestAnimationFrameCallback.h"
#include "core/events/Event.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/FrameView.h"

namespace WebCore {

std::pair<EventTarget*, StringImpl*> scheduledEventTargetKey(const Event* event)
{
    return std::make_pair(event->target(), event->type().impl());
}

ScriptedAnimationController::ScriptedAnimationController(Document* document)
    : m_document(document)
    , m_nextCallbackId(0)
    , m_suspendCount(0)
{
}

ScriptedAnimationController::~ScriptedAnimationController()
{
}

void ScriptedAnimationController::suspend()
{
    ++m_suspendCount;
}

void ScriptedAnimationController::resume()
{
    // It would be nice to put an ASSERT(m_suspendCount > 0) here, but in WK1 resume() can be called
    // even when suspend hasn't (if a tab was created in the background).
    if (m_suspendCount > 0)
        --m_suspendCount;
    scheduleAnimationIfNeeded();
}

ScriptedAnimationController::CallbackId ScriptedAnimationController::registerCallback(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    ScriptedAnimationController::CallbackId id = ++m_nextCallbackId;
    callback->m_firedOrCancelled = false;
    callback->m_id = id;
    m_callbacks.append(callback);
    scheduleAnimationIfNeeded();

    InspectorInstrumentation::didRequestAnimationFrame(m_document, id);

    return id;
}

void ScriptedAnimationController::cancelCallback(CallbackId id)
{
    for (size_t i = 0; i < m_callbacks.size(); ++i) {
        if (m_callbacks[i]->m_id == id) {
            m_callbacks[i]->m_firedOrCancelled = true;
            InspectorInstrumentation::didCancelAnimationFrame(m_document, id);
            m_callbacks.remove(i);
            return;
        }
    }
}

void ScriptedAnimationController::serviceScriptedAnimations(double monotonicTimeNow)
{
    if ((!m_callbacks.size() && !m_eventQueue.size()) || m_suspendCount)
        return;

    // Invoking callbacks may detach elements from our document, which clears the document's
    // reference to us, so take a defensive reference.
    RefPtr<ScriptedAnimationController> protector(this);

    double highResNowMs = 1000.0 * m_document->loader()->timing()->monotonicTimeToZeroBasedDocumentTime(monotonicTimeNow);
    double legacyHighResNowMs = 1000.0 * m_document->loader()->timing()->monotonicTimeToPseudoWallTime(monotonicTimeNow);

    Vector<RefPtr<Event> > events;
    events.swap(m_eventQueue);
    m_scheduledEventTargets.clear();

    for (size_t i = 0; i < events.size(); ++i)
        events[i]->target()->dispatchEvent(events[i]);

    // First, generate a list of callbacks to consider.  Callbacks registered from this point
    // on are considered only for the "next" frame, not this one.
    CallbackList callbacks(m_callbacks);

    for (size_t i = 0; i < callbacks.size(); ++i) {
        RequestAnimationFrameCallback* callback = callbacks[i].get();
        if (!callback->m_firedOrCancelled) {
            callback->m_firedOrCancelled = true;
            InspectorInstrumentationCookie cookie = InspectorInstrumentation::willFireAnimationFrame(m_document, callback->m_id);
            if (callback->m_useLegacyTimeBase)
                callback->handleEvent(legacyHighResNowMs);
            else
                callback->handleEvent(highResNowMs);
            InspectorInstrumentation::didFireAnimationFrame(cookie);
        }
    }

    // Remove any callbacks we fired from the list of pending callbacks.
    for (size_t i = 0; i < m_callbacks.size();) {
        if (m_callbacks[i]->m_firedOrCancelled)
            m_callbacks.remove(i);
        else
            ++i;
    }

    scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::scheduleEvent(PassRefPtr<Event> event)
{
    if (!m_scheduledEventTargets.add(scheduledEventTargetKey(event.get())).isNewEntry)
        return;
    m_eventQueue.append(event);
    scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::scheduleAnimationIfNeeded()
{
    if (!m_document)
        return;

    if (m_suspendCount)
        return;

    if (!m_callbacks.size() && !m_eventQueue.size())
        return;

    if (FrameView* frameView = m_document->view())
        frameView->scheduleAnimation();
}

}
