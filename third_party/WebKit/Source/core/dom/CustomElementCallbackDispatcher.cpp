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
#include "CustomElementCallbackDispatcher.h"

#include "CustomElementLifecycleCallbacks.h"

namespace WebCore {

CustomElementCallbackDispatcher& CustomElementCallbackDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(CustomElementCallbackDispatcher, instance, ());
    return instance;
}

CustomElementCallbackDispatcher::ReadyInvocation::ReadyInvocation(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
    : m_callbacks(callbacks)
    , m_element(element)
{
}

bool CustomElementCallbackDispatcher::dispatch()
{
    if (m_invocations.isEmpty())
        return false;

    do  {
        Vector<ReadyInvocation> invocations;
        m_invocations.swap(invocations);

        for (Vector<ReadyInvocation>::iterator it = invocations.begin(); it != invocations.end(); ++it)
            it->invoke();
    } while (!m_invocations.isEmpty());

    return true;
}

void CustomElementCallbackDispatcher::enqueueReadyCallback(CustomElementLifecycleCallbacks* callbacks, Element* element)
{
    if (!callbacks->hasReady())
        return;

    m_invocations.append(ReadyInvocation(callbacks, element));
}

} // namespace WebCore
