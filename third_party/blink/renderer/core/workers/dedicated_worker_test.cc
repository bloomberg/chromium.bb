// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
 public:
  DedicatedWorkerThreadForTest(DedicatedWorkerObjectProxy& worker_object_proxy)
      : DedicatedWorkerThread(nullptr /* ThreadableLoadingContext */,
                              worker_object_proxy) {
    worker_backing_thread_ = WorkerBackingThread::Create(
        WebThreadCreationParams(WebThreadType::kTestThread));
  }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    auto* global_scope = new DedicatedWorkerGlobalScope(
        std::move(creation_params), this, time_origin_);
    // Initializing a global scope with a dummy creation params may emit warning
    // messages (e.g., invalid CSP directives). Clear them here for tests that
    // check console messages (i.e., UseCounter tests).
    GetConsoleMessageStorage()->Clear();
    return global_scope;
  }

  // Emulates API use on DedicatedWorkerGlobalScope.
  void CountFeature(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountFeature(feature);
    PostCrossThreadTask(
        *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalTest),
        FROM_HERE, CrossThreadBind(&test::ExitRunLoop));
  }

  // Emulates deprecated API use on DedicatedWorkerGlobalScope.
  void CountDeprecation(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountDeprecation(feature);

    // CountDeprecation() should add a warning message.
    EXPECT_EQ(1u, GetConsoleMessageStorage()->size());
    String console_message = GetConsoleMessageStorage()->at(0)->Message();
    EXPECT_TRUE(console_message.Contains("deprecated"));

    PostCrossThreadTask(
        *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalTest),
        FROM_HERE, CrossThreadBind(&test::ExitRunLoop));
  }

  void TestTaskRunner() {
    EXPECT_TRUE(IsCurrentThread());
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GlobalScope()->GetTaskRunner(TaskType::kInternalTest);
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    PostCrossThreadTask(
        *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalTest),
        FROM_HERE, CrossThreadBind(&test::ExitRunLoop));
  }
};

class DedicatedWorkerObjectProxyForTest final
    : public DedicatedWorkerObjectProxy {
 public:
  DedicatedWorkerObjectProxyForTest(
      DedicatedWorkerMessagingProxy* messaging_proxy,
      ParentExecutionContextTaskRunners* parent_execution_context_task_runners)
      : DedicatedWorkerObjectProxy(messaging_proxy,
                                   parent_execution_context_task_runners),
        reported_features_(static_cast<int>(WebFeature::kNumberOfFeatures)) {}

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    DedicatedWorkerObjectProxy::CountFeature(feature);
  }

  void CountDeprecation(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    DedicatedWorkerObjectProxy::CountDeprecation(feature);
  }

 private:
  BitVector reported_features_;
};

class DedicatedWorkerMessagingProxyForTest
    : public DedicatedWorkerMessagingProxy {
 public:
  DedicatedWorkerMessagingProxyForTest(ExecutionContext* execution_context)
      : DedicatedWorkerMessagingProxy(execution_context,
                                      nullptr /* worker_object */) {
    worker_object_proxy_ = std::make_unique<DedicatedWorkerObjectProxyForTest>(
        this, GetParentExecutionContextTaskRunners());
  }

  ~DedicatedWorkerMessagingProxyForTest() override = default;

  void StartWithSourceCode(const String& source) {
    KURL script_url("http://fake.url/");
    security_origin_ = SecurityOrigin::Create(script_url);
    Vector<CSPHeaderAndType> headers{
        {"contentSecurityPolicy", kContentSecurityPolicyHeaderTypeReport}};
    auto worker_settings = std::make_unique<WorkerSettings>(
        ToDocument(GetExecutionContext())->GetSettings());
    InitializeWorkerThread(
        std::make_unique<GlobalScopeCreationParams>(
            script_url, ScriptType::kClassic, "fake user agent", headers,
            kReferrerPolicyDefault, security_origin_.get(),
            false /* starter_secure_context */, nullptr /* worker_clients */,
            mojom::IPAddressSpace::kLocal, nullptr /* origin_trial_tokens */,
            base::UnguessableToken::Create(), std::move(worker_settings),
            kV8CacheOptionsDefault, nullptr /* worklet_module_responses_map */),
        WorkerBackingThreadStartupData(
            WorkerBackingThreadStartupData::HeapLimitMode::kDefault,
            WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow));
    GetWorkerThread()->EvaluateClassicScript(script_url, source,
                                             nullptr /* cached_meta_data */,
                                             v8_inspector::V8StackTraceId());
  }

  DedicatedWorkerThreadForTest* GetDedicatedWorkerThread() {
    return static_cast<DedicatedWorkerThreadForTest*>(GetWorkerThread());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(mock_worker_thread_lifecycle_observer_);
    DedicatedWorkerMessagingProxy::Trace(visitor);
  }

 private:
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    auto worker_thread =
        std::make_unique<DedicatedWorkerThreadForTest>(WorkerObjectProxy());
    mock_worker_thread_lifecycle_observer_ =
        new MockWorkerThreadLifecycleObserver(
            worker_thread->GetWorkerThreadLifecycleContext());
    EXPECT_CALL(*mock_worker_thread_lifecycle_observer_,
                ContextDestroyed(testing::_))
        .Times(1);
    return std::move(worker_thread);
  }

  Member<MockWorkerThreadLifecycleObserver>
      mock_worker_thread_lifecycle_observer_;
  scoped_refptr<const SecurityOrigin> security_origin_;
};

class DedicatedWorkerTest : public PageTestBase {
 public:
  DedicatedWorkerTest() = default;

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    worker_messaging_proxy_ =
        new DedicatedWorkerMessagingProxyForTest(&GetDocument());
  }

  void TearDown() override {
    GetWorkerThread()->Terminate();
    GetWorkerThread()->WaitForShutdownForTesting();
  }

  void DispatchMessageEvent() {
    WorkerMessagingProxy()->PostMessageToWorkerGlobalScope(
        nullptr /* message */, Vector<MessagePortChannel>(),
        v8_inspector::V8StackTraceId());
  }

  DedicatedWorkerMessagingProxyForTest* WorkerMessagingProxy() {
    return worker_messaging_proxy_.Get();
  }

  DedicatedWorkerThreadForTest* GetWorkerThread() {
    return worker_messaging_proxy_->GetDedicatedWorkerThread();
  }

 private:
  Persistent<DedicatedWorkerMessagingProxyForTest> worker_messaging_proxy_;
};

TEST_F(DedicatedWorkerTest, PendingActivity_NoActivityAfterContextDestroyed) {
  const String source_code = "// Do nothing";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // Destroying the context should result in no pending activities.
  WorkerMessagingProxy()->TerminateGlobalScope();
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, UseCounter) {
  Page::InsertOrdinaryPageForTesting(&GetPage());
  const String source_code = "// Do nothing";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the DedicatedWorkerGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(), kFeature1));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(&DedicatedWorkerThreadForTest::CountFeature,
                      CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountFeature.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(&DedicatedWorkerThreadForTest::CountFeature,
                      CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

  // Deprecated API use on the DedicatedWorkerGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(), kFeature2));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(&DedicatedWorkerThreadForTest::CountDeprecation,
                      CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountDeprecation.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(&DedicatedWorkerThreadForTest::CountDeprecation,
                      CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
}

TEST_F(DedicatedWorkerTest, TaskRunner) {
  const String source_code = "// Do nothing";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(&DedicatedWorkerThreadForTest::TestTaskRunner,
                      CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

}  // namespace blink
