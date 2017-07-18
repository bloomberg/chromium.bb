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
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThreadStartupData.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// These are chosen by trial-and-error. Making these intervals smaller causes
// test flakiness. The main thread needs to wait until message confirmation and
// activity report separately. If the intervals are very short, they are
// notified to the main thread almost at the same time and the thread may miss
// the second notification.
constexpr double kDefaultIntervalInSec = 0.01;
constexpr double kNextIntervalInSec = 0.01;
constexpr double kMaxIntervalInSec = 0.02;

}  // namespace

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
 public:
  DedicatedWorkerThreadForTest(InProcessWorkerObjectProxy& worker_object_proxy)
      : DedicatedWorkerThread(nullptr /* ThreadableLoadingContext */,
                              worker_object_proxy) {
    worker_backing_thread_ = WorkerBackingThread::CreateForTest("Test thread");
  }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    return new DedicatedWorkerGlobalScope(
        creation_params->script_url, creation_params->user_agent, this,
        time_origin_, std::move(creation_params->starter_origin_privilege_data),
        std::move(creation_params->worker_clients));
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

    // countDeprecation() should add a warning message.
    EXPECT_EQ(1u, GetConsoleMessageStorage()->size());
    String console_message = GetConsoleMessageStorage()->at(0)->Message();
    EXPECT_TRUE(console_message.Contains("deprecated"));

    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE, CrossThreadBind(&testing::ExitRunLoop));
  }
};

class InProcessWorkerObjectProxyForTest final
    : public InProcessWorkerObjectProxy {
 public:
  InProcessWorkerObjectProxyForTest(
      InProcessWorkerMessagingProxy* messaging_proxy,
      ParentFrameTaskRunners* parent_frame_task_runners)
      : InProcessWorkerObjectProxy(messaging_proxy, parent_frame_task_runners),
        reported_features_(static_cast<int>(WebFeature::kNumberOfFeatures)) {
    default_interval_in_sec_ = kDefaultIntervalInSec;
    next_interval_in_sec_ = kNextIntervalInSec;
    max_interval_in_sec_ = kMaxIntervalInSec;
  }

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    InProcessWorkerObjectProxy::CountFeature(feature);
  }

  void CountDeprecation(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    InProcessWorkerObjectProxy::CountDeprecation(feature);
  }

 private:
  BitVector reported_features_;
};

class InProcessWorkerMessagingProxyForTest
    : public InProcessWorkerMessagingProxy {
 public:
  InProcessWorkerMessagingProxyForTest(ExecutionContext* execution_context)
      : InProcessWorkerMessagingProxy(execution_context,
                                      nullptr /* workerObject */,
                                      nullptr /* workerClients */) {
    worker_object_proxy_ = WTF::MakeUnique<InProcessWorkerObjectProxyForTest>(
        this, GetParentFrameTaskRunners());
  }

  ~InProcessWorkerMessagingProxyForTest() override {
    EXPECT_FALSE(blocking_);
  }

  void StartWithSourceCode(const String& source) {
    KURL script_url(kParsedURLString, "http://fake.url/");
    security_origin_ = SecurityOrigin::Create(script_url);
    std::unique_ptr<Vector<CSPHeaderAndType>> headers =
        WTF::MakeUnique<Vector<CSPHeaderAndType>>();
    CSPHeaderAndType header_and_type("contentSecurityPolicy",
                                     kContentSecurityPolicyHeaderTypeReport);
    headers->push_back(header_and_type);
    InitializeWorkerThread(
        WTF::MakeUnique<GlobalScopeCreationParams>(
            script_url, "fake user agent", source, nullptr /* cachedMetaData */,
            kDontPauseWorkerGlobalScopeOnStart, headers.get(),
            "" /* referrerPolicy */, security_origin_.Get(),
            nullptr /* workerClients */, kWebAddressSpaceLocal,
            nullptr /* originTrialTokens */, nullptr /* workerSettings */,
            kV8CacheOptionsDefault),
        CreateBackingThreadStartupData(nullptr /* isolate */), script_url);
  }

  enum class Notification {
    kMessageConfirmed,
    kPendingActivityReported,
    kThreadTerminated,
  };

  // Blocks the main thread until some event is notified.
  Notification WaitForNotification() {
    EXPECT_TRUE(IsMainThread());
    DCHECK(!blocking_);
    if (events_.IsEmpty()) {
      blocking_ = true;
      testing::EnterRunLoop();
      DCHECK(!blocking_);
    }
    return events_.TakeFirst();
  }

  void ConfirmMessageFromWorkerObject() override {
    EXPECT_TRUE(IsMainThread());
    InProcessWorkerMessagingProxy::ConfirmMessageFromWorkerObject();
    events_.push_back(Notification::kMessageConfirmed);
    if (blocking_)
      testing::ExitRunLoop();
    blocking_ = false;
  }

  void PendingActivityFinished() override {
    EXPECT_TRUE(IsMainThread());
    InProcessWorkerMessagingProxy::PendingActivityFinished();
    events_.push_back(Notification::kPendingActivityReported);
    if (blocking_)
      testing::ExitRunLoop();
    blocking_ = false;
  }

  void WorkerThreadTerminated() override {
    EXPECT_TRUE(IsMainThread());
    ThreadedMessagingProxyBase::WorkerThreadTerminated();
    events_.push_back(Notification::kThreadTerminated);
    if (blocking_)
      testing::ExitRunLoop();
    blocking_ = false;
  }

  DedicatedWorkerThreadForTest* GetDedicatedWorkerThread() {
    return static_cast<DedicatedWorkerThreadForTest*>(GetWorkerThread());
  }

  unsigned UnconfirmedMessageCount() const {
    return unconfirmed_message_count_;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(mock_worker_thread_lifecycle_observer_);
    InProcessWorkerMessagingProxy::Trace(visitor);
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

  WTF::Optional<WorkerBackingThreadStartupData> CreateBackingThreadStartupData(
      v8::Isolate*) override {
    return WorkerBackingThreadStartupData(
        WorkerBackingThreadStartupData::HeapLimitMode::kDefault,
        WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow);
  }

  Member<MockWorkerThreadLifecycleObserver>
      mock_worker_thread_lifecycle_observer_;
  RefPtr<SecurityOrigin> security_origin_;

  WTF::Deque<Notification> events_;
  bool blocking_ = false;
};

using Notification = InProcessWorkerMessagingProxyForTest::Notification;

class DedicatedWorkerTest : public ::testing::Test {
 public:
  DedicatedWorkerTest() {}

  void SetUp() override {
    page_ = DummyPageHolder::Create();
    worker_messaging_proxy_ =
        new InProcessWorkerMessagingProxyForTest(&page_->GetDocument());
  }

  void TearDown() override {
    GetWorkerThread()->Terminate();
    EXPECT_EQ(Notification::kThreadTerminated,
              WorkerMessagingProxy()->WaitForNotification());
  }

  void DispatchMessageEvent() {
    WorkerMessagingProxy()->PostMessageToWorkerGlobalScope(
        nullptr /* message */, MessagePortChannelArray());
  }

  InProcessWorkerMessagingProxyForTest* WorkerMessagingProxy() {
    return worker_messaging_proxy_.Get();
  }

  DedicatedWorkerThreadForTest* GetWorkerThread() {
    return worker_messaging_proxy_->GetDedicatedWorkerThread();
  }

  Document& GetDocument() { return page_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<InProcessWorkerMessagingProxyForTest> worker_messaging_proxy_;
};

TEST_F(DedicatedWorkerTest, PendingActivity_NoActivity) {
  const String source_code = "// Do nothing";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // There should be no pending activities after the initialization.
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetTimeout) {
  // Start an oneshot timer on initial script evaluation.
  const String source_code = "setTimeout(function() {}, 0);";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // The timer is fired soon and there should be no pending activities after
  // that.
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetInterval) {
  // Start a repeated timer on initial script evaluation, and stop it when a
  // message is received. The timer needs a non-zero delay or else worker
  // activities would not run.
  const String source_code =
      "var id = setInterval(function() {}, 50);"
      "addEventListener('message', function(event) { clearInterval(id); });";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // Stop the timer.
  DispatchMessageEvent();
  EXPECT_EQ(1u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());
  EXPECT_EQ(Notification::kMessageConfirmed,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_EQ(0u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // There should be no pending activities after the timer is stopped.
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetTimeoutOnMessageEvent) {
  // Start an oneshot timer on a message event.
  const String source_code =
      "addEventListener('message', function(event) {"
      "  setTimeout(function() {}, 0);"
      "});";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());

  // A message starts the oneshot timer that is counted as a pending activity.
  DispatchMessageEvent();
  EXPECT_EQ(1u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());
  EXPECT_EQ(Notification::kMessageConfirmed,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_EQ(0u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // The timer is fired soon and there should be no pending activities after
  // that.
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetIntervalOnMessageEvent) {
  // Start a repeated timer on a message event, and stop it when another
  // message is received. The timer needs a non-zero delay or else worker
  // activities would not run.
  const String source_code =
      "var count = 0;"
      "var id;"
      "addEventListener('message', function(event) {"
      "  if (count++ == 0) {"
      "    id = setInterval(function() {}, 50);"
      "  } else {"
      "    clearInterval(id);"
      "  }"
      "});";
  WorkerMessagingProxy()->StartWithSourceCode(source_code);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());

  // The first message event sets the active timer that is counted as a
  // pending activity.
  DispatchMessageEvent();
  EXPECT_EQ(1u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());
  EXPECT_EQ(Notification::kMessageConfirmed,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_EQ(0u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // Run the message loop for a while to make sure the timer is counted as a
  // pending activity until it's stopped. The delay is equal to the max
  // interval so that the pending activity timer may be able to have a chance
  // to run before the next expectation check.
  constexpr TimeDelta kDelay = TimeDelta::FromSecondsD(kMaxIntervalInSec);
  testing::RunDelayedTasks(kDelay);
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // Stop the timer.
  DispatchMessageEvent();
  EXPECT_EQ(1u, WorkerMessagingProxy()->UnconfirmedMessageCount());
  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());
  EXPECT_EQ(Notification::kMessageConfirmed,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_EQ(0u, WorkerMessagingProxy()->UnconfirmedMessageCount());

  // There should be no pending activities after the timer is stopped.
  EXPECT_EQ(Notification::kPendingActivityReported,
            WorkerMessagingProxy()->WaitForNotification());
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

// Test is flaky. crbug.com/699712
TEST_F(DedicatedWorkerTest, DISABLED_UseCounter) {
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
  // InProcessWorkerObjectProxyForTest::CountFeature.
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
  // InProcessWorkerObjectProxyForTest::CountDeprecation.
  TaskRunnerHelper::Get(TaskType::kUnspecedTimer, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  testing::EnterRunLoop();
}

}  // namespace blink
