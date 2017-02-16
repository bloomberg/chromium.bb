// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementIntersectionObserverData.h"

#include "core/dom/Document.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserver.h"
#include "core/dom/IntersectionObserverController.h"

namespace blink {

ElementIntersectionObserverData::ElementIntersectionObserverData() {}

IntersectionObservation* ElementIntersectionObserverData::getObservationFor(
    IntersectionObserver& observer) {
  auto i = m_intersectionObservations.find(&observer);
  if (i == m_intersectionObservations.end())
    return nullptr;
  return i->value;
}

void ElementIntersectionObserverData::addObserver(
    IntersectionObserver& observer) {
  m_intersectionObservers.insert(&observer);
}

void ElementIntersectionObserverData::removeObserver(
    IntersectionObserver& observer) {
  m_intersectionObservers.erase(&observer);
}

void ElementIntersectionObserverData::addObservation(
    IntersectionObservation& observation) {
  DCHECK(observation.observer());
  m_intersectionObservations.insert(
      TraceWrapperMember<IntersectionObserver>(this, observation.observer()),
      &observation);
}

void ElementIntersectionObserverData::removeObservation(
    IntersectionObserver& observer) {
  m_intersectionObservations.erase(&observer);
}

void ElementIntersectionObserverData::activateValidIntersectionObservers(
    Node& node) {
  for (auto& observer : m_intersectionObservers) {
    observer->trackingDocument()
        .ensureIntersectionObserverController()
        .addTrackedObserver(*observer);
  }
}

void ElementIntersectionObserverData::deactivateAllIntersectionObservers(
    Node& node) {
  for (auto& observer : m_intersectionObservers) {
    observer->trackingDocument()
        .ensureIntersectionObserverController()
        .addTrackedObserver(*observer);
  }
  node.document()
      .ensureIntersectionObserverController()
      .removeTrackedObserversForRoot(node);
}

DEFINE_TRACE(ElementIntersectionObserverData) {
  visitor->trace(m_intersectionObservers);
  visitor->trace(m_intersectionObservations);
}

DEFINE_TRACE_WRAPPERS(ElementIntersectionObserverData) {
  for (auto& entry : m_intersectionObservations) {
    visitor->traceWrappers(entry.key);
  }
}

}  // namespace blink
