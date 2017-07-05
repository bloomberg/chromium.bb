// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThread.h"

#include <memory>
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/WaitableEvent.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtMost;

namespace blink {

using ExitCode = WorkerThread::ExitCode;

namespace {

class MockWorkerReportingProxy final : public WorkerReportingProxy {
 public:
  MockWorkerReportingProxy() = default;
  ~MockWorkerReportingProxy() override = default;

  MOCK_METHOD1(DidCreateWorkerGlobalScope, void(WorkerOrWorkletGlobalScope*));
  MOCK_METHOD0(DidInitializeWorkerContext, void());
  MOCK_METHOD2(WillEvaluateWorkerScriptMock,
               void(size_t scriptSize, size_t cachedMetadataSize));
  MOCK_METHOD1(DidEvaluateWorkerScript, void(bool success));
  MOCK_METHOD0(DidCloseWorkerGlobalScope, void());
  MOCK_METHOD0(WillDestroyWorkerGlobalScope, void());
  MOCK_METHOD0(DidTerminateWorkerThread, void());

  void WillEvaluateWorkerScript(size_t script_size,
                                size_t cached_metadata_size) override {
    script_evaluation_event_.Signal();
    WillEvaluateWorkerScriptMock(script_size, cached_metadata_size);
  }

  void WaitUntilScriptEvaluation() { script_evaluation_event_.Wait(); }

 private:
  WaitableEvent script_evaluation_event_;
};

// Used as a debugger task. Waits for a signal from the main thread.
void WaitForSignalTask(WorkerThread* worker_thread,
                       WaitableEvent* waitable_event) {
  EXPECT_TRUE(worker_thread->IsCurrentThread());

  // Notify the main thread that the debugger task is waiting for the signal.
  worker_thread->GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  waitable_event->Wait();
}

}  // namespace

class WorkerThreadTest : public ::testing::Test {
 public:
  WorkerThreadTest() {}

  void SetUp() override {
    reporting_proxy_ = WTF::MakeUnique<MockWorkerReportingProxy>();
    security_origin_ =
        SecurityOrigin::Create(KURL(kParsedURLString, "http://fake.url/"));
    worker_thread_ =
        WTF::WrapUnique(new WorkerThreadForTest(nullptr, *reporting_proxy_));
    lifecycle_observer_ = new MockWorkerThreadLifecycleObserver(
        worker_thread_->GetWorkerThreadLifecycleContext());
  }

  void TearDown() override {}

  void Start() {
    worker_thread_->StartWithSourceCode(
        security_origin_.Get(), "//fake source code",
        ParentFrameTaskRunners::Create(nullptr));
  }

  void StartWithSourceCodeNotToFinish() {
    // Use a JavaScript source code that makes an infinite loop so that we
    // can catch some kind of issues as a timeout.
    worker_thread_->StartWithSourceCode(
        security_origin_.Get(), "while(true) {}",
        ParentFrameTaskRunners::Create(nullptr));
  }

  void SetForcibleTerminationDelayInMs(
      long long forcible_termination_delay_in_ms) {
    worker_thread_->forcible_termination_delay_in_ms_ =
        forcible_termination_delay_in_ms;
  }

  bool IsForcibleTerminationTaskScheduled() {
    return worker_thread_->forcible_termination_task_handle_.IsActive();
  }

 protected:
  void ExpectReportingCalls() {
    EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidInitializeWorkerContext()).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillEvaluateWorkerScriptMock(_, _)).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidEvaluateWorkerScript(true)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope()).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
    EXPECT_CALL(*lifecycle_observer_, ContextDestroyed(_)).Times(1);
  }

  void ExpectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization() {
    EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidInitializeWorkerContext())
        .Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, WillEvaluateWorkerScriptMock(_, _))
        .Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, DidEvaluateWorkerScript(_)).Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope())
        .Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
    EXPECT_CALL(*lifecycle_observer_, ContextDestroyed(_)).Times(1);
  }

  void ExpectReportingCallsForWorkerForciblyTerminated() {
    EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidInitializeWorkerContext()).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillEvaluateWorkerScriptMock(_, _)).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidEvaluateWorkerScript(false)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope()).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
    EXPECT_CALL(*lifecycle_observer_, ContextDestroyed(_)).Times(1);
  }

  ExitCode GetExitCode() { return worker_thread_->GetExitCodeForTesting(); }

  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<MockWorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<WorkerThreadForTest> worker_thread_;
  Persistent<MockWorkerThreadLifecycleObserver> lifecycle_observer_;
};

TEST_F(WorkerThreadTest, ShouldScheduleToTerminateExecution) {
  using ThreadState = WorkerThread::ThreadState;
  MutexLocker dummy_lock(worker_thread_->thread_state_mutex_);

  EXPECT_EQ(ThreadState::kNotStarted, worker_thread_->thread_state_);
  EXPECT_FALSE(worker_thread_->ShouldScheduleToTerminateExecution(dummy_lock));

  worker_thread_->SetThreadState(dummy_lock, ThreadState::kRunning);
  EXPECT_FALSE(worker_thread_->running_debugger_task_);
  EXPECT_TRUE(worker_thread_->ShouldScheduleToTerminateExecution(dummy_lock));

  worker_thread_->running_debugger_task_ = true;
  EXPECT_FALSE(worker_thread_->ShouldScheduleToTerminateExecution(dummy_lock));

  worker_thread_->SetThreadState(dummy_lock, ThreadState::kReadyToShutdown);
  EXPECT_FALSE(worker_thread_->ShouldScheduleToTerminateExecution(dummy_lock));

  // This is necessary to satisfy DCHECK in the dtor of WorkerThread.
  worker_thread_->SetExitCode(dummy_lock, ExitCode::kGracefullyTerminated);
}

TEST_F(WorkerThreadTest, AsyncTerminate_OnIdle) {
  ExpectReportingCalls();
  Start();

  // Wait until the initialization completes and the worker thread becomes
  // idle.
  worker_thread_->WaitForInit();

  // The worker thread is not being blocked, so the worker thread should be
  // gracefully shut down.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, SyncTerminate_OnIdle) {
  ExpectReportingCalls();
  Start();

  // Wait until the initialization completes and the worker thread becomes
  // idle.
  worker_thread_->WaitForInit();

  WorkerThread::TerminateAllWorkersForTesting();

  // The worker thread may gracefully shut down before forcible termination
  // runs.
  ExitCode exit_code = GetExitCode();
  EXPECT_TRUE(ExitCode::kGracefullyTerminated == exit_code ||
              ExitCode::kSyncForciblyTerminated == exit_code);
}

TEST_F(WorkerThreadTest, AsyncTerminate_ImmediatelyAfterStart) {
  ExpectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
  Start();

  // The worker thread is not being blocked, so the worker thread should be
  // gracefully shut down.
  worker_thread_->Terminate();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, SyncTerminate_ImmediatelyAfterStart) {
  ExpectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
  Start();

  // There are two possible cases depending on timing:
  // (1) If the thread hasn't been initialized on the worker thread yet,
  // terminateAndWait() should wait for initialization and shut down the
  // thread immediately after that.
  // (2) If the thread has already been initialized on the worker thread,
  // terminateAndWait() should synchronously forcibly terminates the worker
  // execution.
  // TODO(nhiroki): Make this test deterministically pass through the case 1),
  // that is, terminateAndWait() is called before initializeOnWorkerThread().
  // Then, rename this test to SyncTerminate_BeforeInitialization.
  WorkerThread::TerminateAllWorkersForTesting();
  ExitCode exit_code = GetExitCode();
  EXPECT_TRUE(ExitCode::kGracefullyTerminated == exit_code ||
              ExitCode::kSyncForciblyTerminated == exit_code);
}

TEST_F(WorkerThreadTest, AsyncTerminate_WhileTaskIsRunning) {
  const long long kForcibleTerminationDelayInMs = 10;
  SetForcibleTerminationDelayInMs(kForcibleTerminationDelayInMs);

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  // terminate() schedules a force termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Multiple terminate() calls should not take effect.
  worker_thread_->Terminate();
  worker_thread_->Terminate();
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Wait until the force termination task runs.
  testing::RunDelayedTasks(
      TimeDelta::FromMilliseconds(kForcibleTerminationDelayInMs));
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kAsyncForciblyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, SyncTerminate_WhileTaskIsRunning) {
  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  // terminateAndWait() synchronously terminates the worker execution.
  WorkerThread::TerminateAllWorkersForTesting();
  EXPECT_EQ(ExitCode::kSyncForciblyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest,
       AsyncTerminateAndThenSyncTerminate_WhileTaskIsRunning) {
  const long long kForcibleTerminationDelayInMs = 10;
  SetForcibleTerminationDelayInMs(kForcibleTerminationDelayInMs);

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  // terminate() schedules a force termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // terminateAndWait() should overtake the scheduled force termination task.
  WorkerThread::TerminateAllWorkersForTesting();
  EXPECT_FALSE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kSyncForciblyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, Terminate_WhileDebuggerTaskIsRunningOnInitialization) {
  EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
  EXPECT_CALL(*reporting_proxy_, DidInitializeWorkerContext()).Times(1);
  EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope()).Times(1);
  EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
  EXPECT_CALL(*lifecycle_observer_, ContextDestroyed(_)).Times(1);

  std::unique_ptr<Vector<CSPHeaderAndType>> headers =
      WTF::MakeUnique<Vector<CSPHeaderAndType>>();
  CSPHeaderAndType header_and_type("contentSecurityPolicy",
                                   kContentSecurityPolicyHeaderTypeReport);
  headers->push_back(header_and_type);

  // Specify PauseWorkerGlobalScopeOnStart so that the worker thread can pause
  // on initialziation to run debugger tasks.
  std::unique_ptr<WorkerThreadStartupData> startup_data =
      WorkerThreadStartupData::Create(
          KURL(kParsedURLString, "http://fake.url/"), "fake user agent",
          "//fake source code", nullptr, /* cachedMetaData */
          kPauseWorkerGlobalScopeOnStart, headers.get(), "",
          security_origin_.Get(), nullptr, /* workerClients */
          kWebAddressSpaceLocal, nullptr /* originTrialToken */,
          nullptr /* WorkerSettings */, WorkerV8Settings::Default());
  worker_thread_->Start(std::move(startup_data),
                        ParentFrameTaskRunners::Create(nullptr));

  // Used to wait for worker thread termination in a debugger task on the
  // worker thread.
  WaitableEvent waitable_event;
  worker_thread_->AppendDebuggerTask(CrossThreadBind(
      &WaitForSignalTask, CrossThreadUnretained(worker_thread_.get()),
      CrossThreadUnretained(&waitable_event)));

  // Wait for the debugger task.
  testing::EnterRunLoop();
  EXPECT_TRUE(worker_thread_->running_debugger_task_);

  // terminate() should not schedule a force termination task because there is
  // a running debugger task.
  worker_thread_->Terminate();
  EXPECT_FALSE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Multiple terminate() calls should not take effect.
  worker_thread_->Terminate();
  worker_thread_->Terminate();
  EXPECT_FALSE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Focible script termination request should also respect the running debugger
  // task.
  worker_thread_->EnsureScriptExecutionTerminates(
      ExitCode::kSyncForciblyTerminated);
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Resume the debugger task. Shutdown starts after that.
  waitable_event.Signal();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, Terminate_WhileDebuggerTaskIsRunning) {
  ExpectReportingCalls();
  Start();
  worker_thread_->WaitForInit();

  // Used to wait for worker thread termination in a debugger task on the
  // worker thread.
  WaitableEvent waitable_event;
  worker_thread_->AppendDebuggerTask(CrossThreadBind(
      &WaitForSignalTask, CrossThreadUnretained(worker_thread_.get()),
      CrossThreadUnretained(&waitable_event)));

  // Wait for the debugger task.
  testing::EnterRunLoop();
  EXPECT_TRUE(worker_thread_->running_debugger_task_);

  // terminate() should not schedule a force termination task because there is
  // a running debugger task.
  worker_thread_->Terminate();
  EXPECT_FALSE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Multiple terminate() calls should not take effect.
  worker_thread_->Terminate();
  worker_thread_->Terminate();
  EXPECT_FALSE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Focible script termination request should also respect the running debugger
  // task.
  worker_thread_->EnsureScriptExecutionTerminates(
      ExitCode::kSyncForciblyTerminated);
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Resume the debugger task. Shutdown starts after that.
  waitable_event.Signal();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

}  // namespace blink
