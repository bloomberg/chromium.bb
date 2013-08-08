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
#include "core/dom/CustomElementCallbackDispatcher.h"

#include "core/dom/CustomElementCallbackInvocation.h"
#include "wtf/MainThread.h"

namespace WebCore {

size_t CustomElementCallbackDispatcher::s_elementQueueStart = 0;

// The base of the stack has a null sentinel value.
size_t CustomElementCallbackDispatcher::s_elementQueueEnd = kNumSentinels;

CustomElementCallbackDispatcher& CustomElementCallbackDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(CustomElementCallbackDispatcher, instance, ());
    return instance;
}

// Dispatches callbacks at microtask checkpoint.
bool CustomElementCallbackDispatcher::dispatch()
{
    ASSERT(isMainThread());
    if (inCallbackDeliveryScope())
        return false;

    size_t start = kNumSentinels; // skip null sentinel
    size_t end = s_elementQueueEnd;
    ElementQueue thisQueue = currentElementQueue();

    for (size_t i = start; i < end; i++) {
        {
            // The created callback may schedule entered document
            // callbacks.
            CallbackDeliveryScope deliveryScope;
            m_flattenedProcessingStack[i]->processInElementQueue(thisQueue);
        }

        ASSERT(!s_elementQueueStart);
        ASSERT(s_elementQueueEnd == end);
    }

    s_elementQueueEnd = kNumSentinels;
    m_flattenedProcessingStack.resize(s_elementQueueEnd);

    ElementCallbackQueueMap emptyMap;
    m_elementCallbackQueueMap.swap(emptyMap);

    bool didWork = start < end;
    return didWork;
}

// Dispatches callbacks when popping the processing stack.
void CustomElementCallbackDispatcher::processElementQueueAndPop()
{
    instance().processElementQueueAndPop(s_elementQueueStart, s_elementQueueEnd);
}

void CustomElementCallbackDispatcher::processElementQueueAndPop(size_t start, size_t end)
{
    ASSERT(isMainThread());
    ElementQueue thisQueue = currentElementQueue();

    for (size_t i = start; i < end; i++) {
        {
            // The created callback may schedule entered document
            // callbacks.
            CallbackDeliveryScope deliveryScope;
            m_flattenedProcessingStack[i]->processInElementQueue(thisQueue);
        }

        ASSERT(start == s_elementQueueStart);
        ASSERT(end == s_elementQueueEnd);
    }

    // Pop the element queue from the processing stack
    m_flattenedProcessingStack.resize(start);
    s_elementQueueEnd = start;

    if (start == kNumSentinels) {
        ElementCallbackQueueMap emptyMap;
        m_elementCallbackQueueMap.swap(emptyMap);
    }
}

CustomElementCallbackQueue* CustomElementCallbackDispatcher::createCallbackQueue(PassRefPtr<Element> element)
{
    Element* key = element.get();
    ElementCallbackQueueMap::AddResult result = m_elementCallbackQueueMap.add(key, CustomElementCallbackQueue::create(element));
    ASSERT(result.isNewEntry);
    return result.iterator->value.get();
}

CustomElementCallbackQueue* CustomElementCallbackDispatcher::ensureCallbackQueue(PassRefPtr<Element> element)
{
    Element* key = element.get();
    ElementCallbackQueueMap::iterator it = m_elementCallbackQueueMap.find(key);
    if (it == m_elementCallbackQueueMap.end())
        it = m_elementCallbackQueueMap.add(key, CustomElementCallbackQueue::create(element)).iterator;
    return it->value.get();
}

// Finds or creates the callback queue for element. If the element's
// callback queue is scheduled in an earlier processing stack frame,
// its owner is set to the element queue on the top of the processing
// stack. Because callback queues are processed exhaustively, this
// effectively moves the callback queue to the top of the stack.
CustomElementCallbackQueue* CustomElementCallbackDispatcher::ensureInCurrentElementQueue(PassRefPtr<Element> element)
{
    CustomElementCallbackQueue* queue = ensureCallbackQueue(element);
    bool isInCurrentQueue = queue->owner() == currentElementQueue();
    if (!isInCurrentQueue) {
        queue->setOwner(currentElementQueue());
        m_flattenedProcessingStack.append(queue);
        ++s_elementQueueEnd;
    }
    return queue;
}

CustomElementCallbackQueue* CustomElementCallbackDispatcher::createAtFrontOfCurrentElementQueue(PassRefPtr<Element> element)
{
    CustomElementCallbackQueue* queue = createCallbackQueue(element);
    queue->setOwner(currentElementQueue());

    // The created callback is unique in being prepended to the front
    // of the element queue
    m_flattenedProcessingStack.insert(inCallbackDeliveryScope() ? s_elementQueueStart : kNumSentinels, queue);
    ++s_elementQueueEnd;

    return queue;
}

} // namespace WebCore
