/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "core/frame/DOMWindowLifecycleNotifier.h"

namespace WebCore {

DOMWindowLifecycleNotifier::DOMWindowLifecycleNotifier(LifecycleContext* context)
    : LifecycleNotifier(context)
{
}

void DOMWindowLifecycleNotifier::addObserver(LifecycleObserver* observer)
{
    if (observer->observerType() == LifecycleObserver::DOMWindowLifecycleObserverType) {
        RELEASE_ASSERT(m_iterating != IteratingOverDOMWindowObservers);
        m_windowObservers.add(static_cast<DOMWindowLifecycleObserver*>(observer));
    }

    LifecycleNotifier::addObserver(observer);
}

void DOMWindowLifecycleNotifier::removeObserver(LifecycleObserver* observer)
{
    if (observer->observerType() == LifecycleObserver::DOMWindowLifecycleObserverType) {
        RELEASE_ASSERT(m_iterating != IteratingOverDOMWindowObservers);
        m_windowObservers.remove(static_cast<DOMWindowLifecycleObserver*>(observer));
    }

    LifecycleNotifier::removeObserver(observer);
}

PassOwnPtr<DOMWindowLifecycleNotifier> DOMWindowLifecycleNotifier::create(LifecycleContext* context)
{
    return adoptPtr(new DOMWindowLifecycleNotifier(context));
}

void DOMWindowLifecycleNotifier::notifyAddEventListener(DOMWindow* window, const AtomicString& eventType)
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverDOMWindowObservers);
    for (DOMWindowObserverSet::iterator it = m_windowObservers.begin(); it != m_windowObservers.end(); ++it)
        (*it)->didAddEventListener(window, eventType);
}

void DOMWindowLifecycleNotifier::notifyRemoveEventListener(DOMWindow* window, const AtomicString& eventType)
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverDOMWindowObservers);
    for (DOMWindowObserverSet::iterator it = m_windowObservers.begin(); it != m_windowObservers.end(); ++it)
        (*it)->didRemoveEventListener(window, eventType);
}

void DOMWindowLifecycleNotifier::notifyRemoveAllEventListeners(DOMWindow* window)
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverDOMWindowObservers);
    for (DOMWindowObserverSet::iterator it = m_windowObservers.begin(); it != m_windowObservers.end(); ++it)
        (*it)->didRemoveAllEventListeners(window);
}

} // namespace WebCore
