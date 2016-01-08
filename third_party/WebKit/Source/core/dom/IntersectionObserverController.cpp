// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverController.h"

#include "core/dom/Document.h"

namespace blink {

typedef HeapVector<Member<IntersectionObserver>> IntersectionObserverVector;

IntersectionObserverController::IntersectionObserverController()
    : m_timer(this, &IntersectionObserverController::deliverIntersectionObservations)
{
}

IntersectionObserverController::~IntersectionObserverController() { }

void IntersectionObserverController::scheduleIntersectionObserverForDelivery(IntersectionObserver& observer)
{
    // TODO(szager): use idle callback with a timeout
    if (!m_timer.isActive())
        m_timer.startOneShot(0, BLINK_FROM_HERE);
    m_pendingIntersectionObservers.add(&observer);
}

void IntersectionObserverController::deliverIntersectionObservations(Timer<IntersectionObserverController>*)
{
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
    for (auto& observer : m_trackedIntersectionObservers)
        observer->computeIntersectionObservations(timestamp);
}

void IntersectionObserverController::addTrackedObserver(IntersectionObserver& observer)
{
    m_trackedIntersectionObservers.add(&observer);
}

void IntersectionObserverController::removeTrackedObserversForRoot(const Element& root)
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
}

} // namespace blink
