// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThread.h"

#include "core/workers/WorkerThreadTestHelper.h"
#include "public/platform/WebScheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;
using testing::Invoke;
using testing::Return;
using testing::Mock;

namespace blink {

class WorkerThreadTest : public testing::Test {
public:
    void SetUp() override
    {
        m_mockWorkerLoaderProxyProvider = adoptPtr(new MockWorkerLoaderProxyProvider());
        m_mockWorkerReportingProxy = adoptPtr(new MockWorkerReportingProxy());
        m_securityOrigin = SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
        m_workerThread = adoptPtr(new WorkerThreadForTest(
            m_mockWorkerLoaderProxyProvider.get(),
            *m_mockWorkerReportingProxy));
    }

    void TearDown() override
    {
        m_workerThread->workerLoaderProxy()->detachProvider(m_mockWorkerLoaderProxyProvider.get());
    }

    void start()
    {
        startWithSourceCode("//fake source code");
    }

    void startWithSourceCode(const String& source)
    {
        m_workerThread->startWithSourceCode(m_securityOrigin.get(), source);
    }

    void waitForInit()
    {
        m_workerThread->waitForInit();
    }

protected:
    void expectWorkerLifetimeReportingCalls()
    {
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(true)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated()).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope()).Times(1);
    }

    RefPtr<SecurityOrigin> m_securityOrigin;
    OwnPtr<MockWorkerLoaderProxyProvider> m_mockWorkerLoaderProxyProvider;
    OwnPtr<MockWorkerReportingProxy> m_mockWorkerReportingProxy;
    OwnPtr<WorkerThreadForTest> m_workerThread;
};

TEST_F(WorkerThreadTest, StartAndStop)
{
    expectWorkerLifetimeReportingCalls();
    start();
    waitForInit();
    m_workerThread->terminateAndWait();
}

TEST_F(WorkerThreadTest, StartAndStopImmediately)
{
    EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_))
        .Times(AtMost(1));
    EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(_))
        .Times(AtMost(1));
    EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated())
        .Times(AtMost(1));
    EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope())
        .Times(AtMost(1));
    start();
    m_workerThread->terminateAndWait();
}

TEST_F(WorkerThreadTest, StartAndStopOnScriptLoaded)
{
    // Use a JavaScript source code that makes an infinite loop so that we can
    // catch some kind of issues as a timeout.
    const String source("while(true) {}");

    EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_))
        .Times(AtMost(1));
    EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(_))
        .Times(AtMost(1));
    EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated())
        .Times(AtMost(1));
    EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope())
        .Times(AtMost(1));
    startWithSourceCode(source);
    m_workerThread->waitUntilScriptLoaded();
    m_workerThread->terminateAndWait();
}

} // namespace blink
