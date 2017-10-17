// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "core/workers/WorkletThreadHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ThreadedWorkletObjectProxyForTest final
    : public ThreadedWorkletObjectProxy {
 public:
  ThreadedWorkletObjectProxyForTest(
      ThreadedWorkletMessagingProxy* messaging_proxy,
      ParentFrameTaskRunners* parent_frame_task_runners)
      : ThreadedWorkletObjectProxy(messaging_proxy, parent_frame_task_runners),
        reported_features_(static_cast<int>(WebFeature::kNumberOfFeatures)) {}

 protected:
  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    ThreadedWorkletObjectProxy::CountFeature(feature);
  }

  void CountDeprecation(WebFeature feature) final {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    ThreadedWorkletObjectProxy::CountDeprecation(feature);
  }

 private:
  BitVector reported_features_;
};

class ThreadedWorkletThreadForTest : public WorkerThread {
 public:
  explicit ThreadedWorkletThreadForTest(
      WorkerReportingProxy& worker_reporting_proxy)
      : WorkerThread(nullptr, worker_reporting_proxy) {}
  ~ThreadedWorkletThreadForTest() override {}

  WorkerBackingThread& GetWorkerBackingThread() override {
    auto worklet_thread_holder =
        WorkletThreadHolder<ThreadedWorkletThreadForTest>::GetInstance();
    DCHECK(worklet_thread_holder);
    return *worklet_thread_holder->GetThread();
  }

  void ClearWorkerBackingThread() override {}

  static void EnsureSharedBackingThread() {
    DCHECK(IsMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::CreateForTest(
        "ThreadedWorkletThreadForTest");
  }

  static void ClearSharedBackingThread() {
    DCHECK(IsMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::ClearInstance();
  }

  void TestSecurityOrigin() {
    WorkletGlobalScope* global_scope = ToWorkletGlobalScope(GlobalScope());
    // The SecurityOrigin for a worklet should be a unique opaque origin, while
    // the owner Document's SecurityOrigin shouldn't.
    EXPECT_TRUE(global_scope->GetSecurityOrigin()->IsUnique());
    EXPECT_FALSE(global_scope->DocumentSecurityOrigin()->IsUnique());
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }

  // Emulates API use on ThreadedWorkletGlobalScope.
  void CountFeature(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountFeature(feature);
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }

  // Emulates deprecated API use on ThreadedWorkletGlobalScope.
  void CountDeprecation(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountDeprecation(feature);

    // countDeprecation() should add a warning message.
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

 private:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) final {
    RefPtr<SecurityOrigin> security_origin =
        SecurityOrigin::Create(creation_params->script_url);
    return new ThreadedWorkletGlobalScope(
        creation_params->script_url, creation_params->user_agent,
        std::move(security_origin), this->GetIsolate(), this,
        creation_params->worker_clients);
  }

  bool IsOwningBackingThread() const final { return false; }
};

class ThreadedWorkletMessagingProxyForTest
    : public ThreadedWorkletMessagingProxy {
 public:
  ThreadedWorkletMessagingProxyForTest(ExecutionContext* execution_context,
                                       WorkerClients* worker_clients)
      : ThreadedWorkletMessagingProxy(execution_context, worker_clients) {
    worklet_object_proxy_ = WTF::MakeUnique<ThreadedWorkletObjectProxyForTest>(
        this, GetParentFrameTaskRunners());
  }

  ~ThreadedWorkletMessagingProxyForTest() override {}

  void Start() {
    KURL script_url(kParsedURLString, "http://fake.url/");
    std::unique_ptr<Vector<char>> cached_meta_data = nullptr;
    Vector<CSPHeaderAndType> content_security_policy_headers;
    String referrer_policy = "";
    security_origin_ = SecurityOrigin::Create(script_url);
    WorkerClients* worker_clients = nullptr;
    Vector<String> origin_trial_tokens;
    std::unique_ptr<WorkerSettings> worker_settings = nullptr;
    InitializeWorkerThread(
        WTF::MakeUnique<GlobalScopeCreationParams>(
            script_url, "fake user agent", "// fake source code",
            std::move(cached_meta_data), kDontPauseWorkerGlobalScopeOnStart,
            &content_security_policy_headers, referrer_policy,
            security_origin_.get(), worker_clients, kWebAddressSpaceLocal,
            &origin_trial_tokens, std::move(worker_settings),
            kV8CacheOptionsDefault),
        WTF::nullopt, script_url);
  }

 private:
  friend class ThreadedWorkletTest;

  std::unique_ptr<WorkerThread> CreateWorkerThread() final {
    return WTF::MakeUnique<ThreadedWorkletThreadForTest>(WorkletObjectProxy());
  }

  RefPtr<SecurityOrigin> security_origin_;
};

class ThreadedWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    page_ = DummyPageHolder::Create();
    messaging_proxy_ = new ThreadedWorkletMessagingProxyForTest(
        &page_->GetDocument(), WorkerClients::Create());
    ThreadedWorkletThreadForTest::EnsureSharedBackingThread();
  }

  void TearDown() override {
    GetWorkerThread()->Terminate();
    GetWorkerThread()->WaitForShutdownForTesting();
    testing::RunPendingTasks();
    ThreadedWorkletThreadForTest::ClearSharedBackingThread();
    messaging_proxy_ = nullptr;
  }

  ThreadedWorkletMessagingProxyForTest* MessagingProxy() {
    return messaging_proxy_.Get();
  }

  ThreadedWorkletThreadForTest* GetWorkerThread() {
    return static_cast<ThreadedWorkletThreadForTest*>(
        messaging_proxy_->GetWorkerThread());
  }

  Document& GetDocument() { return page_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<ThreadedWorkletMessagingProxyForTest> messaging_proxy_;
};

TEST_F(ThreadedWorkletTest, SecurityOrigin) {
  MessagingProxy()->Start();

  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::TestSecurityOrigin,
                          CrossThreadUnretained(GetWorkerThread())));
  testing::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, UseCounter) {
  MessagingProxy()->Start();

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the ThreadedWorkletGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(), kFeature1));
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  testing::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletGlobalScopeForTest::CountFeature.
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  testing::EnterRunLoop();

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

  // Deprecated API use on the ThreadedWorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(), kFeature2));
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletGlobalScopeForTest::CountDeprecation.
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, TaskRunner) {
  MessagingProxy()->Start();

  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&ThreadedWorkletThreadForTest::TestTaskRunner,
                                 CrossThreadUnretained(GetWorkerThread())));
  testing::EnterRunLoop();
}

}  // namespace blink
