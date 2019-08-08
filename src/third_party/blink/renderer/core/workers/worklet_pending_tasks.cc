// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/workers/worklet.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

WorkletPendingTasks::WorkletPendingTasks(Worklet* worklet,
                                         ScriptPromiseResolver* resolver)
    : resolver_(resolver), worklet_(worklet) {
  DCHECK(IsMainThread());
}

void WorkletPendingTasks::InitializeCounter(int counter) {
  DCHECK(IsMainThread());
  counter_ = counter;
}

void WorkletPendingTasks::Abort() {
  DCHECK(IsMainThread());
  // Step 3: "If script is null, then queue a task on outsideSettings's
  // responsible event loop to run these steps:"
  //   1: "If pendingTaskStruct's counter is not -1, then run these steps:"
  //     1: "Set pendingTaskStruct's counter to -1."
  //     2: "Reject promise with an "AbortError" DOMException."
  if (counter_ != -1) {
    counter_ = -1;
    worklet_->FinishPendingTasks(this);
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }
}

void WorkletPendingTasks::DecrementCounter() {
  DCHECK(IsMainThread());
  // Step 5: "Queue a task on outsideSettings's responsible event loop to run
  // these steps:"
  //   1: "If pendingTaskStruct's counter is not -1, then run these steps:"
  //     1: "Decrement pendingTaskStruct's counter by 1."
  //     2: "If pendingTaskStruct's counter is 0, then resolve promise."
  if (counter_ != -1) {
    --counter_;
    if (counter_ == 0) {
      worklet_->FinishPendingTasks(this);
      resolver_->Resolve();
    }
  }
}

void WorkletPendingTasks::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  visitor->Trace(worklet_);
}

}  // namespace blink
