// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThread.h"

#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/WaitableEvent.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

using testing::_;
using testing::AtMost;

namespace blink {

namespace {

// Called from WorkerThread::startRunningDebuggerTasksOnPauseOnWorkerThread as a
// debugger task.
void waitForTermination(WorkerThread* workerThread, WaitableEvent* waitableEvent)
{
    EXPECT_TRUE(workerThread->isCurrentThread());

    // Notify the main thread that the debugger task is waiting for termination.
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&testing::exitRunLoop));
    waitableEvent->wait();
}

} // namespace

class WorkerThreadLifecycleObserverForTest final : public GarbageCollectedFinalized<WorkerThreadLifecycleObserverForTest>, public WorkerThreadLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(WorkerThreadLifecycleObserverForTest);
    WTF_MAKE_NONCOPYABLE(WorkerThreadLifecycleObserverForTest);
public:
    explicit WorkerThreadLifecycleObserverForTest(WorkerThreadLifecycleContext* context)
        : WorkerThreadLifecycleObserver(context)
    {
        DCHECK(!wasContextDestroyedBeforeObserverCreation());
    }
    ~WorkerThreadLifecycleObserverForTest() override {}

    void contextDestroyed() override
    {
        if (m_closure)
            (*m_closure)();
    }

    void setClosure(std::unique_ptr<WTF::Closure> closure)
    {
        m_closure = std::move(closure);
    }

private:
    std::unique_ptr<WTF::Closure> m_closure;
};

class WorkerThreadTest : public ::testing::Test {
public:
    void SetUp() override
    {
        m_mockWorkerLoaderProxyProvider = wrapUnique(new MockWorkerLoaderProxyProvider());
        m_mockWorkerReportingProxy = wrapUnique(new MockWorkerReportingProxy());
        m_securityOrigin = SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
        m_workerThread = wrapUnique(new WorkerThreadForTest(
            m_mockWorkerLoaderProxyProvider.get(),
            *m_mockWorkerReportingProxy));
        m_workerThreadLifecycleObserver = new WorkerThreadLifecycleObserverForTest(m_workerThread->getWorkerThreadLifecycleContext());
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

    void setForceTerminationDelayInMs(long long forceTerminationDelayInMs)
    {
        m_workerThread->m_forceTerminationDelayInMs = forceTerminationDelayInMs;
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

    WorkerThread::ExitCode getExitCode()
    {
        return m_workerThread->getExitCodeForTesting();
    }

    RefPtr<SecurityOrigin> m_securityOrigin;
    std::unique_ptr<MockWorkerLoaderProxyProvider> m_mockWorkerLoaderProxyProvider;
    std::unique_ptr<MockWorkerReportingProxy> m_mockWorkerReportingProxy;
    std::unique_ptr<WorkerThreadForTest> m_workerThread;
    Persistent<WorkerThreadLifecycleObserverForTest> m_workerThreadLifecycleObserver;
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
    m_workerThread->waitForShutdownForTesting();
    EXPECT_EQ(WorkerThread::ExitCode::GracefullyTerminated, getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminate_SyncTerminate)
{
    expectReportingCalls();
    start();
    m_workerThread->waitForInit();
    m_workerThread->terminateAndWait();
    EXPECT_EQ(WorkerThread::ExitCode::SyncForciblyTerminated, getExitCode());
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
    m_workerThread->waitForShutdownForTesting();
    WorkerThread::ExitCode exitCode = getExitCode();
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
    WorkerThread::ExitCode exitCode = getExitCode();
    EXPECT_TRUE(WorkerThread::ExitCode::GracefullyTerminated == exitCode || WorkerThread::ExitCode::SyncForciblyTerminated == exitCode);
}

TEST_F(WorkerThreadTest, StartAndTerminateOnInitialization_TerminateWhileDebuggerTaskIsRunning)
{
    EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_)).Times(1);
    EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated()).Times(1);
    EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope()).Times(1);

    std::unique_ptr<Vector<CSPHeaderAndType>> headers = wrapUnique(new Vector<CSPHeaderAndType>());
    CSPHeaderAndType headerAndType("contentSecurityPolicy", ContentSecurityPolicyHeaderTypeReport);
    headers->append(headerAndType);

    // Specify PauseWorkerGlobalScopeOnStart so that the worker thread can pause
    // on initialziation to run debugger tasks.
    std::unique_ptr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(
        KURL(ParsedURLString, "http://fake.url/"),
        "fake user agent",
        "//fake source code",
        nullptr, /* cachedMetaData */
        PauseWorkerGlobalScopeOnStart,
        headers.get(),
        "",
        m_securityOrigin.get(),
        nullptr, /* workerClients */
        WebAddressSpaceLocal,
        nullptr /* originTrialToken */,
        nullptr /* WorkerSettings */,
        V8CacheOptionsDefault);
    m_workerThread->start(std::move(startupData));

    // Used to wait for worker thread termination in a debugger task on the
    // worker thread. Signaled when WorkerThreadLifecycleContext is destroyed on
    // the main thread.
    WaitableEvent waitableEvent(WaitableEvent::ResetPolicy::Manual, WaitableEvent::InitialState::NonSignaled);
    m_workerThread->appendDebuggerTask(crossThreadBind(&waitForTermination, crossThreadUnretained(m_workerThread.get()), crossThreadUnretained(&waitableEvent)));
    m_workerThreadLifecycleObserver->setClosure(WTF::bind(&WaitableEvent::signal, unretained(&waitableEvent)));

    // Wait for the debugger task.
    testing::enterRunLoop();

    // Start termination while the debugger task is running.
    EXPECT_TRUE(m_workerThread->m_runningDebuggerTask);
    m_workerThread->terminateAndWait();
    EXPECT_EQ(WorkerThread::ExitCode::GracefullyTerminated, getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminateOnScriptLoaded_SyncForciblyTerminate)
{
    expectReportingCallsForWorkerForciblyTerminated();
    startWithSourceCodeNotToFinish();
    m_workerThread->waitUntilScriptLoaded();

    // terminateAndWait() synchronously terminates the worker execution.
    m_workerThread->terminateAndWait();
    EXPECT_EQ(WorkerThread::ExitCode::SyncForciblyTerminated, getExitCode());
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
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // Wait until the force termination task runs.
    testing::runDelayedTasks(kForceTerminationDelayInMs);
    m_workerThread->waitForShutdownForTesting();
    EXPECT_EQ(WorkerThread::ExitCode::AsyncForciblyTerminated, getExitCode());
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
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // Multiple terminate() calls should not take effect.
    m_workerThread->terminate();
    m_workerThread->terminate();
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // Wait until the force termination task runs.
    testing::runDelayedTasks(kForceTerminationDelayInMs);
    m_workerThread->waitForShutdownForTesting();
    EXPECT_EQ(WorkerThread::ExitCode::AsyncForciblyTerminated, getExitCode());
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
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // terminateAndWait() should overtake the scheduled force termination task.
    m_workerThread->terminateAndWait();
    EXPECT_FALSE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::SyncForciblyTerminated, getExitCode());
}

TEST_F(WorkerThreadTest, StartAndTerminateOnScriptLoaded_TerminateWhileDebuggerTaskIsRunning)
{
    expectReportingCallsForWorkerForciblyTerminated();
    startWithSourceCodeNotToFinish();
    m_workerThread->waitUntilScriptLoaded();

    // Simulate that a debugger task is running.
    m_workerThread->m_runningDebuggerTask = true;

    // terminate() should not schedule a force termination task because there is
    // a running debugger task.
    m_workerThread->terminate();
    EXPECT_FALSE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // Multiple terminate() calls should not take effect.
    m_workerThread->terminate();
    m_workerThread->terminate();
    EXPECT_FALSE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // Focible termination request should also respect the running debugger
    // task.
    m_workerThread->terminateInternal(WorkerThread::TerminationMode::Forcible);
    EXPECT_FALSE(isForceTerminationTaskScheduled());
    EXPECT_EQ(WorkerThread::ExitCode::NotTerminated, getExitCode());

    // Clean up in order to satisfy DCHECKs in dtors.
    m_workerThread->m_runningDebuggerTask = false;
    m_workerThread->forciblyTerminateExecution();
    m_workerThread->waitForShutdownForTesting();
}

// TODO(nhiroki): Add tests for terminateAndWaitForAllWorkers.

} // namespace blink
