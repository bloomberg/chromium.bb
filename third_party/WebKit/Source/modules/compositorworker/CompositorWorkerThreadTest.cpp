// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/CompositorWorkerProxyClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// A null InProcessWorkerObjectProxy, supplied when creating
// CompositorWorkerThreads.
class TestCompositorWorkerObjectProxy : public InProcessWorkerObjectProxy {
 public:
  static std::unique_ptr<TestCompositorWorkerObjectProxy> Create(
      ParentFrameTaskRunners* parent_frame_task_runners) {
    return WTF::WrapUnique(
        new TestCompositorWorkerObjectProxy(parent_frame_task_runners));
  }

  // (Empty) WorkerReportingProxy implementation:
  virtual void DispatchErrorEvent(const String& error_message,
                                  std::unique_ptr<SourceLocation>,
                                  int exception_id) {}
  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override {}
  void PostMessageToPageInspector(int session_id, const String&) override {}
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override {}
  void DidEvaluateWorkerScript(bool success) override {}
  void DidCloseWorkerGlobalScope() override {}
  void WillDestroyWorkerGlobalScope() override {}
  void DidTerminateWorkerThread() override {}

 private:
  explicit TestCompositorWorkerObjectProxy(
      ParentFrameTaskRunners* parent_frame_task_runners)
      : InProcessWorkerObjectProxy(nullptr, parent_frame_task_runners) {}
};

class TestCompositorWorkerProxyClient
    : public GarbageCollected<TestCompositorWorkerProxyClient>,
      public CompositorWorkerProxyClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestCompositorWorkerProxyClient);

 public:
  TestCompositorWorkerProxyClient() {}

  void Dispose() override {}
  void SetGlobalScope(WorkerGlobalScope*) override {}
  void RequestAnimationFrame() override {}
};

class CompositorWorkerTestPlatform : public TestingPlatformSupport {
 public:
  CompositorWorkerTestPlatform()
      : thread_(old_platform_->CreateThread("Compositor")) {}

  WebThread* CompositorThread() const override { return thread_.get(); }

  WebCompositorSupport* CompositorSupport() override {
    return &compositor_support_;
  }

 private:
  std::unique_ptr<WebThread> thread_;
  TestingCompositorSupport compositor_support_;
};

}  // namespace

class CompositorWorkerThreadTest : public ::testing::Test {
 public:
  void SetUp() override {
    CompositorWorkerThread::CreateSharedBackingThreadForTest();
    parent_frame_task_runners_ = ParentFrameTaskRunners::Create();
    object_proxy_ = TestCompositorWorkerObjectProxy::Create(
        parent_frame_task_runners_.Get());
    security_origin_ =
        SecurityOrigin::Create(KURL(kParsedURLString, "http://fake.url/"));
  }

  void TearDown() override {
    CompositorWorkerThread::ClearSharedBackingThread();
  }

  std::unique_ptr<CompositorWorkerThread> CreateCompositorWorker() {
    std::unique_ptr<CompositorWorkerThread> worker_thread =
        CompositorWorkerThread::Create(nullptr, *object_proxy_);
    WorkerClients* clients = WorkerClients::Create();
    ProvideCompositorWorkerProxyClientTo(clients,
                                         new TestCompositorWorkerProxyClient);
    worker_thread->Start(
        WTF::MakeUnique<GlobalScopeCreationParams>(
            KURL(kParsedURLString, "http://fake.url/"), "fake user agent",
            "//fake source code", nullptr, kDontPauseWorkerGlobalScopeOnStart,
            nullptr, "", security_origin_.Get(), clients, kWebAddressSpaceLocal,
            nullptr, nullptr, kV8CacheOptionsDefault),
        WTF::nullopt, parent_frame_task_runners_.Get());
    return worker_thread;
  }

  // Attempts to run some simple script for |worker|.
  void CheckWorkerCanExecuteScript(WorkerThread* worker) {
    std::unique_ptr<WaitableEvent> wait_event =
        WTF::MakeUnique<WaitableEvent>();
    worker->GetWorkerBackingThread().BackingThread().PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&CompositorWorkerThreadTest::ExecuteScriptInWorker,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(worker),
                        CrossThreadUnretained(wait_event.get())));
    wait_event->Wait();
  }

 private:
  void ExecuteScriptInWorker(WorkerThread* worker, WaitableEvent* wait_event) {
    WorkerOrWorkletScriptController* script_controller =
        worker->GlobalScope()->ScriptController();
    bool evaluate_result = script_controller->Evaluate(
        ScriptSourceCode("var counter = 0; ++counter;"));
    DCHECK(evaluate_result);
    wait_event->Signal();
  }

  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<InProcessWorkerObjectProxy> object_proxy_;
  Persistent<ParentFrameTaskRunners> parent_frame_task_runners_;
  ScopedTestingPlatformSupport<CompositorWorkerTestPlatform> platform_;
};

TEST_F(CompositorWorkerThreadTest, Basic) {
  std::unique_ptr<CompositorWorkerThread> compositor_worker =
      CreateCompositorWorker();
  CheckWorkerCanExecuteScript(compositor_worker.get());
  compositor_worker->Terminate();
  compositor_worker->WaitForShutdownForTesting();
}

// Tests that the same WebThread is used for new workers if the WebThread is
// still alive.
TEST_F(CompositorWorkerThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worker and wait until it is initialized.
  std::unique_ptr<CompositorWorkerThread> first_worker =
      CreateCompositorWorker();
  WebThreadSupportingGC* first_thread =
      &first_worker->GetWorkerBackingThread().BackingThread();
  CheckWorkerCanExecuteScript(first_worker.get());
  v8::Isolate* first_isolate = first_worker->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Create the second worker and immediately destroy the first worker.
  std::unique_ptr<CompositorWorkerThread> second_worker =
      CreateCompositorWorker();
  // We don't use terminateAndWait here to avoid forcible termination.
  first_worker->Terminate();
  first_worker->WaitForShutdownForTesting();

  // Wait until the second worker is initialized. Verify that the second worker
  // is using the same thread and Isolate as the first worker.
  WebThreadSupportingGC* second_thread =
      &second_worker->GetWorkerBackingThread().BackingThread();
  ASSERT_EQ(first_thread, second_thread);

  v8::Isolate* second_isolate = second_worker->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the worker can still successfully execute script.
  CheckWorkerCanExecuteScript(second_worker.get());

  second_worker->Terminate();
  second_worker->WaitForShutdownForTesting();
}

// Tests that a new WebThread is created if all existing workers are terminated
// before a new worker is created.
TEST_F(CompositorWorkerThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worker, wait until it is initialized, and terminate it.
  std::unique_ptr<CompositorWorkerThread> compositor_worker =
      CreateCompositorWorker();
  WebThreadSupportingGC* first_thread =
      &compositor_worker->GetWorkerBackingThread().BackingThread();
  CheckWorkerCanExecuteScript(compositor_worker.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  compositor_worker->Terminate();
  compositor_worker->WaitForShutdownForTesting();

  // Create the second worker. The backing thread is same.
  compositor_worker = CreateCompositorWorker();
  WebThreadSupportingGC* second_thread =
      &compositor_worker->GetWorkerBackingThread().BackingThread();
  EXPECT_EQ(first_thread, second_thread);
  CheckWorkerCanExecuteScript(compositor_worker.get());

  compositor_worker->Terminate();
  compositor_worker->WaitForShutdownForTesting();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worker is
// created while another is terminating.
TEST_F(CompositorWorkerThreadTest, CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<CompositorWorkerThread> first_worker =
      CreateCompositorWorker();
  CheckWorkerCanExecuteScript(first_worker.get());
  v8::Isolate* first_isolate = first_worker->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Request termination of the first worker and create the second worker
  // as soon as possible.
  first_worker->Terminate();
  // We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worker thread so quickly. This could be a source of flakiness.

  std::unique_ptr<CompositorWorkerThread> second_worker =
      CreateCompositorWorker();

  v8::Isolate* second_isolate = second_worker->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worker.
  CheckWorkerCanExecuteScript(second_worker.get());
  second_worker->Terminate();
  second_worker->WaitForShutdownForTesting();
}

}  // namespace blink
