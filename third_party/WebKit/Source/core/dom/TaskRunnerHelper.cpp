// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/workers/WorkerThread.h"

namespace blink {

scoped_refptr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    WorkerThread* worker_thread) {
  return worker_thread->GetGlobalScopeScheduler()->GetTaskRunner(type);
}

}  // namespace blink
