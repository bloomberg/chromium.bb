// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/DedicatedWorkerMessagingProxy.h"
#include "core/workers/DedicatedWorkerObjectProxy.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThreadStartupData.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
 public:
  DedicatedWorkerThreadForTest(DedicatedWorkerObjectProxy& worker_object_proxy)
      : DedicatedWorkerThread(nullptr /* ThreadableLoadingContext */,
                              worker_object_proxy) {
    worker_backing_thread_ = WorkerBackingThread::CreateForTest("Test thread");
  }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    auto global_scope = new DedicatedWorkerGlobalScope(
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
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }

  // Emulates deprecated API use on DedicatedWorkerGlobalScope.
  void CountDeprecation(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountDeprecation(feature);

    // CountDeprecation() should add a warning message.
    EXPECT_EQ(1u, GetConsoleMessageStorage()->size());
    String console_message = GetConsoleMessageStorage()->at(0)->Message();
    EXPECT_TRUE(console_message.Contains("deprecated"));

    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }

  void TestTaskRunner() {
    EXPECT_TRUE(IsCurrentThread());
    RefPtr<WebTaskRunner> task_runner =
        TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GlobalScope());
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }
};

class DedicatedWorkerObjectProxyForTest final
    : public DedicatedWorkerObjectProxy {
 public:
  DedicatedWorkerObjectProxyForTest(
      DedicatedWorkerMessagingProxy* messaging_proxy,
      ParentFrameTaskRunners* parent_frame_task_runners)
      : DedicatedWorkerObjectProxy(messaging_proxy, parent_frame_task_runners),
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
                                      nullptr /* workerObject */,
                                      nullptr /* workerClients */) {
    worker_object_proxy_ = WTF::MakeUnique<DedicatedWorkerObjectProxyForTest>(
        this, GetParentFrameTaskRunners());
  }

  ~DedicatedWorkerMessagingProxyForTest() override = default;

  void StartWithSourceCode(const String& source) {
    KURL script_url(kParsedURLString, "http://fake.url/");
    security_origin_ = SecurityOrigin::Create(script_url);
    std::unique_ptr<Vector<CSPHeaderAndType>> headers =
        WTF::MakeUnique<Vector<CSPHeaderAndType>>();
    CSPHeaderAndType header_and_type("contentSecurityPolicy",
                                     kContentSecurityPolicyHeaderTypeReport);
    headers->push_back(header_and_type);
    auto worker_settings = std::make_unique<WorkerSettings>(
        ToDocument(GetExecutionContext())->GetSettings());
    InitializeWorkerThread(
        WTF::MakeUnique<GlobalScopeCreationParams>(
            script_url, "fake user agent", source,
            nullptr /* cached_meta_data */, kDontPauseWorkerGlobalScopeOnStart,
            headers.get(), "" /* referrer_policy */, security_origin_.get(),
            nullptr /* worker_clients */, kWebAddressSpaceLocal,
            nullptr /* origin_trial_tokens */, std::move(worker_settings),
            kV8CacheOptionsDefault),
        WorkerBackingThreadStartupData(
            WorkerBackingThreadStartupData::HeapLimitMode::kDefault,
            WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow),
        script_url);
  }

  DedicatedWorkerThreadForTest* GetDedicatedWorkerThread() {
    return static_cast<DedicatedWorkerThreadForTest*>(GetWorkerThread());
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(mock_worker_thread_lifecycle_observer_);
    DedicatedWorkerMessagingProxy::Trace(visitor);
  }

 private:
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    auto worker_thread =
        WTF::MakeUnique<DedicatedWorkerThreadForTest>(WorkerObjectProxy());
    mock_worker_thread_lifecycle_observer_ =
        new MockWorkerThreadLifecycleObserver(
            worker_thread->GetWorkerThreadLifecycleContext());
    EXPECT_CALL(*mock_worker_thread_lifecycle_observer_,
                ContextDestroyed(::testing::_))
        .Times(1);
    return std::move(worker_thread);
  }

  Member<MockWorkerThreadLifecycleObserver>
      mock_worker_thread_lifecycle_observer_;
  RefPtr<SecurityOrigin> security_origin_;
};

class DedicatedWorkerTest : public ::testing::Test {
 public:
  DedicatedWorkerTest() {}

  void SetUp() override {
    page_ = DummyPageHolder::Create();
    worker_messaging_proxy_ =
        new DedicatedWorkerMessagingProxyForTest(&page_->GetDocument());
  }

  void TearDown() override {
    GetWorkerThread()->Terminate();
    GetWorkerThread()->WaitForShutdownForTesting();
  }

  void DispatchMessageEvent() {
    WorkerMessagingProxy()->PostMessageToWorkerGlobalScope(
        nullptr /* message */, Vector<MessagePortChannel>());
  }

  DedicatedWorkerMessagingProxyForTest* WorkerMessagingProxy() {
    return worker_messaging_proxy_.Get();
  }

  DedicatedWorkerThreadForTest* GetWorkerThread() {
    return worker_messaging_proxy_->GetDedicatedWorkerThread();
  }

  Document& GetDocument() { return page_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> page_;
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
  const String source_code = "// Do nothing";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the DedicatedWorkerGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(), kFeature1));
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&DedicatedWorkerThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  testing::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountFeature.
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&DedicatedWorkerThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  testing::EnterRunLoop();

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

  // Deprecated API use on the DedicatedWorkerGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(), kFeature2));
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountDeprecation.
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
}

TEST_F(DedicatedWorkerTest, TaskRunner) {
  const String source_code = "// Do nothing";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&DedicatedWorkerThreadForTest::TestTaskRunner,
                                 CrossThreadUnretained(GetWorkerThread())));
  testing::EnterRunLoop();
}

}  // namespace blink
