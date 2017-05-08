// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadStartupData.h"
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
      const WeakPtr<ThreadedWorkletMessagingProxy>& messaging_proxy_weak_ptr,
      ParentFrameTaskRunners* parent_frame_task_runners)
      : ThreadedWorkletObjectProxy(messaging_proxy_weak_ptr,
                                   parent_frame_task_runners),
        reported_features_(UseCounter::kNumberOfFeatures) {}

 protected:
  void CountFeature(UseCounter::Feature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(feature));
    reported_features_.QuickSet(feature);
    ThreadedWorkletObjectProxy::CountFeature(feature);
  }

  void CountDeprecation(UseCounter::Feature feature) final {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(feature));
    reported_features_.QuickSet(feature);
    ThreadedWorkletObjectProxy::CountDeprecation(feature);
  }

 private:
  BitVector reported_features_;
};

class ThreadedWorkletThreadForTest : public WorkerThread {
 public:
  ThreadedWorkletThreadForTest(
      WorkerLoaderProxyProvider* worker_loader_proxy_provider,
      WorkerReportingProxy& worker_reporting_proxy)
      : WorkerThread(WorkerLoaderProxy::Create(worker_loader_proxy_provider),
                     worker_reporting_proxy) {}
  ~ThreadedWorkletThreadForTest() override {}

  WorkerBackingThread& GetWorkerBackingThread() override {
    auto worklet_thread_holder =
        WorkletThreadHolder<ThreadedWorkletThreadForTest>::GetInstance();
    DCHECK(worklet_thread_holder);
    return *worklet_thread_holder->GetThread();
  }

  void ClearWorkerBackingThread() override {}

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData> startup_data) final {
    RefPtr<SecurityOrigin> security_origin =
        SecurityOrigin::Create(startup_data->script_url_);
    return new ThreadedWorkletGlobalScope(
        startup_data->script_url_, startup_data->user_agent_,
        security_origin.Release(), this->GetIsolate(), this);
  }

  bool IsOwningBackingThread() const final { return false; }

  static void EnsureSharedBackingThread() {
    DCHECK(IsMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::CreateForTest(
        "ThreadedWorkletThreadForTest");
  }

  static void ClearSharedBackingThread() {
    DCHECK(IsMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::ClearInstance();
  }

  // Emulates API use on ThreadedWorkletGlobalScope.
  void CountFeature(UseCounter::Feature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountFeature(feature);
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }

  // Emulates deprecated API use on ThreadedWorkletGlobalScope.
  void CountDeprecation(UseCounter::Feature feature) {
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
};

class ThreadedWorkletMessagingProxyForTest
    : public ThreadedWorkletMessagingProxy {
 public:
  ThreadedWorkletMessagingProxyForTest(ExecutionContext* execution_context)
      : ThreadedWorkletMessagingProxy(execution_context) {
    worklet_object_proxy_ = WTF::MakeUnique<ThreadedWorkletObjectProxyForTest>(
        weak_ptr_factory_.CreateWeakPtr(), GetParentFrameTaskRunners());
    worker_loader_proxy_provider_ =
        WTF::MakeUnique<WorkerLoaderProxyProvider>();
    worker_thread_ = WTF::MakeUnique<ThreadedWorkletThreadForTest>(
        worker_loader_proxy_provider_.get(), WorkletObjectProxy());
    ThreadedWorkletThreadForTest::EnsureSharedBackingThread();
  }

  ~ThreadedWorkletMessagingProxyForTest() override {
    worker_thread_->GetWorkerLoaderProxy()->DetachProvider(
        worker_loader_proxy_provider_.get());
    worker_thread_->TerminateAndWait();
    ThreadedWorkletThreadForTest::ClearSharedBackingThread();
  };

  void Start() {
    KURL script_url(kParsedURLString, "http://fake.url/");
    std::unique_ptr<Vector<char>> cached_meta_data = nullptr;
    Vector<CSPHeaderAndType> content_security_policy_headers;
    String referrer_policy = "";
    security_origin_ = SecurityOrigin::Create(script_url);
    WorkerClients* worker_clients = nullptr;
    Vector<String> origin_trial_tokens;
    std::unique_ptr<WorkerSettings> worker_settings = nullptr;
    worker_thread_->Start(
        WorkerThreadStartupData::Create(
            script_url, "fake user agent", "// fake source code",
            std::move(cached_meta_data), kDontPauseWorkerGlobalScopeOnStart,
            &content_security_policy_headers, referrer_policy,
            security_origin_.Get(), worker_clients, kWebAddressSpaceLocal,
            &origin_trial_tokens, std::move(worker_settings),
            WorkerV8Settings::Default()),
        GetParentFrameTaskRunners());
    GetWorkerInspectorProxy()->WorkerThreadCreated(
        ToDocument(GetExecutionContext()), worker_thread_.get(), script_url);
  }

 protected:
  std::unique_ptr<WorkerThread> CreateWorkerThread(double origin_time) final {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class ThreadedWorkletTest;

  std::unique_ptr<WorkerLoaderProxyProvider> worker_loader_proxy_provider_;
  RefPtr<SecurityOrigin> security_origin_;
};

class ThreadedWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    page_ = DummyPageHolder::Create();
    messaging_proxy_ = WTF::MakeUnique<ThreadedWorkletMessagingProxyForTest>(
        &page_->GetDocument());
  }

  ThreadedWorkletMessagingProxyForTest* MessagingProxy() {
    return messaging_proxy_.get();
  }

  ThreadedWorkletThreadForTest* GetWorkerThread() {
    return static_cast<ThreadedWorkletThreadForTest*>(
        messaging_proxy_->GetWorkerThread());
  }

  Document& GetDocument() { return page_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> page_;
  std::unique_ptr<ThreadedWorkletMessagingProxyForTest> messaging_proxy_;
};

TEST_F(ThreadedWorkletTest, UseCounter) {
  MessagingProxy()->Start();

  // This feature is randomly selected.
  const UseCounter::Feature kFeature1 = UseCounter::Feature::kRequestFileSystem;

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
  const UseCounter::Feature kFeature2 =
      UseCounter::Feature::kPrefixedStorageInfo;

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

}  // namespace blink
