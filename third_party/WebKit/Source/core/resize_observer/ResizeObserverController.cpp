// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/resize_observer/ResizeObserverController.h"

#include "core/resize_observer/ResizeObserver.h"

namespace blink {

ResizeObserverController::ResizeObserverController()
    : observers_changed_(false) {}

void ResizeObserverController::AddObserver(ResizeObserver& observer) {
  observers_.insert(&observer);
}

size_t ResizeObserverController::GatherObservations(size_t deeper_than) {
  size_t shallowest = ResizeObserverController::kDepthBottom;
  if (!observers_changed_)
    return shallowest;
  for (auto& observer : observers_) {
    size_t depth = observer->GatherObservations(deeper_than);
    if (depth < shallowest)
      shallowest = depth;
  }
  return shallowest;
}

bool ResizeObserverController::SkippedObservations() {
  for (auto& observer : observers_) {
    if (observer->SkippedObservations())
      return true;
  }
  return false;
}

void ResizeObserverController::DeliverObservations() {
  observers_changed_ = false;
  // Copy is needed because m_observers might get modified during
  // deliverObservations.
  HeapVector<Member<ResizeObserver>> observers;
  CopyToVector(observers_, observers);

  for (auto& observer : observers) {
    if (observer) {
      observer->DeliverObservations();
      observers_changed_ =
          observers_changed_ || observer->HasElementSizeChanged();
    }
  }
}

void ResizeObserverController::ClearObservations() {
  for (auto& observer : observers_)
    observer->ClearObservations();
}

DEFINE_TRACE(ResizeObserverController) {
  visitor->Trace(observers_);
}

}  // namespace blink
