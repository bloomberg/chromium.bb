// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include <memory>
#include "base/test/scoped_task_environment.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void PingPongTask(WaitableEvent* done_event) {
  done_event->Signal();
}

}  // namespace

TEST(BackgroundTaskRunnerTest, RunOnBackgroundThread) {
  base::test::ScopedTaskEnvironment scoped_task_environment;
  std::unique_ptr<WaitableEvent> done_event = std::make_unique<WaitableEvent>();
  BackgroundTaskRunner::PostOnBackgroundThread(
      BLINK_FROM_HERE,
      CrossThreadBind(&PingPongTask, CrossThreadUnretained(done_event.get())));
  // Test passes by not hanging on the following wait().
  done_event->Wait();
}

}  // namespace blink
