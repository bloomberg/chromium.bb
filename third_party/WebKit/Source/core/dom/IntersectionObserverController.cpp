// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverController.h"

#include "core/dom/Document.h"
#include "core/dom/IdleRequestOptions.h"

namespace blink {

typedef HeapVector<Member<IntersectionObserver>> IntersectionObserverVector;

IntersectionObserverController* IntersectionObserverController::create(Document* document)
{
    IntersectionObserverController* result = new IntersectionObserverController(document);
    result->suspendIfNeeded();
    return result;
}

IntersectionObserverController::IntersectionObserverController(Document* document)
    : ActiveDOMObject(document)
    , m_callbackIsScheduled(false)
    , m_callbackFiredWhileSuspended(false)
{
}

IntersectionObserverController::~IntersectionObserverController() { }

void IntersectionObserverController::scheduleIntersectionObserverForDelivery(IntersectionObserver& observer)
{
    m_pendingIntersectionObservers.add(&observer);
    if (m_callbackIsScheduled)
        return;
    Document* document = toDocument(getExecutionContext());
    if (!document)
        return;
    m_callbackIsScheduled = true;
    IdleRequestOptions options;
    // The IntersectionObserver spec mandates that notifications be sent within 100ms.
    options.setTimeout(100);
    document->requestIdleCallback(this, options);
}

void IntersectionObserverController::resume()
{
    // If the callback fired while DOM objects were suspended, notifications might be late, so deliver
    // them right away (rather than waiting to fire again).
    if (m_callbackFiredWhileSuspended) {
        m_callbackFiredWhileSuspended = false;
        deliverIntersectionObservations();
    }
}

void IntersectionObserverController::handleEvent(IdleDeadline*)
{
    m_callbackIsScheduled = false;
    deliverIntersectionObservations();
}

void IntersectionObserverController::deliverIntersectionObservations()
{
    if (getExecutionContext()->activeDOMObjectsAreSuspended()) {
        m_callbackFiredWhileSuspended = true;
        return;
    }
    IntersectionObserverVector observers;
    copyToVector(m_pendingIntersectionObservers, observers);
    m_pendingIntersectionObservers.clear();
    for (auto& observer : observers)
        observer->deliver();
}

void IntersectionObserverController::computeTrackedIntersectionObservations()
{
    for (auto& observer : m_trackedIntersectionObservers) {
        observer->computeIntersectionObservations();
        if (observer->hasEntries())
            scheduleIntersectionObserverForDelivery(*observer);
    }
}

void IntersectionObserverController::addTrackedObserver(IntersectionObserver& observer)
{
    m_trackedIntersectionObservers.add(&observer);
}

void IntersectionObserverController::removeTrackedObserversForRoot(const Node& root)
{
    HeapVector<Member<IntersectionObserver>> toRemove;
    for (auto& observer : m_trackedIntersectionObservers) {
        if (observer->rootNode() == &root)
            toRemove.append(observer);
    }
    m_trackedIntersectionObservers.removeAll(toRemove);
}

DEFINE_TRACE(IntersectionObserverController)
{
    visitor->trace(m_trackedIntersectionObservers);
    visitor->trace(m_pendingIntersectionObservers);
    ActiveDOMObject::trace(visitor);
    IdleRequestCallback::trace(visitor);
}

} // namespace blink
