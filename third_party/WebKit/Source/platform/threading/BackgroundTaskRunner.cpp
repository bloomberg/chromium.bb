// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include "base/location.h"
#include "base/task_scheduler/post_task.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

void BackgroundTaskRunner::PostOnBackgroundThread(
    const WebTraceLocation& location,
    CrossThreadClosure closure) {
  base::PostTaskWithTraits(location,
                           {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                           ConvertToBaseCallback(std::move(closure)));
}

}  // namespace blink
