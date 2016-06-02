// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThread.h"

#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;

namespace blink {

class WorkerThreadTest : public ::testing::Test {
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
        m_workerThread->startWithSourceCode(m_securityOrigin.get(), "//fake source code");
    }

    void startWithSourceCodeNotToFinish()
    {
        // Use a JavaScript source code that makes an infinite loop so that we
        // can catch some kind of issues as a timeout.
        m_workerThread->startWithSourceCode(m_securityOrigin.get(), "while(true) {}");
    }

    void waitForShutdown()
    {
        m_workerThread->m_shutdownEvent->wait();
    }

    void setForceTerminationDelayInMs(long long forceTerminationDelayInMs)
    {
        m_workerThread->setForceTerminationDelayInMsForTesting(forceTerminationDelayInMs);
    }

    bool isForceTerminationTaskScheduled()
    {
        return m_workerThread->m_scheduledForceTerminationTask.get();
    }

protected:
    void expectReportingCalls()
    {
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(true)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated()).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope()).Times(1);
    }

    void expectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization()
    {
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_)).Times(AtMost(1));
        EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(_)).Times(AtMost(1));
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated()).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope()).Times(AtMost(1));
    }

    void expectReportingCallsForWorkerForciblyTerminated()
    {
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(false)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated()).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope()).Times(1);
    }

    RefPtr<SecurityOrigin> m_securityOrigin;
    OwnPtr<MockWorkerLoaderProxyProvider> m_mockWorkerLoaderProxyProvider;
    OwnPtr<MockWorkerReportingProxy> m_mockWorkerReportingProxy;
    OwnPtr<WorkerThreadForTest> m_workerThread;
};

TEST_F(WorkerThreadTest, StartAndTerminate_AsyncTerminate)
{
    expectReportingCalls();
    start();
    m_workerThread->waitForInit();

    // The worker thread is not being blocked, so the worker thread should be
    // gracefully shut down.
    m_workerThread->terminate();
    EXPECT_TRUE(isForceTerminationTaskScheduled());
    waitForShutdown();
    EXPECT_EQ(WorkerThread::ExitCode::GracefullyTerminated, m_workerThread->getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminate_SyncTerminate)
{
    expectReportingCalls();
    start();
    m_workerThread->waitForInit();
    m_workerThread->terminateAndWait();
    EXPECT_EQ(WorkerThread::ExitCode::SyncForciblyTerminated, m_workerThread->getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminateImmediately_AsyncTerminate)
{
    expectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
    start();

    // There are two possible cases depending on timing:
    // (1) If the thread hasn't been initialized on the worker thread yet,
    // terminate() should not attempt to shut down the thread.
    // (2) If the thread has already been initialized on the worker thread,
    // terminate() should gracefully shut down the thread.
    m_workerThread->terminate();
    waitForShutdown();
    WorkerThread::ExitCode exitCode = m_workerThread->getExitCode();
    EXPECT_EQ(WorkerThread::ExitCode::GracefullyTerminated, exitCode);
}

TEST_F(WorkerThreadTest, StartAndTerminateImmediately_SyncTerminate)
{
    expectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
    start();

    // There are two possible cases depending on timing:
    // (1) If the thread hasn't been initialized on the worker thread yet,
    // terminateAndWait() should not attempt to shut down the thread.
    // (2) If the thread has already been initialized on the worker thread,
    // terminateAndWait() should synchronously forcibly terminates the worker
    // execution.
    m_workerThread->terminateAndWait();
    WorkerThread::ExitCode exitCode = m_workerThread->getExitCode();
    EXPECT_TRUE(WorkerThread::ExitCode::GracefullyTerminated == exitCode || WorkerThread::ExitCode::SyncForciblyTerminated == exitCode);
}

TEST_F(WorkerThreadTest, StartAndTerminateOnScriptLoaded_SyncForciblyTerminate)
{
    expectReportingCallsForWorkerForciblyTerminated();
    startWithSourceCodeNotToFinish();
    m_workerThread->waitUntilScriptLoaded();

    // terminateAndWait() synchronously terminates the worker execution.
    m_workerThread->terminateAndWait();
    EXPECT_EQ(WorkerThread::ExitCode::SyncForciblyTerminated, m_workerThread->getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminateOnScriptLoaded_AsyncForciblyTerminate)
{
    const long long kForceTerminationDelayInMs = 10;
    setForceTerminationDelayInMs(kForceTerminationDelayInMs);

    expectReportingCallsForWorkerForciblyTerminated();
    startWithSourceCodeNotToFinish();
    m_workerThread->waitUntilScriptLoaded();

    // terminate() schedules a force termination task.
    m_workerThread->terminate();
    EXPECT_TRUE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, m_workerThread->getExitCode());

    // Wait until the force termination task runs.
    testing::runDelayedTasks(kForceTerminationDelayInMs);
    waitForShutdown();
    EXPECT_EQ(WorkerThread::ExitCode::AsyncForciblyTerminated, m_workerThread->getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminateOnScriptLoaded_AsyncForciblyTerminate_MultipleTimes)
{
    const long long kForceTerminationDelayInMs = 10;
    setForceTerminationDelayInMs(kForceTerminationDelayInMs);

    expectReportingCallsForWorkerForciblyTerminated();
    startWithSourceCodeNotToFinish();
    m_workerThread->waitUntilScriptLoaded();

    // terminate() schedules a force termination task.
    m_workerThread->terminate();
    EXPECT_TRUE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, m_workerThread->getExitCode());

    // Multiple terminate() calls should not take effect.
    m_workerThread->terminate();
    m_workerThread->terminate();
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, m_workerThread->getExitCode());

    // Wait until the force termination task runs.
    testing::runDelayedTasks(kForceTerminationDelayInMs);
    waitForShutdown();
    EXPECT_EQ(WorkerThread::ExitCode::AsyncForciblyTerminated, m_workerThread->getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminateOnScriptLoaded_SyncForciblyTerminateAfterTerminationTaskIsScheduled)
{
    const long long kForceTerminationDelayInMs = 10;
    setForceTerminationDelayInMs(kForceTerminationDelayInMs);

    expectReportingCallsForWorkerForciblyTerminated();
    startWithSourceCodeNotToFinish();
    m_workerThread->waitUntilScriptLoaded();

    // terminate() schedules a force termination task.
    m_workerThread->terminate();
    EXPECT_TRUE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, m_workerThread->getExitCode());

    // terminateAndWait() should overtake the scheduled force termination task.
    m_workerThread->terminateAndWait();
    EXPECT_FALSE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::SyncForciblyTerminated, m_workerThread->getExitCode());
}

// TODO(nhiroki): Add tests for terminateAndWaitForAllWorkers.

} // namespace blink
