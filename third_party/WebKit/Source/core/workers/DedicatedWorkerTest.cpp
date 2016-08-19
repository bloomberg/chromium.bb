// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CrossThreadTask.h"
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

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
public:
    DedicatedWorkerThreadForTest(
        WorkerLoaderProxyProvider* workerLoaderProxyProvider,
        InProcessWorkerObjectProxy& workerObjectProxy)
        : DedicatedWorkerThread(WorkerLoaderProxy::create(workerLoaderProxyProvider), workerObjectProxy, monotonicallyIncreasingTime())
    {
        m_workerBackingThread = WorkerBackingThread::createForTest("Test thread");
    }

    WorkerOrWorkletGlobalScope* createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData> startupData) override
    {
        return new DedicatedWorkerGlobalScope(startupData->m_scriptURL, startupData->m_userAgent, this, m_timeOrigin, std::move(startupData->m_starterOriginPrivilegeData), std::move(startupData->m_workerClients));
    }
};

class InProcessWorkerMessagingProxyForTest : public InProcessWorkerMessagingProxy {
public:
    InProcessWorkerMessagingProxyForTest(ExecutionContext* executionContext)
        : InProcessWorkerMessagingProxy(executionContext, nullptr /* workerObject */ , nullptr /* workerClients */)
    {
        workerObjectProxy().m_nextIntervalInSec = 0.1;
        workerObjectProxy().m_maxIntervalInSec = 0.2;

        m_mockWorkerLoaderProxyProvider = wrapUnique(new MockWorkerLoaderProxyProvider());
        m_workerThread = wrapUnique(new DedicatedWorkerThreadForTest(m_mockWorkerLoaderProxyProvider.get(), workerObjectProxy()));
        workerThreadCreated();

        m_mockWorkerThreadLifecycleObserver = new MockWorkerThreadLifecycleObserver(m_workerThread->getWorkerThreadLifecycleContext());
        EXPECT_CALL(*m_mockWorkerThreadLifecycleObserver, contextDestroyed()).Times(1);
    }

    ~InProcessWorkerMessagingProxyForTest() override
    {
        EXPECT_EQ(WaitUntilMode::DontWait, m_waitUntilMode);
        m_workerThread->workerLoaderProxy()->detachProvider(m_mockWorkerLoaderProxyProvider.get());
    }

    enum class WaitUntilMode {
        DontWait,
        MessageConfirmed,
        PendingActivityReported,
        ThreadTerminated,
    };

    // Blocks the main thread until a specified event happens.
    void waitUntil(WaitUntilMode mode)
    {
        EXPECT_TRUE(isMainThread());
        EXPECT_EQ(WaitUntilMode::DontWait, m_waitUntilMode);
        m_waitUntilMode = mode;
        testing::enterRunLoop();
    }

    void confirmMessageFromWorkerObject() override
    {
        EXPECT_TRUE(isMainThread());
        InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject();
        if (m_waitUntilMode != WaitUntilMode::MessageConfirmed)
            return;
        m_waitUntilMode = WaitUntilMode::DontWait;
        testing::exitRunLoop();
    }

    void pendingActivityFinished() override
    {
        EXPECT_TRUE(isMainThread());
        InProcessWorkerMessagingProxy::pendingActivityFinished();
        if (m_waitUntilMode != WaitUntilMode::PendingActivityReported)
            return;
        m_waitUntilMode = WaitUntilMode::DontWait;
        testing::exitRunLoop();
    }

    void workerThreadTerminated() override
    {
        EXPECT_TRUE(isMainThread());
        if (m_waitUntilMode != WaitUntilMode::ThreadTerminated)
            return;
        m_waitUntilMode = WaitUntilMode::DontWait;
        testing::exitRunLoop();
    }

    std::unique_ptr<WorkerThread> createWorkerThread(double originTime) override
    {
        NOTREACHED();
        return nullptr;
    }

    DedicatedWorkerThreadForTest* workerThread()
    {
        return static_cast<DedicatedWorkerThreadForTest*>(m_workerThread.get());
    }

    bool workerGlobalScopeMayHavePendingActivity() const { return m_workerGlobalScopeMayHavePendingActivity; }
    unsigned unconfirmedMessageCount() const { return m_unconfirmedMessageCount; }

private:
    std::unique_ptr<MockWorkerLoaderProxyProvider> m_mockWorkerLoaderProxyProvider;
    Persistent<MockWorkerThreadLifecycleObserver> m_mockWorkerThreadLifecycleObserver;

    WaitUntilMode m_waitUntilMode = WaitUntilMode::DontWait;
};

using WaitUntilMode = InProcessWorkerMessagingProxyForTest::WaitUntilMode;

class DedicatedWorkerTest : public ::testing::Test {
public:
    void SetUp() override
    {
        m_page = DummyPageHolder::create();
        m_workerMessagingProxy = wrapUnique(new InProcessWorkerMessagingProxyForTest(&m_page->document()));
        m_securityOrigin = SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
    }

    void TearDown() override
    {
        workerThread()->terminate();
        workerMessagingProxy()->waitUntil(WaitUntilMode::ThreadTerminated);
    }

    void startWithSourceCode(const String& source)
    {
        std::unique_ptr<Vector<CSPHeaderAndType>> headers = wrapUnique(new Vector<CSPHeaderAndType>());
        CSPHeaderAndType headerAndType("contentSecurityPolicy", ContentSecurityPolicyHeaderTypeReport);
        headers->append(headerAndType);
        workerThread()->start(WorkerThreadStartupData::create(
            KURL(ParsedURLString, "http://fake.url/"),
            "fake user agent",
            source,
            nullptr /* cachedMetaData */,
            DontPauseWorkerGlobalScopeOnStart,
            headers.get(),
            "" /* referrerPolicy */,
            m_securityOrigin.get(),
            nullptr /* workerClients */,
            WebAddressSpaceLocal,
            nullptr /* originTrialTokens */,
            nullptr /* workerSettings */,
            V8CacheOptionsDefault));
    }

    void dispatchMessageEvent()
    {
        workerMessagingProxy()->postMessageToWorkerGlobalScope(nullptr /* message */, nullptr /* channels */);
    }

    InProcessWorkerMessagingProxyForTest* workerMessagingProxy()
    {
        return m_workerMessagingProxy.get();
    }

    DedicatedWorkerThreadForTest* workerThread()
    {
        return m_workerMessagingProxy->workerThread();
    }

private:
    RefPtr<SecurityOrigin> m_securityOrigin;
    std::unique_ptr<DummyPageHolder> m_page;
    std::unique_ptr<InProcessWorkerMessagingProxyForTest> m_workerMessagingProxy;
};

TEST_F(DedicatedWorkerTest, PendingActivity_NoActivity)
{
    const String sourceCode = "// Do nothing";
    startWithSourceCode(sourceCode);

    // Worker initialization should be counted as a pending activity.
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // There should be no pending activities after the initialization.
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetTimeout)
{
    // Start an oneshot timer on initial script evaluation.
    const String sourceCode = "setTimeout(function() {}, 50);";
    startWithSourceCode(sourceCode);

    // Worker initialization should be counted as a pending activity.
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // The timer is fired soon and there should be no pending activities after
    // that.
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->hasPendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetInterval)
{
    // Start a repeated timer on initial script evaluation, and stop it when a
    // message is received.
    const String sourceCode =
        "var id = setInterval(function() {}, 50);"
        "addEventListener('message', function(event) { clearInterval(id); });";
    startWithSourceCode(sourceCode);

    // Worker initialization should be counted as a pending activity.
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // Stop the timer.
    dispatchMessageEvent();
    EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
    workerMessagingProxy()->waitUntil(WaitUntilMode::MessageConfirmed);
    EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // There should be no pending activities after the timer is stopped.
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetTimeoutOnMessageEvent)
{
    // Start an oneshot timer on a message event.
    const String sourceCode =
        "addEventListener('message', function(event) {"
        "  setTimeout(function() {}, 50);"
        "});";
    startWithSourceCode(sourceCode);

    // Worker initialization should be counted as a pending activity.
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // A message starts the oneshot timer that is counted as a pending activity.
    dispatchMessageEvent();
    EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
    workerMessagingProxy()->waitUntil(WaitUntilMode::MessageConfirmed);
    EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // The timer is fired soon and there should be no pending activities after
    // that.
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
}

TEST_F(DedicatedWorkerTest, PendingActivity_SetIntervalOnMessageEvent)
{
    // Start a repeated timer on a message event, and stop it when another
    // message is received.
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
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // The first message event sets the active timer that is counted as a
    // pending activity.
    dispatchMessageEvent();
    EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
    workerMessagingProxy()->waitUntil(WaitUntilMode::MessageConfirmed);
    EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // Run the message loop for a while to make sure the timer is counted as a
    // pending activity until it's stopped.
    testing::runDelayedTasks(1000);
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // Stop the timer.
    dispatchMessageEvent();
    EXPECT_EQ(1u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
    workerMessagingProxy()->waitUntil(WaitUntilMode::MessageConfirmed);
    EXPECT_EQ(0u, workerMessagingProxy()->unconfirmedMessageCount());
    EXPECT_TRUE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());

    // There should be no pending activities after the timer is stopped.
    workerMessagingProxy()->waitUntil(WaitUntilMode::PendingActivityReported);
    EXPECT_FALSE(workerMessagingProxy()->workerGlobalScopeMayHavePendingActivity());
}

} // namespace blink
