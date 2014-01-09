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
#include "core/dom/custom/CustomElementBaseElementQueue.h"

#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/dom/custom/CustomElementCallbackQueue.h"

namespace WebCore {

void CustomElementBaseElementQueue::enqueue(CustomElementBaseElementQueue::Item* item)
{
    m_queue.append(item);
}

void CustomElementBaseElementQueue::remove(Item* item)
{
    size_t found = m_queue.find(item);
    if (found != kNotFound)
        m_queue.remove(found);
}

void CustomElementBaseElementQueue::removeAndDeleteLater(PassOwnPtr<Item> item)
{
    size_t found = m_queue.find(item.get());
    if (found != kNotFound)
        m_queue.remove(found);
    m_dyingItems.append(item);
}

bool CustomElementBaseElementQueue::dispatch(ElementQueue baseQueueId)
{
    ASSERT(!m_inDispatch);
    m_inDispatch = true;

    unsigned i;
    for (i = 0; i < m_queue.size(); ++i) {
        // The created callback may schedule entered document
        // callbacks.
        CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
        // The task can be blocked by pending imports.
        if (!m_queue[i]->process(baseQueueId))
            break;
    }

    bool didWork = 0 < m_queue.size() && 0 < i;
    if (i < m_queue.size())
        m_queue.remove(0, i);
    else
        m_queue.resize(0);

    m_dyingItems.clear();
    m_inDispatch = 0;

    return didWork;
}

} // namespace WebCore
