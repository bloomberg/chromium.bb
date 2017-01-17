// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IdleDeadline.h"

#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/CurrentTime.h"

namespace blink {
namespace {

class MockScheduler final : public WebScheduler {
 public:
  MockScheduler() {}
  ~MockScheduler() override {}

  // WebScheduler implementation:
  WebTaskRunner* loadingTaskRunner() override { return nullptr; }
  WebTaskRunner* timerTaskRunner() override { return nullptr; }
  void shutdown() override {}
  bool shouldYieldForHighPriorityWork() override { return true; }
  bool canExceedIdleDeadlineIfRequired() override { return false; }
  void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
  void postNonNestableIdleTask(const WebTraceLocation&,
                               WebThread::IdleTask*) override {}
  std::unique_ptr<WebViewScheduler> createWebViewScheduler(
      InterventionReporter*,
      WebViewScheduler::WebViewSchedulerSettings*) override {
    return nullptr;
  }
  void suspendTimerQueue() override {}
  void resumeTimerQueue() override {}
  void addPendingNavigation(WebScheduler::NavigatingFrameType) override {}
  void removePendingNavigation(WebScheduler::NavigatingFrameType) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScheduler);
};

class MockThread final : public WebThread {
 public:
  MockThread() {}
  ~MockThread() override {}
  bool isCurrentThread() const override { return true; }
  WebScheduler* scheduler() const override { return &m_scheduler; }

 private:
  mutable MockScheduler m_scheduler;
  DISALLOW_COPY_AND_ASSIGN(MockThread);
};

class MockPlatform : public TestingPlatformSupport {
 public:
  MockPlatform() {}
  ~MockPlatform() override {}
  WebThread* currentThread() override { return &m_thread; }

 private:
  MockThread m_thread;
  DISALLOW_COPY_AND_ASSIGN(MockPlatform);
};

}  // namespace

class IdleDeadlineTest : public testing::Test {
 public:
  void SetUp() override {
    m_originalTimeFunction = setTimeFunctionsForTesting([] { return 1.0; });
  }

  void TearDown() override {
    setTimeFunctionsForTesting(m_originalTimeFunction);
  }

 private:
  TimeFunction m_originalTimeFunction;
};

TEST_F(IdleDeadlineTest, deadlineInFuture) {
  IdleDeadline* deadline =
      IdleDeadline::create(1.25, IdleDeadline::CallbackType::CalledWhenIdle);
  // Note: the deadline is computed with reduced resolution.
  EXPECT_FLOAT_EQ(249.995, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, deadlineInPast) {
  IdleDeadline* deadline =
      IdleDeadline::create(0.75, IdleDeadline::CallbackType::CalledWhenIdle);
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, yieldForHighPriorityWork) {
  ScopedTestingPlatformSupport<MockPlatform> platform;

  IdleDeadline* deadline =
      IdleDeadline::create(1.25, IdleDeadline::CallbackType::CalledWhenIdle);
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

}  // namespace blink
