/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/dom/ContextLifecycleNotifier.h"

#include "core/dom/ScriptExecutionContext.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

ContextLifecycleNotifier::ContextLifecycleNotifier(ScriptExecutionContext* context)
    : LifecycleNotifier(context)
{
}

ContextLifecycleNotifier::~ContextLifecycleNotifier()
{
}

void ContextLifecycleNotifier::addObserver(LifecycleObserver* observer)
{
    LifecycleNotifier::addObserver(observer);

    RELEASE_ASSERT(m_iterating != IteratingOverContextObservers);
    if (observer->observerType() == LifecycleObserver::ActiveDOMObjectType) {
        RELEASE_ASSERT(m_iterating != IteratingOverActiveDOMObjects);
        m_activeDOMObjects.add(static_cast<ActiveDOMObject*>(observer));
    }
}

void ContextLifecycleNotifier::removeObserver(LifecycleObserver* observer)
{
    LifecycleNotifier::removeObserver(observer);

    RELEASE_ASSERT(m_iterating != IteratingOverContextObservers);
    if (observer->observerType() == LifecycleObserver::ActiveDOMObjectType) {
        RELEASE_ASSERT(m_iterating != IteratingOverActiveDOMObjects);
        m_activeDOMObjects.remove(static_cast<ActiveDOMObject*>(observer));
    }
}

void ContextLifecycleNotifier::notifyResumingActiveDOMObjects()
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverActiveDOMObjects);
    ActiveDOMObjectSet::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (ActiveDOMObjectSet::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == context());
        ASSERT((*iter)->suspendIfNeededCalled());
        (*iter)->resume();
    }
}

void ContextLifecycleNotifier::notifySuspendingActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverActiveDOMObjects);
    ActiveDOMObjectSet::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (ActiveDOMObjectSet::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == context());
        ASSERT((*iter)->suspendIfNeededCalled());
        (*iter)->suspend(why);
    }
}

void ContextLifecycleNotifier::notifyStoppingActiveDOMObjects()
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverActiveDOMObjects);
    ActiveDOMObjectSet::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (ActiveDOMObjectSet::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == context());
        ASSERT((*iter)->suspendIfNeededCalled());
        (*iter)->stop();
    }
}

bool ContextLifecycleNotifier::canSuspendActiveDOMObjects()
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverActiveDOMObjects);
    ActiveDOMObjectSet::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (ActiveDOMObjectSet::const_iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == context());
        ASSERT((*iter)->suspendIfNeededCalled());
        if (!(*iter)->canSuspend())
            return false;
    }

    return true;
}

bool ContextLifecycleNotifier::hasPendingActivity() const
{
    ActiveDOMObjectSet::const_iterator activeObjectsEnd = activeDOMObjects().end();
    for (ActiveDOMObjectSet::const_iterator iter = activeDOMObjects().begin(); iter != activeObjectsEnd; ++iter) {
        if ((*iter)->hasPendingActivity())
            return true;
    }

    return false;
}

} // namespace WebCore

