// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

static void RunBackgroundTask(PassOwnPtr<Closure> closure)
{
    (*closure)();
}

void BackgroundTaskRunner::postOnBackgroundThread(const WebTraceLocation& location, PassOwnPtr<Closure> closure, TaskSize taskSize)
{
    tracked_objects::Location baseLocation(location.functionName(), location.fileName(), 0, nullptr);
    base::WorkerPool::PostTask(baseLocation, base::Bind(&RunBackgroundTask, closure), taskSize == TaskSizeLongRunningTask);
}

} // namespace blink
