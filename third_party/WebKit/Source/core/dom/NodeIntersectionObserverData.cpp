// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/NodeIntersectionObserverData.h"

#include "core/dom/Document.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserver.h"
#include "core/dom/IntersectionObserverController.h"

namespace blink {

NodeIntersectionObserverData::NodeIntersectionObserverData() { }

NodeIntersectionObserverData::~NodeIntersectionObserverData() { }

bool NodeIntersectionObserverData::hasIntersectionObserver() const
{
    return !m_intersectionObservers.isEmpty();
}

bool NodeIntersectionObserverData::hasIntersectionObservation() const
{
    return !m_intersectionObservations.isEmpty();
}

IntersectionObservation* NodeIntersectionObserverData::getObservationFor(IntersectionObserver& observer)
{
    auto i = m_intersectionObservations.find(&observer);
    if (i == m_intersectionObservations.end())
        return nullptr;
    return i->value;
}

void NodeIntersectionObserverData::addObservation(IntersectionObservation& observation)
{
    m_intersectionObservations.add(&observation.observer(), &observation);
}

void NodeIntersectionObserverData::removeObservation(IntersectionObserver& observer)
{
    m_intersectionObservations.remove(&observer);
}

void NodeIntersectionObserverData::activateValidIntersectionObservers(Node& node)
{
    IntersectionObserverController& controller = node.document().ensureIntersectionObserverController();
    for (auto& observer : m_intersectionObservers)
        controller.addTrackedObserver(*observer);
}

void NodeIntersectionObserverData::deactivateAllIntersectionObservers(Node& node)
{
    node.document().ensureIntersectionObserverController().removeTrackedObserversForRoot(node);
}

#if !ENABLE(OILPAN)
void NodeIntersectionObserverData::dispose()
{
    HeapVector<Member<IntersectionObserver>> observersToDisconnect;
    copyToVector(m_intersectionObservers, observersToDisconnect);
    for (auto& observer : observersToDisconnect)
        observer->disconnect();
    ASSERT(m_intersectionObservers.isEmpty());
}
#endif

WeakPtrWillBeRawPtr<Node> NodeIntersectionObserverData::createWeakPtr(Node* node)
{
#if ENABLE(OILPAN)
    return node;
#else
    if (!m_weakPointerFactory)
        m_weakPointerFactory = adoptPtrWillBeNoop(new WeakPtrFactory<Node>(node));
    WeakPtr<Node> result = m_weakPointerFactory->createWeakPtr();
    ASSERT(result.get() == node);
    return result;
#endif
}

DEFINE_TRACE(NodeIntersectionObserverData)
{
    visitor->trace(m_intersectionObservers);
    visitor->trace(m_intersectionObservations);
}

} // namespace blink
