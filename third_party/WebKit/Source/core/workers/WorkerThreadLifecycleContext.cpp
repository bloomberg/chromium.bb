// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThreadLifecycleContext.h"

#include "core/workers/WorkerThreadLifecycleObserver.h"

namespace blink {

WorkerThreadLifecycleContext::WorkerThreadLifecycleContext() {
  DCHECK(IsMainThread());
}

WorkerThreadLifecycleContext::~WorkerThreadLifecycleContext() {
  DCHECK(IsMainThread());
}

void WorkerThreadLifecycleContext::NotifyContextDestroyed() {
  DCHECK(IsMainThread());
  DCHECK(!was_context_destroyed_);
  was_context_destroyed_ = true;
  LifecycleNotifier::NotifyContextDestroyed();
}

}  // namespace blink
