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

#include "wtf/MainThread.h"

namespace WebCore {

size_t CustomElementCallbackDispatcher::s_elementQueueStart = 0;

// The base of the stack has a null sentinel value.
size_t CustomElementCallbackDispatcher::s_elementQueueEnd = 1;

CustomElementCallbackDispatcher& CustomElementCallbackDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(CustomElementCallbackDispatcher, instance, ());
    return instance;
}

void CustomElementCallbackDispatcher::enqueueCreatedCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
{
    Element* key = element.get();
    ElementCallbackQueueMap::AddResult result = m_elementCallbackQueueMap.add(key, CustomElementCallbackQueue::create(callbacks, element));
    ASSERT(result.isNewEntry); // creation callbacks are always scheduled first

    CustomElementCallbackQueue* queue = result.iterator->value.get();
    queue->setOwner(currentElementQueue());
    queue->append(CustomElementLifecycleCallbacks::Created);

    // The created callback is unique in being prepended to the front
    // of the element queue
    m_flattenedProcessingStack.insert(inCallbackDeliveryScope() ? s_elementQueueStart : /* skip null sentinel */ 1, queue);
    s_elementQueueEnd++;
}

// Dispatches callbacks at microtask checkpoint.
bool CustomElementCallbackDispatcher::dispatch()
{
    ASSERT(isMainThread());
    ASSERT(!inCallbackDeliveryScope());
    size_t start = 1; // skip null sentinel
    size_t end = s_elementQueueEnd;

    for (size_t i = start; i < end; i++) {
        m_flattenedProcessingStack[i]->processInElementQueue(currentElementQueue());

        // new callbacks as a result of recursion must be scheduled in
        // a CallbackDeliveryScope which restore this queue on completion
        ASSERT(!s_elementQueueStart);
        ASSERT(s_elementQueueEnd == end);
    }

    s_elementQueueEnd = 1;
    m_flattenedProcessingStack.resize(s_elementQueueEnd);
    m_elementCallbackQueueMap.clear();

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

    for (size_t i = start; i < end; i++) {
        m_flattenedProcessingStack[i]->processInElementQueue(currentElementQueue());

        // process() may run script which grows and shrinks the
        // processing stack above this entry, but the processing stack
        // should always drop back to having this entry at the
        // top-of-stack on exit
        ASSERT(start == s_elementQueueStart);
        ASSERT(end == s_elementQueueEnd);
    }

    // Pop the element queue from the processing stack
    m_flattenedProcessingStack.resize(start);
    s_elementQueueEnd = start;

    if (start == /* allow sentinel */ 1)
        m_elementCallbackQueueMap.clear();
}

} // namespace WebCore
