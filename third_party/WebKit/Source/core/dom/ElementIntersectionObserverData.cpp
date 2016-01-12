// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementIntersectionObserverData.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserver.h"
#include "core/dom/IntersectionObserverController.h"

namespace blink {

ElementIntersectionObserverData::ElementIntersectionObserverData() { }

ElementIntersectionObserverData::~ElementIntersectionObserverData() { }

bool ElementIntersectionObserverData::hasIntersectionObserver() const
{
    return !m_intersectionObservers.isEmpty();
}

bool ElementIntersectionObserverData::hasIntersectionObservation() const
{
    return !m_intersectionObservations.isEmpty();
}

IntersectionObservation* ElementIntersectionObserverData::getObservationFor(IntersectionObserver& observer)
{
    auto i = m_intersectionObservations.find(&observer);
    if (i == m_intersectionObservations.end())
        return nullptr;
    return i->value;
}

void ElementIntersectionObserverData::addObservation(IntersectionObservation& observation)
{
    m_intersectionObservations.add(&observation.observer(), &observation);
}

void ElementIntersectionObserverData::removeObservation(IntersectionObserver& observer)
{
    m_intersectionObservations.remove(&observer);
}

void ElementIntersectionObserverData::activateValidIntersectionObservers(Element& element)
{
    IntersectionObserverController& controller = element.document().ensureIntersectionObserverController();
    for (auto& observer : m_intersectionObservers) {
        controller.addTrackedObserver(*observer);
        observer->setActive(true);
    }
    for (auto& observation : m_intersectionObservations)
        observation.value->setActive(observation.key->isDescendantOfRoot(&element));
}

void ElementIntersectionObserverData::deactivateAllIntersectionObservers(Element& element)
{
    element.document().ensureIntersectionObserverController().removeTrackedObserversForRoot(element);
    for (auto& observer : m_intersectionObservers)
        observer->setActive(false);
    for (auto& observation : m_intersectionObservations)
        observation.value->setActive(false);
}

#if !ENABLE(OILPAN)
void ElementIntersectionObserverData::dispose()
{
    HeapVector<Member<IntersectionObserver>> observersToDisconnect;
    copyToVector(m_intersectionObservers, observersToDisconnect);
    for (auto& observer : observersToDisconnect)
        observer->disconnect();
    ASSERT(m_intersectionObservers.isEmpty());
}
#endif

WeakPtrWillBeRawPtr<Element> ElementIntersectionObserverData::createWeakPtr(Element* element)
{
#if ENABLE(OILPAN)
    return element;
#else
    if (!m_weakPointerFactory)
        m_weakPointerFactory = adoptPtrWillBeNoop(new WeakPtrFactory<Element>(element));
    return m_weakPointerFactory->createWeakPtr();
#endif
}

DEFINE_TRACE(ElementIntersectionObserverData)
{
    visitor->trace(m_intersectionObservers);
    visitor->trace(m_intersectionObservations);
}

} // namespace blink
