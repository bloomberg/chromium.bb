// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

IntersectionObserverController* IntersectionObserverController::create(
    Document* document) {
  IntersectionObserverController* result =
      new IntersectionObserverController(document);
  result->suspendIfNeeded();
  return result;
}

IntersectionObserverController::IntersectionObserverController(
    Document* document)
    : SuspendableObject(document),
      m_weakPtrFactory(this),
      m_callbackFiredWhileSuspended(false) {}

IntersectionObserverController::~IntersectionObserverController() {}

void IntersectionObserverController::postTaskToDeliverObservations() {
  if (!m_weakPtrFactory.hasWeakPtrs()) {
    // TODO(ojan): These tasks decide whether to throttle a subframe, so they
    // need to be unthrottled, but we should throttle all the other tasks
    // (e.g. ones coming from the web page).
    TaskRunnerHelper::get(TaskType::Unthrottled, getExecutionContext())
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&IntersectionObserverController::
                                 deliverIntersectionObservations,
                             m_weakPtrFactory.createWeakPtr()));
  }
}

void IntersectionObserverController::scheduleIntersectionObserverForDelivery(
    IntersectionObserver& observer) {
  m_pendingIntersectionObservers.insert(&observer);
  postTaskToDeliverObservations();
}

void IntersectionObserverController::resume() {
  // If the callback fired while DOM objects were suspended, notifications might
  // be late, so deliver them right away (rather than waiting to fire again).
  if (m_callbackFiredWhileSuspended) {
    m_callbackFiredWhileSuspended = false;
    postTaskToDeliverObservations();
  }
}

void IntersectionObserverController::deliverIntersectionObservations() {
  ExecutionContext* context = getExecutionContext();
  if (!context) {
    m_pendingIntersectionObservers.clear();
    return;
  }
  if (context->isContextSuspended()) {
    m_callbackFiredWhileSuspended = true;
    return;
  }
  HeapHashSet<Member<IntersectionObserver>> observers;
  m_pendingIntersectionObservers.swap(observers);
  for (auto& observer : observers)
    observer->deliver();
}

void IntersectionObserverController::computeTrackedIntersectionObservations() {
  TRACE_EVENT0(
      "blink",
      "IntersectionObserverController::computeTrackedIntersectionObservations");
  for (auto& observer : m_trackedIntersectionObservers) {
    observer->computeIntersectionObservations();
    if (observer->hasEntries())
      scheduleIntersectionObserverForDelivery(*observer);
  }
}

void IntersectionObserverController::addTrackedObserver(
    IntersectionObserver& observer) {
  m_trackedIntersectionObservers.insert(&observer);
}

void IntersectionObserverController::removeTrackedObserversForRoot(
    const Node& root) {
  HeapVector<Member<IntersectionObserver>> toRemove;
  for (auto& observer : m_trackedIntersectionObservers) {
    if (observer->root() == &root)
      toRemove.push_back(observer);
  }
  m_trackedIntersectionObservers.removeAll(toRemove);
}

DEFINE_TRACE(IntersectionObserverController) {
  visitor->trace(m_trackedIntersectionObservers);
  visitor->trace(m_pendingIntersectionObservers);
  SuspendableObject::trace(visitor);
}

}  // namespace blink
