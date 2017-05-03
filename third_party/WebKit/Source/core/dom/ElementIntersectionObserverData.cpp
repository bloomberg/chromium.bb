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

IntersectionObservation* ElementIntersectionObserverData::GetObservationFor(
    IntersectionObserver& observer) {
  auto i = intersection_observations_.find(&observer);
  if (i == intersection_observations_.end())
    return nullptr;
  return i->value;
}

void ElementIntersectionObserverData::AddObserver(
    IntersectionObserver& observer) {
  intersection_observers_.insert(&observer);
}

void ElementIntersectionObserverData::RemoveObserver(
    IntersectionObserver& observer) {
  intersection_observers_.erase(&observer);
}

void ElementIntersectionObserverData::AddObservation(
    IntersectionObservation& observation) {
  DCHECK(observation.Observer());
  intersection_observations_.insert(
      TraceWrapperMember<IntersectionObserver>(this, observation.Observer()),
      &observation);
}

void ElementIntersectionObserverData::RemoveObservation(
    IntersectionObserver& observer) {
  intersection_observations_.erase(&observer);
}

void ElementIntersectionObserverData::ActivateValidIntersectionObservers(
    Node& node) {
  for (auto& observer : intersection_observers_) {
    observer->TrackingDocument()
        .EnsureIntersectionObserverController()
        .AddTrackedObserver(*observer);
  }
  for (auto& observation : intersection_observations_)
    observation.value->UpdateShouldReportRootBoundsAfterDomChange();
}

void ElementIntersectionObserverData::DeactivateAllIntersectionObservers(
    Node& node) {
  node.GetDocument()
      .EnsureIntersectionObserverController()
      .RemoveTrackedObserversForRoot(node);
}

DEFINE_TRACE(ElementIntersectionObserverData) {
  visitor->Trace(intersection_observers_);
  visitor->Trace(intersection_observations_);
}

DEFINE_TRACE_WRAPPERS(ElementIntersectionObserverData) {
  for (auto& entry : intersection_observations_) {
    visitor->TraceWrappers(entry.key);
  }
}

}  // namespace blink
