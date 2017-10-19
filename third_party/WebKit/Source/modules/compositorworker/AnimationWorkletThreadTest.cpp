// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletThread.h"

#include <memory>
#include "bindings/core/v8/ScriptModule.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/TextPosition.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebURLRequest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class AnimationWorkletTestPlatform : public TestingPlatformSupport {
 public:
  AnimationWorkletTestPlatform()
      : thread_(old_platform_->CreateThread("Compositor")) {}

  WebThread* CompositorThread() const override { return thread_.get(); }

  WebCompositorSupport* CompositorSupport() override {
    return &compositor_support_;
  }

 private:
  std::unique_ptr<WebThread> thread_;
  TestingCompositorSupport compositor_support_;
};

class TestAnimationWorkletProxyClient
    : public GarbageCollected<TestAnimationWorkletProxyClient>,
      public AnimationWorkletProxyClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestAnimationWorkletProxyClient);

 public:
  TestAnimationWorkletProxyClient() {}
  void SetGlobalScope(WorkletGlobalScope*) override {}
  void Dispose() override {}
};

}  // namespace

class AnimationWorkletThreadTest : public ::testing::Test {
 public:
  void SetUp() override {
    AnimationWorkletThread::CreateSharedBackingThreadForTest();
    reporting_proxy_ = WTF::MakeUnique<WorkerReportingProxy>();
    security_origin_ = SecurityOrigin::Create(KURL("http://fake.url/"));
  }

  void TearDown() override {
    AnimationWorkletThread::ClearSharedBackingThread();
  }

  std::unique_ptr<AnimationWorkletThread> CreateAnimationWorkletThread() {
    WorkerClients* clients = WorkerClients::Create();
    ProvideAnimationWorkletProxyClientTo(clients,
                                         new TestAnimationWorkletProxyClient());

    std::unique_ptr<AnimationWorkletThread> thread =
        AnimationWorkletThread::Create(nullptr, *reporting_proxy_);

    thread->Start(WTF::MakeUnique<GlobalScopeCreationParams>(
                      KURL("http://fake.url/"), "fake user agent", "", nullptr,
                      kDontPauseWorkerGlobalScopeOnStart, nullptr, "",
                      security_origin_.get(), clients, kWebAddressSpaceLocal,
                      nullptr, nullptr, kV8CacheOptionsDefault),
                  WTF::nullopt, ParentFrameTaskRunners::Create());
    return thread;
  }

  // Attempts to run some simple script for |thread|.
  void CheckWorkletCanExecuteScript(WorkerThread* thread) {
    std::unique_ptr<WaitableEvent> wait_event =
        WTF::MakeUnique<WaitableEvent>();
    thread->GetWorkerBackingThread().BackingThread().PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&AnimationWorkletThreadTest::ExecuteScriptInWorklet,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(thread),
                        CrossThreadUnretained(wait_event.get())));
    wait_event->Wait();
  }

 private:
  void ExecuteScriptInWorklet(WorkerThread* thread, WaitableEvent* wait_event) {
    ScriptState* script_state =
        thread->GlobalScope()->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptState::Scope scope(script_state);
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), "var counter = 0; ++counter;", "worklet.js",
        kSharableCrossOrigin, WebURLRequest::kFetchCredentialsModeOmit,
        "" /* nonce */, kParserInserted, TextPosition::MinimumPosition(),
        ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());
    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptValue value =
        module.Evaluate(script_state, CaptureEvalErrorFlag::kCapture);
    EXPECT_TRUE(value.IsEmpty());
    wait_event->Signal();
  }

  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
  ScopedTestingPlatformSupport<AnimationWorkletTestPlatform> platform_;
};

TEST_F(AnimationWorkletThreadTest, Basic) {
  ScopedTestingPlatformSupport<AnimationWorkletTestPlatform> platform;

  std::unique_ptr<AnimationWorkletThread> worklet =
      CreateAnimationWorkletThread();
  CheckWorkletCanExecuteScript(worklet.get());
  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

// Tests that the same WebThread is used for new worklets if the WebThread is
// still alive.
TEST_F(AnimationWorkletThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
  std::unique_ptr<AnimationWorkletThread> first_worklet =
      CreateAnimationWorkletThread();
  WebThreadSupportingGC* first_thread =
      &first_worklet->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(first_worklet.get());
  v8::Isolate* first_isolate = first_worklet->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Create the second worklet and immediately destroy the first worklet.
  std::unique_ptr<AnimationWorkletThread> second_worklet =
      CreateAnimationWorkletThread();
  // We don't use terminateAndWait here to avoid forcible termination.
  first_worklet->Terminate();
  first_worklet->WaitForShutdownForTesting();

  // Wait until the second worklet is initialized. Verify that the second
  // worklet is using the same thread and Isolate as the first worklet.
  WebThreadSupportingGC* second_thread =
      &second_worklet->GetWorkerBackingThread().BackingThread();
  ASSERT_EQ(first_thread, second_thread);

  v8::Isolate* second_isolate = second_worklet->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the worklet can still successfully execute script.
  CheckWorkletCanExecuteScript(second_worklet.get());

  second_worklet->Terminate();
  second_worklet->WaitForShutdownForTesting();
}

// Tests that a new WebThread is created if all existing worklets are
// terminated before a new worklet is created.
TEST_F(AnimationWorkletThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<AnimationWorkletThread> worklet =
      CreateAnimationWorkletThread();
  WebThreadSupportingGC* first_thread =
      &worklet->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(worklet.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet->Terminate();
  worklet->WaitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet = CreateAnimationWorkletThread();
  WebThreadSupportingGC* second_thread =
      &worklet->GetWorkerBackingThread().BackingThread();
  EXPECT_EQ(first_thread, second_thread);
  CheckWorkletCanExecuteScript(worklet.get());

  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worklet is
// created while another is terminating.
TEST_F(AnimationWorkletThreadTest, CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<AnimationWorkletThread> first_worklet =
      CreateAnimationWorkletThread();
  CheckWorkletCanExecuteScript(first_worklet.get());
  v8::Isolate* first_isolate = first_worklet->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Request termination of the first worklet and create the second worklet
  // as soon as possible.
  first_worklet->Terminate();
  // We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worklet thread so quickly. This could be a source of flakiness.

  std::unique_ptr<AnimationWorkletThread> second_worklet =
      CreateAnimationWorkletThread();

  v8::Isolate* second_isolate = second_worklet->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worklet.
  CheckWorkletCanExecuteScript(second_worklet.get());
  second_worklet->Terminate();
  second_worklet->WaitForShutdownForTesting();
}

}  // namespace blink
