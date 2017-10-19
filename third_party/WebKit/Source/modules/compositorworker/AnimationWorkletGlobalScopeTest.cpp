// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletGlobalScope.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptModule.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerReportingProxy.h"
#include "modules/compositorworker/AnimationWorklet.h"
#include "modules/compositorworker/AnimationWorkletThread.h"
#include "modules/compositorworker/Animator.h"
#include "modules/compositorworker/AnimatorDefinition.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/text/TextPosition.h"
#include "public/platform/WebURLRequest.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class AnimationWorkletGlobalScopeTest : public ::testing::Test {
 public:
  AnimationWorkletGlobalScopeTest() {}

  void SetUp() override {
    AnimationWorkletThread::CreateSharedBackingThreadForTest();
    reporting_proxy_ = WTF::MakeUnique<WorkerReportingProxy>();
    security_origin_ = SecurityOrigin::Create(KURL("http://fake.url/"));
  }

  void TearDown() override {
    AnimationWorkletThread::ClearSharedBackingThread();
  }

  std::unique_ptr<AnimationWorkletThread> CreateAnimationWorkletThread() {
    std::unique_ptr<AnimationWorkletThread> thread =
        AnimationWorkletThread::Create(nullptr, *reporting_proxy_);

    WorkerClients* clients = WorkerClients::Create();

    thread->Start(WTF::MakeUnique<GlobalScopeCreationParams>(
                      KURL("http://fake.url/"), "fake user agent", "", nullptr,
                      kDontPauseWorkerGlobalScopeOnStart, nullptr, "",
                      security_origin_.get(), clients, kWebAddressSpaceLocal,
                      nullptr, nullptr, kV8CacheOptionsDefault),
                  WTF::nullopt, ParentFrameTaskRunners::Create());
    return thread;
  }

  using TestCalback = void (AnimationWorkletGlobalScopeTest::*)(WorkerThread*,
                                                                WaitableEvent*);
  // Create a new animation worklet and run the callback task on it. Terminate
  // the worklet once the task completion is signaled.
  void RunTestOnWorkletThread(TestCalback callback) {
    std::unique_ptr<WorkerThread> worklet = CreateAnimationWorkletThread();
    WaitableEvent waitable_event;
    TaskRunnerHelper::Get(TaskType::kUnthrottled, worklet.get())
        ->PostTask(BLINK_FROM_HERE,
                   CrossThreadBind(callback, CrossThreadUnretained(this),
                                   CrossThreadUnretained(worklet.get()),
                                   CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();

    worklet->Terminate();
    worklet->WaitForShutdownForTesting();
  }

  void RunBasicParsingTestOnWorklet(WorkerThread* thread,
                                    WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = ToAnimationWorkletGlobalScope(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    {
      // registerAnimator() with a valid class definition should define an
      // animator.
      String source_code =
          R"JS(
            registerAnimator('test', class {
              constructor () {}
              animate () {}
            });
          )JS";
      ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

      AnimatorDefinition* definition =
          global_scope->FindDefinitionForTest("test");
      ASSERT_TRUE(definition);

      EXPECT_TRUE(definition->ConstructorLocal(isolate)->IsFunction());
      EXPECT_TRUE(definition->AnimateLocal(isolate)->IsFunction());
    }

    {
      // registerAnimator() with a null class definition should fail to define
      // an animator.
      String source_code = "registerAnimator('null', null);";
      ASSERT_FALSE(EvaluateScriptModule(global_scope, source_code));
      EXPECT_FALSE(global_scope->FindDefinitionForTest("null"));
    }

    EXPECT_FALSE(global_scope->FindDefinitionForTest("non-existent"));

    waitable_event->Signal();
  }

  void RunConstructAndAnimateTestOnWorklet(WorkerThread* thread,
                                           WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = ToAnimationWorkletGlobalScope(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
            // Worklet doesn't have a reference to the global object. Instead,
            // retrieve it in a tricky way.
            var global_object = Function('return this')();
            global_object.constructed = false;
            global_object.animated = false;

            registerAnimator('test', class {
              constructor () {
                constructed = true;
              }
              animate () {
                animated = true;
              }
            });
        )JS";
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

    ScriptValue constructed =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().constructed"));
    EXPECT_TRUE(ToBoolean(isolate, constructed.V8Value(), ASSERT_NO_EXCEPTION))
        << "constructor is not invoked";

    ScriptValue animated_before =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().animated"));
    EXPECT_FALSE(
        ToBoolean(isolate, animated_before.V8Value(), ASSERT_NO_EXCEPTION))
        << "animate function is invoked early";

    global_scope->Mutate();

    ScriptValue animated_after =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().animated"));
    EXPECT_TRUE(
        ToBoolean(isolate, animated_after.V8Value(), ASSERT_NO_EXCEPTION))
        << "animate function is not invoked";

    waitable_event->Signal();
  }

 private:
  // Returns false when a script evaluation error happens.
  bool EvaluateScriptModule(AnimationWorkletGlobalScope* global_scope,
                            const String& source_code) {
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), source_code, "worklet.js",
        kSharableCrossOrigin, WebURLRequest::kFetchCredentialsModeOmit,
        "" /* nonce */, kParserInserted, TextPosition::MinimumPosition(),
        ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());
    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptValue value =
        module.Evaluate(script_state, CaptureEvalErrorFlag::kCapture);
    return value.IsEmpty();
  }

  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AnimationWorkletGlobalScopeTest, BasicParsing) {
  RunTestOnWorkletThread(
      &AnimationWorkletGlobalScopeTest::RunBasicParsingTestOnWorklet);
}

TEST_F(AnimationWorkletGlobalScopeTest, ConstructAndAnimate) {
  RunTestOnWorkletThread(
      &AnimationWorkletGlobalScopeTest::RunConstructAndAnimateTestOnWorklet);
}

}  // namespace blink
