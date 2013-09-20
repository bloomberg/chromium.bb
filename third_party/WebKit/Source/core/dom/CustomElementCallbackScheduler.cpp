/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/CustomElementCallbackScheduler.h"

#include "core/dom/CustomElementCallbackDispatcher.h"
#include "core/dom/CustomElementCallbackQueue.h"
#include "core/dom/CustomElementLifecycleCallbacks.h"
#include "core/dom/Element.h"

namespace WebCore {

void CustomElementCallbackScheduler::scheduleAttributeChangedCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!callbacks->hasAttributeChangedCallback())
        return;

    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementCallbackInvocation::createAttributeChangedInvocation(callbacks, name, oldValue, newValue));
}

void CustomElementCallbackScheduler::scheduleCreatedCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
{
    if (!callbacks->hasCreatedCallback())
        return;

    CustomElementCallbackQueue* queue = instance().scheduleInCurrentElementQueue(element);
    queue->append(CustomElementCallbackInvocation::createInvocation(callbacks, CustomElementLifecycleCallbacks::Created));
}

void CustomElementCallbackScheduler::scheduleEnteredViewCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
{
    if (!callbacks->hasEnteredViewCallback())
        return;

    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementCallbackInvocation::createInvocation(callbacks, CustomElementLifecycleCallbacks::EnteredView));
}

void CustomElementCallbackScheduler::scheduleLeftViewCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
{
    if (!callbacks->hasLeftViewCallback())
        return;

    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementCallbackInvocation::createInvocation(callbacks, CustomElementLifecycleCallbacks::LeftView));
}

CustomElementCallbackScheduler& CustomElementCallbackScheduler::instance()
{
    DEFINE_STATIC_LOCAL(CustomElementCallbackScheduler, instance, ());
    return instance;
}

CustomElementCallbackQueue* CustomElementCallbackScheduler::ensureCallbackQueue(PassRefPtr<Element> element)
{
    Element* key = element.get();
    ElementCallbackQueueMap::iterator it = m_elementCallbackQueueMap.find(key);
    if (it == m_elementCallbackQueueMap.end())
        it = m_elementCallbackQueueMap.add(key, CustomElementCallbackQueue::create(element)).iterator;
    return it->value.get();
}

void CustomElementCallbackScheduler::clearElementCallbackQueueMap()
{
    ElementCallbackQueueMap emptyMap;
    instance().m_elementCallbackQueueMap.swap(emptyMap);
}

// Finds or creates the callback queue for element. If the
// createdCallback has not finished running, the callback queue is not
// moved to the top-of-stack. Otherwise like
// scheduleInCurrentElementQueue.
CustomElementCallbackQueue* CustomElementCallbackScheduler::schedule(PassRefPtr<Element> element)
{
    CustomElementCallbackQueue* callbackQueue = ensureCallbackQueue(element);
    if (!callbackQueue->inCreatedCallback())
        CustomElementCallbackDispatcher::instance().enqueue(callbackQueue);
    return callbackQueue;
}

// Finds or creates the callback queue for element. If the element's
// callback queue is scheduled in an earlier processing stack frame,
// its owner is set to the element queue on the top of the processing
// stack. Because callback queues are processed exhaustively, this
// effectively moves the callback queue to the top of the stack.
CustomElementCallbackQueue* CustomElementCallbackScheduler::scheduleInCurrentElementQueue(PassRefPtr<Element> element)
{
    CustomElementCallbackQueue* callbackQueue = ensureCallbackQueue(element);
    CustomElementCallbackDispatcher::instance().enqueue(callbackQueue);
    return callbackQueue;
}

} // namespace WebCore
