// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8CacheOptions.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/ThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "core/workers/WorkletThreadHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/TaskType.h"
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
    auto* worklet_thread_holder =
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
    scoped_refptr<WebTaskRunner> task_runner =
        GlobalScope()->GetTaskRunner(TaskType::kUnspecedTimer);
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }

 private:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) final {
    return new ThreadedWorkletGlobalScope(std::move(creation_params),
                                          GetIsolate(), this);
  }

  bool IsOwningBackingThread() const final { return false; }
};

class ThreadedWorkletMessagingProxyForTest
    : public ThreadedWorkletMessagingProxy {
 public:
  ThreadedWorkletMessagingProxyForTest(ExecutionContext* execution_context,
                                       WorkerClients* worker_clients)
      : ThreadedWorkletMessagingProxy(execution_context, worker_clients) {
    worklet_object_proxy_ = std::make_unique<ThreadedWorkletObjectProxyForTest>(
        this, GetParentFrameTaskRunners());
  }

  ~ThreadedWorkletMessagingProxyForTest() override {}

  void Start() {
    Document* document = ToDocument(GetExecutionContext());
    std::unique_ptr<Vector<char>> cached_meta_data = nullptr;
    Vector<CSPHeaderAndType> content_security_policy_headers;
    WorkerClients* worker_clients = nullptr;
    std::unique_ptr<WorkerSettings> worker_settings = nullptr;
    InitializeWorkerThread(
        std::make_unique<GlobalScopeCreationParams>(
            document->Url(), document->UserAgent(), "" /* source_code */,
            std::move(cached_meta_data), &content_security_policy_headers,
            document->GetReferrerPolicy(), document->GetSecurityOrigin(),
            worker_clients, document->AddressSpace(),
            OriginTrialContext::GetTokens(document).get(),
            std::move(worker_settings), kV8CacheOptionsDefault),
        WTF::nullopt, document->Url(), v8_inspector::V8StackTraceId());
  }

 private:
  friend class ThreadedWorkletTest;

  std::unique_ptr<WorkerThread> CreateWorkerThread() final {
    return std::make_unique<ThreadedWorkletThreadForTest>(WorkletObjectProxy());
  }
};

class ThreadedWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    page_ = DummyPageHolder::Create();
    Document* document = page_->GetFrame().GetDocument();
    document->SetURL(KURL("https://example.com/"));
    document->UpdateSecurityOrigin(SecurityOrigin::Create(document->Url()));
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

  GetWorkerThread()
      ->GetTaskRunner(TaskType::kUnspecedTimer)
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
  GetWorkerThread()
      ->GetTaskRunner(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  testing::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletGlobalScopeForTest::CountFeature.
  GetWorkerThread()
      ->GetTaskRunner(TaskType::kUnspecedTimer)
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
  GetWorkerThread()
      ->GetTaskRunner(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(), kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletGlobalScopeForTest::CountDeprecation.
  GetWorkerThread()
      ->GetTaskRunner(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, TaskRunner) {
  MessagingProxy()->Start();

  GetWorkerThread()
      ->GetTaskRunner(TaskType::kUnspecedTimer)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&ThreadedWorkletThreadForTest::TestTaskRunner,
                                 CrossThreadUnretained(GetWorkerThread())));
  testing::EnterRunLoop();
}

}  // namespace blink
