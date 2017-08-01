// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptedIdleTaskController.h"

#include "core/dom/IdleRequestCallback.h"
#include "core/dom/IdleRequestOptions.h"
#include "core/testing/NullExecutionContext.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class MockScriptedIdleTaskControllerScheduler final : public WebScheduler {
 public:
  MockScriptedIdleTaskControllerScheduler(bool should_yield)
      : should_yield_(should_yield) {}
  ~MockScriptedIdleTaskControllerScheduler() override {}

  // WebScheduler implementation:
  WebTaskRunner* LoadingTaskRunner() override { return nullptr; }
  WebTaskRunner* TimerTaskRunner() override { return nullptr; }
  WebTaskRunner* CompositorTaskRunner() override { return nullptr; }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return should_yield_; }
  bool CanExceedIdleDeadlineIfRequired() override { return false; }
  void PostIdleTask(const WebTraceLocation&,
                    WebThread::IdleTask* idle_task) override {
    idle_task_.reset(idle_task);
  }
  void PostNonNestableIdleTask(const WebTraceLocation&,
                               WebThread::IdleTask*) override {}
  std::unique_ptr<WebViewScheduler> CreateWebViewScheduler(
      InterventionReporter*,
      WebViewScheduler::WebViewSchedulerDelegate*) override {
    return nullptr;
  }
  void PauseTimerQueue() override {}
  void ResumeTimerQueue() override {}
  void AddPendingNavigation(
      scheduler::RendererScheduler::NavigatingFrameType) override {}
  void RemovePendingNavigation(
      scheduler::RendererScheduler::NavigatingFrameType) override {}

  void RunIdleTask() {
    auto idle_task = std::move(idle_task_);
    idle_task->Run(0);
  }
  bool HasIdleTask() const { return idle_task_.get(); }

 private:
  bool should_yield_;
  std::unique_ptr<WebThread::IdleTask> idle_task_;

  DISALLOW_COPY_AND_ASSIGN(MockScriptedIdleTaskControllerScheduler);
};

class MockScriptedIdleTaskControllerThread final : public WebThread {
 public:
  MockScriptedIdleTaskControllerThread(bool should_yield)
      : scheduler_(should_yield) {}
  ~MockScriptedIdleTaskControllerThread() override {}
  bool IsCurrentThread() const override { return true; }
  WebScheduler* Scheduler() const override { return &scheduler_; }

  void RunIdleTask() { scheduler_.RunIdleTask(); }
  bool HasIdleTask() const { return scheduler_.HasIdleTask(); }

 private:
  mutable MockScriptedIdleTaskControllerScheduler scheduler_;
  DISALLOW_COPY_AND_ASSIGN(MockScriptedIdleTaskControllerThread);
};

class MockScriptedIdleTaskControllerPlatform : public TestingPlatformSupport {
 public:
  MockScriptedIdleTaskControllerPlatform(bool should_yield)
      : thread_(should_yield) {}
  ~MockScriptedIdleTaskControllerPlatform() override {}
  WebThread* CurrentThread() override { return &thread_; }

  void RunIdleTask() { thread_.RunIdleTask(); }
  bool HasIdleTask() const { return thread_.HasIdleTask(); }

 private:
  MockScriptedIdleTaskControllerThread thread_;
  DISALLOW_COPY_AND_ASSIGN(MockScriptedIdleTaskControllerPlatform);
};

class MockIdleRequestCallback : public IdleRequestCallback {
 public:
  MOCK_METHOD1(handleEvent, void(IdleDeadline*));
};

}  // namespace

class ScriptedIdleTaskControllerTest : public ::testing::Test {
 public:
  void SetUp() override { execution_context_ = new NullExecutionContext(); }

 protected:
  Persistent<ExecutionContext> execution_context_;
};

TEST_F(ScriptedIdleTaskControllerTest, RunCallback) {
  ScopedTestingPlatformSupport<MockScriptedIdleTaskControllerPlatform, bool>
      platform(false);
  NullExecutionContext execution_context;
  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  Persistent<MockIdleRequestCallback> callback(new MockIdleRequestCallback());
  IdleRequestOptions options;
  EXPECT_FALSE(platform->HasIdleTask());
  int id = controller->RegisterCallback(callback, options);
  EXPECT_TRUE(platform->HasIdleTask());
  EXPECT_NE(0, id);

  EXPECT_CALL(*callback, handleEvent(::testing::_));
  platform->RunIdleTask();
  ::testing::Mock::VerifyAndClearExpectations(callback);
  EXPECT_FALSE(platform->HasIdleTask());
}

TEST_F(ScriptedIdleTaskControllerTest, DontRunCallbackWhenAskedToYield) {
  ScopedTestingPlatformSupport<MockScriptedIdleTaskControllerPlatform, bool>
      platform(true);
  NullExecutionContext execution_context;
  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  Persistent<MockIdleRequestCallback> callback(new MockIdleRequestCallback());
  IdleRequestOptions options;
  int id = controller->RegisterCallback(callback, options);
  EXPECT_NE(0, id);

  EXPECT_CALL(*callback, handleEvent(::testing::_)).Times(0);
  platform->RunIdleTask();
  ::testing::Mock::VerifyAndClearExpectations(callback);

  // The idle task should have been reposted.
  EXPECT_TRUE(platform->HasIdleTask());
}

}  // namespace blink
