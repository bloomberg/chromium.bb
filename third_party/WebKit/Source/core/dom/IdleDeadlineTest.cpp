// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IdleDeadline.h"

#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class MockScheduler final : public WebScheduler {
 public:
  MockScheduler() {}
  ~MockScheduler() override {}

  // WebScheduler implementation:
  WebTaskRunner* LoadingTaskRunner() override { return nullptr; }
  WebTaskRunner* TimerTaskRunner() override { return nullptr; }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return true; }
  bool CanExceedIdleDeadlineIfRequired() override { return false; }
  void PostIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
  void PostNonNestableIdleTask(const WebTraceLocation&,
                               WebThread::IdleTask*) override {}
  std::unique_ptr<WebViewScheduler> CreateWebViewScheduler(
      InterventionReporter*) override {
    return nullptr;
  }
  WebTaskRunner* CompositorTaskRunner() override { return nullptr; }
  void SuspendTimerQueue() override {}
  void ResumeTimerQueue() override {}
  void AddPendingNavigation(
      scheduler::RendererScheduler::NavigatingFrameType) override {}
  void RemovePendingNavigation(
      scheduler::RendererScheduler::NavigatingFrameType) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScheduler);
};

class MockThread final : public WebThread {
 public:
  MockThread() {}
  ~MockThread() override {}
  bool IsCurrentThread() const override { return true; }
  WebScheduler* Scheduler() const override { return &scheduler_; }

 private:
  mutable MockScheduler scheduler_;
  DISALLOW_COPY_AND_ASSIGN(MockThread);
};

class MockPlatform : public TestingPlatformSupport {
 public:
  MockPlatform() {}
  ~MockPlatform() override {}
  WebThread* CurrentThread() override { return &thread_; }

 private:
  MockThread thread_;
  DISALLOW_COPY_AND_ASSIGN(MockPlatform);
};

}  // namespace

class IdleDeadlineTest : public testing::Test {
 public:
  void SetUp() override {
    original_time_function_ = SetTimeFunctionsForTesting([] { return 1.0; });
  }

  void TearDown() override {
    SetTimeFunctionsForTesting(original_time_function_);
  }

 private:
  TimeFunction original_time_function_;
};

TEST_F(IdleDeadlineTest, deadlineInFuture) {
  IdleDeadline* deadline =
      IdleDeadline::Create(1.25, IdleDeadline::CallbackType::kCalledWhenIdle);
  // Note: the deadline is computed with reduced resolution.
  EXPECT_FLOAT_EQ(249.995, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, deadlineInPast) {
  IdleDeadline* deadline =
      IdleDeadline::Create(0.75, IdleDeadline::CallbackType::kCalledWhenIdle);
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, yieldForHighPriorityWork) {
  ScopedTestingPlatformSupport<MockPlatform> platform;

  IdleDeadline* deadline =
      IdleDeadline::Create(1.25, IdleDeadline::CallbackType::kCalledWhenIdle);
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

}  // namespace blink
