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

#include "core/dom/ActiveDOMObject.h"
#include "wtf/TemporaryChange.h"

namespace blink {

ContextLifecycleNotifier::ContextLifecycleNotifier(ExecutionContext* context)
    : LifecycleNotifier<ExecutionContext, ContextLifecycleObserver>(context)
{
}

void ContextLifecycleNotifier::addObserver(ContextLifecycleObserver* observer)
{
    LifecycleNotifier<ExecutionContext, ContextLifecycleObserver>::addObserver(observer);
    if (observer->observerType() == ContextLifecycleObserver::ActiveDOMObjectType) {
        RELEASE_ASSERT(m_iterating != IteratingOverActiveDOMObjects);
        m_activeDOMObjects.add(static_cast<ActiveDOMObject*>(observer));
    }
}

void ContextLifecycleNotifier::removeObserver(ContextLifecycleObserver* observer)
{
    LifecycleNotifier<ExecutionContext, ContextLifecycleObserver>::removeObserver(observer);
    if (observer->observerType() == ContextLifecycleObserver::ActiveDOMObjectType)
        m_activeDOMObjects.remove(static_cast<ActiveDOMObject*>(observer));
}

void ContextLifecycleNotifier::notifyResumingActiveDOMObjects()
{
    TemporaryChange<IterationType> scope(m_iterating, IteratingOverActiveDOMObjects);
    Vector<ActiveDOMObject*> snapshotOfActiveDOMObjects;
    copyToVector(m_activeDOMObjects, snapshotOfActiveDOMObjects);
    for (ActiveDOMObject* obj : snapshotOfActiveDOMObjects) {
        // FIXME: Oilpan: At the moment, it's possible that the ActiveDOMObject
        // is destructed during the iteration. Once we enable Oilpan by default
        // for ActiveDOMObjects, we can remove the hack by making
        // m_activeDOMObjects a HeapHashSet<WeakMember<ActiveDOMObject>>.
        // (i.e., we can just iterate m_activeDOMObjects without taking
        // a snapshot).
        // For more details, see https://codereview.chromium.org/247253002/.
        if (m_activeDOMObjects.contains(obj)) {
            ASSERT(obj->executionContext() == context());
            ASSERT(obj->suspendIfNeededCalled());
            obj->resume();
        }
    }
}

void ContextLifecycleNotifier::notifySuspendingActiveDOMObjects()
{
    TemporaryChange<IterationType> scope(m_iterating, IteratingOverActiveDOMObjects);
    Vector<ActiveDOMObject*> snapshotOfActiveDOMObjects;
    copyToVector(m_activeDOMObjects, snapshotOfActiveDOMObjects);
    for (ActiveDOMObject* obj : snapshotOfActiveDOMObjects) {
        // It's possible that the ActiveDOMObject is already destructed.
        // See a FIXME above.
        if (m_activeDOMObjects.contains(obj)) {
            ASSERT(obj->executionContext() == context());
            ASSERT(obj->suspendIfNeededCalled());
            obj->suspend();
        }
    }
}

void ContextLifecycleNotifier::notifyStoppingActiveDOMObjects()
{
    TemporaryChange<IterationType> scope(m_iterating, IteratingOverActiveDOMObjects);
    Vector<ActiveDOMObject*> snapshotOfActiveDOMObjects;
    copyToVector(m_activeDOMObjects, snapshotOfActiveDOMObjects);
    for (ActiveDOMObject* obj : snapshotOfActiveDOMObjects) {
        // It's possible that the ActiveDOMObject is already destructed.
        // See a FIXME above.
        if (m_activeDOMObjects.contains(obj)) {
            ASSERT(obj->executionContext() == context());
            ASSERT(obj->suspendIfNeededCalled());
            obj->stop();
        }
    }
}

bool ContextLifecycleNotifier::hasPendingActivity() const
{
    for (ActiveDOMObject* obj : m_activeDOMObjects) {
        if (obj->hasPendingActivity())
            return true;
    }
    return false;
}

} // namespace blink
