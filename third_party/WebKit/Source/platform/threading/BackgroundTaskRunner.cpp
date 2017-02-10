// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include "base/location.h"
#include "base/task_scheduler/post_task.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

void BackgroundTaskRunner::postOnBackgroundThread(
    const WebTraceLocation& location,
    std::unique_ptr<CrossThreadClosure> closure) {
  base::PostTaskWithTraits(
      location, base::TaskTraits().WithShutdownBehavior(
                    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
      convertToBaseCallback(std::move(closure)));
}

}  // namespace blink
