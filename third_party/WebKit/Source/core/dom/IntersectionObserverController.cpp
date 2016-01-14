// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverController.h"

#include "core/dom/Document.h"

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
    , m_timer(this, &IntersectionObserverController::deliverIntersectionObservations)
    , m_timerFiredWhileSuspended(false)
{
}

IntersectionObserverController::~IntersectionObserverController() { }

void IntersectionObserverController::scheduleIntersectionObserverForDelivery(IntersectionObserver& observer)
{
    // TODO(szager): use idle callback with a timeout.  Until we do that, there is no
    // reliable way to write a test for takeRecords, because it's impossible to guarantee
    // that javascript will get a chance to run before the timer fires.
    if (!m_timer.isActive())
        m_timer.startOneShot(0, BLINK_FROM_HERE);
    m_pendingIntersectionObservers.add(&observer);
}

void IntersectionObserverController::resume()
{
    // If the timer fired while DOM objects were suspended, notifications might be late, so deliver
    // them right away (rather than waiting for m_timer to fire again).
    if (m_timerFiredWhileSuspended) {
        m_timerFiredWhileSuspended = false;
        deliverIntersectionObservations(nullptr);
    }
}

void IntersectionObserverController::deliverIntersectionObservations(Timer<IntersectionObserverController>*)
{
    if (executionContext()->activeDOMObjectsAreSuspended()) {
        m_timerFiredWhileSuspended = true;
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
    // TODO(szager): Need to define timestamp.
    double timestamp = currentTime();
    for (auto& observer : m_trackedIntersectionObservers) {
        observer->computeIntersectionObservations(timestamp);
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
        if (observer->root() == &root)
            toRemove.append(observer);
    }
    m_trackedIntersectionObservers.removeAll(toRemove);
}

DEFINE_TRACE(IntersectionObserverController)
{
    visitor->trace(m_trackedIntersectionObservers);
    visitor->trace(m_pendingIntersectionObservers);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
