// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/intersection_observer/IntersectionObserverController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

IntersectionObserverController* IntersectionObserverController::Create(
    Document* document) {
  IntersectionObserverController* result =
      new IntersectionObserverController(document);
  result->SuspendIfNeeded();
  return result;
}

IntersectionObserverController::IntersectionObserverController(
    Document* document)
    : SuspendableObject(document), callback_fired_while_suspended_(false) {}

IntersectionObserverController::~IntersectionObserverController() {}

void IntersectionObserverController::PostTaskToDeliverObservations() {
  DCHECK(GetExecutionContext());
  // TODO(ojan): These tasks decide whether to throttle a subframe, so they
  // need to be unthrottled, but we should throttle all the other tasks
  // (e.g. ones coming from the web page).
  TaskRunnerHelper::Get(TaskType::kUnthrottled, GetExecutionContext())
      ->PostTask(
          BLINK_FROM_HERE,
          WTF::Bind(
              &IntersectionObserverController::DeliverIntersectionObservations,
              WrapWeakPersistent(this)));
}

void IntersectionObserverController::ScheduleIntersectionObserverForDelivery(
    IntersectionObserver& observer) {
  pending_intersection_observers_.insert(&observer);
  PostTaskToDeliverObservations();
}

void IntersectionObserverController::Resume() {
  // If the callback fired while DOM objects were suspended, notifications might
  // be late, so deliver them right away (rather than waiting to fire again).
  if (callback_fired_while_suspended_) {
    callback_fired_while_suspended_ = false;
    PostTaskToDeliverObservations();
  }
}

void IntersectionObserverController::DeliverIntersectionObservations() {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    pending_intersection_observers_.clear();
    return;
  }
  if (context->IsContextSuspended()) {
    callback_fired_while_suspended_ = true;
    return;
  }
  HeapHashSet<Member<IntersectionObserver>> observers;
  pending_intersection_observers_.swap(observers);
  for (auto& observer : observers)
    observer->Deliver();
}

void IntersectionObserverController::ComputeTrackedIntersectionObservations() {
  if (!GetExecutionContext())
    return;
  TRACE_EVENT0(
      "blink",
      "IntersectionObserverController::computeTrackedIntersectionObservations");
  for (auto& observer : tracked_intersection_observers_) {
    observer->ComputeIntersectionObservations();
    if (observer->HasEntries())
      ScheduleIntersectionObserverForDelivery(*observer);
  }
}

void IntersectionObserverController::AddTrackedObserver(
    IntersectionObserver& observer) {
  tracked_intersection_observers_.insert(&observer);
}

void IntersectionObserverController::RemoveTrackedObserversForRoot(
    const Node& root) {
  HeapVector<Member<IntersectionObserver>> to_remove;
  for (auto& observer : tracked_intersection_observers_) {
    if (observer->root() == &root)
      to_remove.push_back(observer);
  }
  tracked_intersection_observers_.RemoveAll(to_remove);
}

DEFINE_TRACE(IntersectionObserverController) {
  visitor->Trace(tracked_intersection_observers_);
  visitor->Trace(pending_intersection_observers_);
  SuspendableObject::Trace(visitor);
}

}  // namespace blink
