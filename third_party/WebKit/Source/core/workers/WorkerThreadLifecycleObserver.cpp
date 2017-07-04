// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThreadLifecycleObserver.h"

#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadLifecycleContext.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/WTF.h"

namespace blink {

WorkerThreadLifecycleObserver::WorkerThreadLifecycleObserver(
    WorkerThreadLifecycleContext* worker_thread_lifecycle_context)
    : LifecycleObserver(worker_thread_lifecycle_context),
      was_context_destroyed_before_observer_creation_(
          worker_thread_lifecycle_context->was_context_destroyed_) {
  DCHECK(IsMainThread());
}

WorkerThreadLifecycleObserver::~WorkerThreadLifecycleObserver() {}

}  // namespace blink
