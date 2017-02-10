// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include <memory>
#include "base/test/scoped_async_task_scheduler.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

void PingPongTask(WaitableEvent* doneEvent) {
  doneEvent->signal();
}

}  // namespace

TEST(BackgroundTaskRunnerTest, RunOnBackgroundThread) {
  base::test::ScopedAsyncTaskScheduler scopedAsyncTaskScheduler;
  std::unique_ptr<WaitableEvent> doneEvent = WTF::makeUnique<WaitableEvent>();
  BackgroundTaskRunner::postOnBackgroundThread(
      BLINK_FROM_HERE,
      crossThreadBind(&PingPongTask, crossThreadUnretained(doneEvent.get())));
  // Test passes by not hanging on the following wait().
  doneEvent->wait();
}

}  // namespace blink
