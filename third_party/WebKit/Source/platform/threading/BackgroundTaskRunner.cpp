// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/threading/BackgroundTaskRunner.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"

namespace blink {

static void RunBackgroundTask(PassOwnPtr<Closure> closure)
{
    (*closure)();
}

void BackgroundTaskRunner::postOnBackgroundThread(PassOwnPtr<Closure> closure, TaskSize taskSize)
{
    base::WorkerPool::PostTask(FROM_HERE, base::Bind(&RunBackgroundTask, closure), taskSize == TaskSizeLongRunningTask);
}

} // namespace blink
