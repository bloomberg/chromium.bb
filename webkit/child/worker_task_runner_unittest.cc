// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/child/worker_task_runner.h"

namespace webkit_glue {

class WorkerTaskRunnerTest : public testing::Test {
 public:
  void FakeStart() {
    task_runner_.OnWorkerRunLoopStarted(WebKit::WebWorkerRunLoop());
  }
  void FakeStop() {
    task_runner_.OnWorkerRunLoopStopped(WebKit::WebWorkerRunLoop());
  }
  WorkerTaskRunner task_runner_;
};

class MockObserver : public WorkerTaskRunner::Observer {
 public:
  MOCK_METHOD0(OnWorkerRunLoopStopped, void());
  void RemoveSelfOnNotify() {
    ON_CALL(*this, OnWorkerRunLoopStopped()).WillByDefault(
        testing::Invoke(this, &MockObserver::RemoveSelf));
  }
  void RemoveSelf() {
    runner_->RemoveStopObserver(this);
  }
  WorkerTaskRunner* runner_;
};

TEST_F(WorkerTaskRunnerTest, BasicObservingAndWorkerId) {
  ASSERT_EQ(0, task_runner_.CurrentWorkerId());
  MockObserver o;
  EXPECT_CALL(o, OnWorkerRunLoopStopped()).Times(1);
  FakeStart();
  task_runner_.AddStopObserver(&o);
  ASSERT_LT(0, task_runner_.CurrentWorkerId());
  FakeStop();
}

TEST_F(WorkerTaskRunnerTest, CanRemoveSelfDuringNotification) {
  MockObserver o;
  o.RemoveSelfOnNotify();
  o.runner_ = &task_runner_;
  EXPECT_CALL(o, OnWorkerRunLoopStopped()).Times(1);
  FakeStart();
  task_runner_.AddStopObserver(&o);
  FakeStop();
}

}
