// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class MockPaintWorkletProxyClient : public PaintWorkletProxyClient {
 public:
  MockPaintWorkletProxyClient()
      : PaintWorkletProxyClient(), did_set_global_scope_(false) {}
  void SetGlobalScope(WorkletGlobalScope*) override {
    did_set_global_scope_ = true;
  }
  bool did_set_global_scope() { return did_set_global_scope_; }

 private:
  bool did_set_global_scope_;
};

}  // namespace

// TODO(smcgruer): Extract a common base class between this and
// AnimationWorkletGlobalScope.
class PaintWorkletGlobalScopeTest : public PageTestBase {
 public:
  PaintWorkletGlobalScopeTest() = default;

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    Document* document = &GetDocument();
    document->SetURL(KURL("https://example.com/"));
    document->UpdateSecurityOrigin(SecurityOrigin::Create(document->Url()));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  std::unique_ptr<AnimationAndPaintWorkletThread>
  CreateAnimationAndPaintWorkletThread(PaintWorkletProxyClient* proxy_client) {
    std::unique_ptr<AnimationAndPaintWorkletThread> thread =
        AnimationAndPaintWorkletThread::CreateForPaintWorklet(
            *reporting_proxy_);

    WorkerClients* clients = WorkerClients::Create();
    if (proxy_client)
      ProvidePaintWorkletProxyClientTo(clients, proxy_client);

    Document* document = &GetDocument();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            document->Url(), mojom::ScriptType::kModule, document->UserAgent(),
            nullptr /* web_worker_fetch_context */, Vector<CSPHeaderAndType>(),
            document->GetReferrerPolicy(), document->GetSecurityOrigin(),
            document->IsSecureContext(), document->GetHttpsState(), clients,
            document->AddressSpace(),
            OriginTrialContext::GetTokens(document).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            kV8CacheOptionsDefault,
            MakeGarbageCollected<WorkletModuleResponsesMap>()),
        base::nullopt, std::make_unique<WorkerDevToolsParams>(),
        ParentExecutionContextTaskRunners::Create());
    return thread;
  }

  using TestCalback = void (PaintWorkletGlobalScopeTest::*)(WorkerThread*,
                                                            WaitableEvent*);

  // Create a new paint worklet and run the callback task on it. Terminate the
  // worklet once the task completion is signaled.
  void RunTestOnWorkletThread(TestCalback callback) {
    std::unique_ptr<WorkerThread> worklet =
        CreateAnimationAndPaintWorkletThread(nullptr);
    WaitableEvent waitable_event;
    PostCrossThreadTask(
        *worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(callback, CrossThreadUnretained(this),
                        CrossThreadUnretained(worklet.get()),
                        CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();

    worklet->Terminate();
    worklet->WaitForShutdownForTesting();
  }

  void RunScriptOnWorklet(String source_code,
                          WorkerThread* thread,
                          WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = To<PaintWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);
    ScriptState::Scope scope(script_state);
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));
    waitable_event->Signal();
  }

  void RunBasicParsingTestOnWorklet(WorkerThread* thread,
                                    WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = To<PaintWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    {
      // registerPaint() with a valid class definition should define an
      // animator.
      String source_code =
          R"JS(
            registerPaint('test', class {
              constructor () {}
              paint (ctx, size) {}
            });
          )JS";
      ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

      CSSPaintDefinition* definition = global_scope->FindDefinition("test");
      ASSERT_TRUE(definition);

      EXPECT_TRUE(definition->ConstructorForTesting(isolate)->IsFunction());
      EXPECT_TRUE(definition->PaintFunctionForTesting(isolate)->IsFunction());
    }

    {
      // registerPaint() with a null class definition should fail to define
      // an painter.
      String source_code = "registerPaint('null', null);";
      ASSERT_FALSE(EvaluateScriptModule(global_scope, source_code));
      EXPECT_FALSE(global_scope->FindDefinition("null"));
    }

    EXPECT_FALSE(global_scope->FindDefinition("non-existent"));

    waitable_event->Signal();
  }

 private:
  bool EvaluateScriptModule(PaintWorkletGlobalScope* global_scope,
                            const String& source_code) {
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    const KURL js_url("https://example.com/worklet.js");
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), source_code, js_url, js_url,
        ScriptFetchOptions(), TextPosition::MinimumPosition(),
        ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());
    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptValue value = module.Evaluate(script_state);
    return value.IsEmpty();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(PaintWorkletGlobalScopeTest, BasicParsing) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RunTestOnWorkletThread(
      &PaintWorkletGlobalScopeTest::RunBasicParsingTestOnWorklet);
}

}  // namespace blink
