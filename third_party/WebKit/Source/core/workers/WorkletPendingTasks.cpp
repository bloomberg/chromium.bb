// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletPendingTasks.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "platform/wtf/WTF.h"

namespace blink {

WorkletPendingTasks::WorkletPendingTasks(int counter,
                                         ScriptPromiseResolver* resolver)
    : counter_(counter), resolver_(resolver) {
  DCHECK(IsMainThread());
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
    resolver_->Reject(DOMException::Create(kAbortError));
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
    if (counter_ == 0)
      resolver_->Resolve();
  }
}

}  // namespace blink
