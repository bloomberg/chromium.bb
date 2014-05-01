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
#include "core/dom/custom/CustomElementMicrotaskQueue.h"

#include "core/dom/custom/CustomElementCallbackDispatcher.h"

namespace WebCore {

class MicrotaskQueueInvocationScope {
public:
#if defined(NDEBUG)
    explicit MicrotaskQueueInvocationScope(CustomElementMicrotaskQueue*) { }
#else
    explicit MicrotaskQueueInvocationScope(CustomElementMicrotaskQueue* queue)
        : m_parent(s_top)
        , m_queue(queue)
    {
        s_top = this;
        ASSERT(m_queue->isEmpty() || !hasReenter());
    }

    ~MicrotaskQueueInvocationScope()
    {
        s_top = m_parent;
    }

private:
    bool hasReenter() const
    {
        for (MicrotaskQueueInvocationScope* scope = this->m_parent; scope; scope = scope->m_parent) {
            if (scope->m_queue == m_queue)
                return true;
        }

        return false;
    }

    MicrotaskQueueInvocationScope* m_parent;
    CustomElementMicrotaskQueue* m_queue;

    static MicrotaskQueueInvocationScope* s_top;
#endif
};

#if !defined(NDEBUG)
MicrotaskQueueInvocationScope* MicrotaskQueueInvocationScope::s_top = 0;
#endif

void CustomElementMicrotaskQueue::enqueue(PassOwnPtr<CustomElementMicrotaskStep> step)
{
    m_queue.append(step);
}

CustomElementMicrotaskStep::Result CustomElementMicrotaskQueue::dispatch()
{
    MicrotaskQueueInvocationScope scope(this);
    Result result = Result(0);

    unsigned i;
    for (i = 0; i < m_queue.size(); ++i) {
        result = Result(result | m_queue[i]->process());

        if (result & CustomElementMicrotaskStep::ShouldStop)
            break;
    }

    bool wasStopped = i < m_queue.size();
    if (wasStopped)
        m_queue.remove(0, i);
    else
        m_queue.resize(0);

    return result;
}

#if !defined(NDEBUG)
void CustomElementMicrotaskQueue::show(unsigned indent)
{
    for (unsigned q = 0; q < m_queue.size(); ++q) {
        if (m_queue[q])
            m_queue[q]->show(indent);
        else
            fprintf(stderr, "%*s\n", indent, "");
    }
}
#endif

} // namespace WebCore
