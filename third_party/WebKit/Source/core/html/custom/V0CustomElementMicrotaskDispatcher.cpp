// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/V0CustomElementMicrotaskDispatcher.h"

#include "core/html/custom/V0CustomElementCallbackQueue.h"
#include "core/html/custom/V0CustomElementMicrotaskImportStep.h"
#include "core/html/custom/V0CustomElementProcessingStack.h"
#include "core/html/custom/V0CustomElementScheduler.h"
#include "platform/bindings/Microtask.h"

namespace blink {

static const V0CustomElementCallbackQueue::ElementQueueId kMicrotaskQueueId = 0;

V0CustomElementMicrotaskDispatcher::V0CustomElementMicrotaskDispatcher()
    : has_scheduled_microtask_(false), phase_(kQuiescent) {}

V0CustomElementMicrotaskDispatcher&
V0CustomElementMicrotaskDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(V0CustomElementMicrotaskDispatcher, instance,
                      (new V0CustomElementMicrotaskDispatcher));
  return instance;
}

void V0CustomElementMicrotaskDispatcher::Enqueue(
    V0CustomElementCallbackQueue* queue) {
  EnsureMicrotaskScheduledForElementQueue();
  queue->SetOwner(kMicrotaskQueueId);
  elements_.push_back(queue);
}

void V0CustomElementMicrotaskDispatcher::
    EnsureMicrotaskScheduledForElementQueue() {
  DCHECK(phase_ == kQuiescent || phase_ == kResolving);
  EnsureMicrotaskScheduled();
}

void V0CustomElementMicrotaskDispatcher::EnsureMicrotaskScheduled() {
  if (!has_scheduled_microtask_) {
    Microtask::EnqueueMicrotask(WTF::Bind(&Dispatch));
    has_scheduled_microtask_ = true;
  }
}

void V0CustomElementMicrotaskDispatcher::Dispatch() {
  Instance().DoDispatch();
}

void V0CustomElementMicrotaskDispatcher::DoDispatch() {
  DCHECK(IsMainThread());

  DCHECK(phase_ == kQuiescent);
  DCHECK(has_scheduled_microtask_);
  has_scheduled_microtask_ = false;

  // Finishing microtask work deletes all
  // V0CustomElementCallbackQueues. Being in a callback delivery scope
  // implies those queues could still be in use.
  SECURITY_DCHECK(!V0CustomElementProcessingStack::InCallbackDeliveryScope());

  phase_ = kResolving;

  phase_ = kDispatchingCallbacks;
  for (const auto& element : elements_) {
    // Created callback may enqueue an attached callback.
    V0CustomElementProcessingStack::CallbackDeliveryScope scope;
    element->ProcessInElementQueue(kMicrotaskQueueId);
  }

  elements_.clear();
  V0CustomElementScheduler::MicrotaskDispatcherDidFinish();
  phase_ = kQuiescent;
}

void V0CustomElementMicrotaskDispatcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(elements_);
}

}  // namespace blink
