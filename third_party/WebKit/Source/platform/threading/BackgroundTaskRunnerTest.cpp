// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include "platform/ThreadSafeFunctional.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

using namespace blink;

void PingPongTask(WebWaitableEvent* doneEvent)
{
    doneEvent->signal();
}

class BackgroundTaskRunnerTest : public testing::Test {
};

TEST_F(BackgroundTaskRunnerTest, RunShortTaskOnBackgroundThread)
{
    OwnPtr<WebWaitableEvent> doneEvent = adoptPtr(Platform::current()->createWaitableEvent());
    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&PingPongTask, AllowCrossThreadAccess(doneEvent.get())), BackgroundTaskRunner::TaskSizeShortRunningTask);
    // Test passes by not hanging on the following wait().
    doneEvent->wait();
}

TEST_F(BackgroundTaskRunnerTest, RunLongTaskOnBackgroundThread)
{
    OwnPtr<WebWaitableEvent> doneEvent = adoptPtr(Platform::current()->createWaitableEvent());
    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&PingPongTask, AllowCrossThreadAccess(doneEvent.get())), BackgroundTaskRunner::TaskSizeLongRunningTask);
    // Test passes by not hanging on the following wait().
    doneEvent->wait();
}

} // unnamed namespace
