// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/MessageEvent.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

// These are chosen by trial-and-error. Making these intervals smaller causes
// test flakiness. The main thread needs to wait until message confirmation and
// activity report separately. If the intervals are very short, they are
// notified to the main thread almost at the same time and the thread may miss
// the second notification.
const double kDefaultIntervalInSec = 0.01;
const double kNextIntervalInSec = 0.01;
const double kMaxIntervalInSec = 0.02;

}  // namespace

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
 public:
  DedicatedWorkerThreadForTest(
      WorkerLoaderProxyProvider* workerLoaderProxyProvider,
      InProcessWorkerObjectProxy& workerObjectProxy,
      BlinkGC::ThreadHeapMode threadHeapMode)
      : DedicatedWorkerThread(
            WorkerLoaderProxy::create(workerLoaderProxyProvider),
            workerObjectProxy,
            monotonicallyIncreasingTime()) {
    m_workerBackingThread =
        WorkerBackingThread::createForTest("Test thread", threadHeapMode);
  }

  WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData> startupData) override {
    return new DedicatedWorkerGlobalScope(
        startupData->m_scriptURL, startupData->m_userAgent, this, m_timeOrigin,
        std::move(startupData->m_starterOriginPrivilegeData),
        std::move(startupData->m_workerClients));
  }
};

class InProcessWorkerMessagingProxyForTest
    : public InProcessWorkerMessagingProxy {
 public:
  InProcessWorkerMessagingProxyForTest(ExecutionContext* executionContext,
                                       BlinkGC::ThreadHeapMode threadHeapMode)
      : InProcessWorkerMessagingProxy(executionContext,
                                      nullptr /* workerObject */,
                                      nullptr /* workerClients */) {
    workerObjectProxy().m_defaultIntervalInSec = kDefaultIntervalInSec;
    workerObjectProxy().m_nextIntervalInSec = kNextIntervalInSec;
    workerObjectProxy().m_maxIntervalInSec = kMaxIntervalInSec;

    m_mockWorkerLoaderProxyProvider =
        WTF::makeUnique<MockWorkerLoaderProxyProvider>();
    m_workerThread = WTF::wrapUnique(
        new DedicatedWorkerThreadForTest(m_mockWorkerLoaderProxyProvider.get(),
                                         workerObjectProxy(), threadHeapMode));
    workerThreadCreated();

    m_mockWorkerThreadLifecycleObserver = new MockWorkerThreadLifecycleObserver(
        m_workerThread->getWorkerThreadLifecycleContext());
    EXPECT_CALL(*m_mockWorkerThreadLifecycleObserver, contextDestroyed())
        .Times(1);
  }

  ~InProcessWorkerMessagingProxyForTest() override {
    EXPECT_FALSE(m_blocking);
    m_workerThread->workerLoaderProxy()->detachProvider(
        m_mockWorkerLoaderProxyProvider.get());
  }

  enum class Notification {
    MessageConfirmed,
    PendingActivityReported,
    ThreadTerminated,
  };

  // Blocks the main thread until some event is notified.
  Notification waitForNotification() {
    EXPECT_TRUE(isMainThread());
    DCHECK(!m_blocking);
    if (m_events.isEmpty()) {
      m_blocking = true;
      testing::enterRunLoop();
      DCHECK(!m_blocking);
    }
    return m_events.takeFirst();
  }

  void confirmMessageFromWorkerObject() override {
    EXPECT_TRUE(isMainThread());
    InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject();
    m_events.append(Notification::MessageConfirmed);
    if (m_blocking)
      testing::exitRunLoop();
    m_blocking = false;
  }

  void pendingActivityFinished() override {
    EXPECT_TRUE(isMainThread());
    InProcessWorkerMessagingProxy::pendingActivityFinished();
    m_events.append(Notification::PendingActivityReported);
    if (m_blocking)
      testing::exitRunLoop();
    m_blocking = false;
  }

  void workerThreadTerminated() override {
    EXPECT_TRUE(isMainThread());
    m_events.append(Notification::ThreadTerminated);
    if (m_blocking)
      testing::exitRunLoop();
    m_blocking = false;
  }

  std::unique_ptr<WorkerThread> createWorkerThread(double originTime) override {
    NOTREACHED();
    return nullptr;
  }

  DedicatedWorkerThreadForTest* workerThread() {
    return static_cast<DedicatedWorkerThreadForTest*>(m_workerThread.get());
  }

  unsigned unconfirmedMessageCount() const { return m_unconfirmedMessageCount; }

 private:
  std::unique_ptr<MockWorkerLoaderProxyProvider>
      m_mockWorkerLoaderProxyProvider;
  Persistent<MockWorkerThreadLifecycleObserver>
      m_mockWorkerThreadLifecycleObserver;

  WTF::Deque<Notification> m_events;
  bool m_blocking = false;
};

using Notification = InProcessWorkerMessagingProxyForTest::Notification;

class DedicatedWorkerTest
    : public ::testing::TestWithParam<BlinkGC::ThreadHeapMode> {
 public:
  DedicatedWorkerTest() : m_threadHeapMode(GetParam()) {}

  void SetUp() override {
    m_page = DummyPageHolder::create();
    m_workerMessagingProxy =
        WTF::wrapUnique(new InProcessWorkerMessagingProxyForTest(
            &m_page->document(), m_threadHeapMode));
    m_securityOrigin =
        SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
  }

  void TearDown() override {
    workerThread()->terminate();
    EXPECT_EQ(Notification::ThreadTerminated,
              workerMessagingProxy()->waitForNotification());
  }

  void startWithSourceCode(const String& source) {
    std::unique_ptr<Vector<CSPHeaderAndType>> headers =
        WTF::makeUnique<Vector<CSPHeaderAndType>>();
    CSPHeaderAndType headerAndType("contentSecurityPolicy",
                                   ContentSecurityPolicyHeaderTypeReport);
    headers->append(headerAndType);
    workerThread()->start(WorkerThreadStartupData::create(
        KURL(ParsedURLString, "http://fake.url/"), "fake user agent", source,
        nullptr /* cachedMetaData */, DontPauseWorkerGlobalScopeOnStart,
        headers.get(), "" /* referrerPolicy */, m_securityOrigin.get(),
        nullptr /* workerClients */, WebAddressSpaceLocal,
        nullptr /* originTrialTokens */, nullptr /* workerSettings */,
        V8CacheOptionsDefault));
  }

  void dispatchMessageEvent() {
    workerMessagingProxy()->postMessageToWorkerGlobalScope(
        nullptr /* message */, nullptr /* channels */);
  }

  InProcessWorkerMessagingProxyForTest* workerMessagingProxy() {
    return m_workerMessagingProxy.get();
  }

  DedicatedWorkerThreadForTest* workerThread() {
    return m_workerMessagingProxy->workerThread();
  }

 private:
  RefPtr<SecurityOrigin> m_securityOrigin;
  std::unique_ptr<DummyPageHolder> m_page;
  std::unique_ptr<InProcessWorkerMessagingProxyForTest> m_workerMessagingProxy;
  const BlinkGC::ThreadHeapMode m_threadHeapMode;
};

INSTANTIATE_TEST_CASE_P(MainThreadHeap,
                        DedicatedWorkerTest,
                        ::testing::Values(BlinkGC::MainThreadHeapMode));

INSTANTIATE_TEST_CASE_P(PerThreadHeap,
                        DedicatedWorkerTest,
                        ::testing::Values(BlinkGC::PerThreadHeapMode));

TEST_P(DedicatedWorkerTest, PendingActivity_NoActivity) {
  const String sourceCode = "// Do nothing";
  startWithSourceCode(sourceCode);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // There should be no pending activities after the initialization.
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

TEST_P(DedicatedWorkerTest, PendingActivity_SetTimeout) {
  // Start an oneshot timer on initial script evaluation.
  const String sourceCode = "setTimeout(function() {}, 0);";
  startWithSourceCode(sourceCode);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // The timer is fired soon and there should be no pending activities after
  // that.
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

TEST_P(DedicatedWorkerTest, PendingActivity_SetInterval) {
  // Start a repeated timer on initial script evaluation, and stop it when a
  // message is received. The timer needs a non-zero delay or else worker
  // activities would not run.
  const String sourceCode =
      "var id = setInterval(function() {}, 50);"
      "addEventListener('message', function(event) { clearInterval(id); });";
  startWithSourceCode(sourceCode);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // Stop the timer.
  dispatchMessageEvent();
  EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());
  EXPECT_EQ(Notification::MessageConfirmed,
            workerMessagingProxy()->waitForNotification());
  EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // There should be no pending activities after the timer is stopped.
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

TEST_P(DedicatedWorkerTest, PendingActivity_SetTimeoutOnMessageEvent) {
  // Start an oneshot timer on a message event.
  const String sourceCode =
      "addEventListener('message', function(event) {"
      "  setTimeout(function() {}, 0);"
      "});";
  startWithSourceCode(sourceCode);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());

  // A message starts the oneshot timer that is counted as a pending activity.
  dispatchMessageEvent();
  EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());
  EXPECT_EQ(Notification::MessageConfirmed,
            workerMessagingProxy()->waitForNotification());
  EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // The timer is fired soon and there should be no pending activities after
  // that.
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

TEST_P(DedicatedWorkerTest, PendingActivity_SetIntervalOnMessageEvent) {
  // Start a repeated timer on a message event, and stop it when another
  // message is received. The timer needs a non-zero delay or else worker
  // activities would not run.
  const String sourceCode =
      "var count = 0;"
      "var id;"
      "addEventListener('message', function(event) {"
      "  if (count++ == 0) {"
      "    id = setInterval(function() {}, 50);"
      "  } else {"
      "    clearInterval(id);"
      "  }"
      "});";
  startWithSourceCode(sourceCode);

  // Worker initialization should be counted as a pending activity.
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());

  // The first message event sets the active timer that is counted as a
  // pending activity.
  dispatchMessageEvent();
  EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());
  EXPECT_EQ(Notification::MessageConfirmed,
            workerMessagingProxy()->waitForNotification());
  EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // Run the message loop for a while to make sure the timer is counted as a
  // pending activity until it's stopped. The delay is equal to the max
  // interval so that the pending activity timer may be able to have a chance
  // to run before the next expectation check.
  const double kDelayInMs = kMaxIntervalInSec * 1000;
  testing::runDelayedTasks(kDelayInMs);
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());

  // Stop the timer.
  dispatchMessageEvent();
  EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
  EXPECT_TRUE(workerMessagingProxy()->hasPendingActivity());
  EXPECT_EQ(Notification::MessageConfirmed,
            workerMessagingProxy()->waitForNotification());
  EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());

  // There should be no pending activities after the timer is stopped.
  EXPECT_EQ(Notification::PendingActivityReported,
            workerMessagingProxy()->waitForNotification());
  EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

}  // namespace blink
