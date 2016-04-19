// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/Handle.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// A null InProcessWorkerObjectProxy, supplied when creating CompositorWorkerThreads.
class TestCompositorWorkerObjectProxy : public InProcessWorkerObjectProxy {
public:
    static PassOwnPtr<TestCompositorWorkerObjectProxy> create(ExecutionContext* context)
    {
        return adoptPtr(new TestCompositorWorkerObjectProxy(context));
    }

    // (Empty) WorkerReportingProxy implementation:
    virtual void reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, int exceptionId) {}
    void reportConsoleMessage(ConsoleMessage*) override {}
    void postMessageToPageInspector(const String&) override {}
    void postWorkerConsoleAgentEnabled() override {}

    void didEvaluateWorkerScript(bool success) override {}
    void workerGlobalScopeStarted(WorkerGlobalScope*) override {}
    void workerGlobalScopeClosed() override {}
    void workerThreadTerminated() override {}
    void willDestroyWorkerGlobalScope() override {}

    ExecutionContext* getExecutionContext() override { return m_executionContext.get(); }

private:
    TestCompositorWorkerObjectProxy(ExecutionContext* context)
        : InProcessWorkerObjectProxy(nullptr)
        , m_executionContext(context)
    {
    }

    Persistent<ExecutionContext> m_executionContext;
};

class CompositorWorkerTestPlatform : public TestingPlatformSupport {
public:
    CompositorWorkerTestPlatform()
        : m_thread(adoptPtr(m_oldPlatform->createThread("Compositor")))
    {
    }

    WebThread* compositorThread() const override
    {
        return m_thread.get();
    }

    WebCompositorSupport* compositorSupport() override { return &m_compositorSupport; }

private:
    OwnPtr<WebThread> m_thread;
    TestingCompositorSupport m_compositorSupport;
};

} // namespace

class CompositorWorkerThreadTest : public ::testing::Test {
public:
    void SetUp() override
    {
        CompositorWorkerThread::resetSharedBackingThreadForTest();
        m_page = DummyPageHolder::create();
        m_objectProxy = TestCompositorWorkerObjectProxy::create(&m_page->document());
        m_securityOrigin = SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
    }

    void TearDown() override
    {
        m_page.clear();
        CompositorWorkerThread::resetSharedBackingThreadForTest();
    }

    PassOwnPtr<CompositorWorkerThread> createCompositorWorker()
    {
        OwnPtr<CompositorWorkerThread> workerThread = CompositorWorkerThread::create(nullptr, *m_objectProxy, 0);
        WorkerClients* clients = nullptr;
        workerThread->start(WorkerThreadStartupData::create(
            KURL(ParsedURLString, "http://fake.url/"),
            "fake user agent",
            "//fake source code",
            nullptr,
            DontPauseWorkerGlobalScopeOnStart,
            adoptPtr(new Vector<CSPHeaderAndType>()),
            m_securityOrigin.get(),
            clients,
            WebAddressSpaceLocal,
            V8CacheOptionsDefault));
        return workerThread.release();
    }

    // Attempts to run some simple script for |worker|.
    void checkWorkerCanExecuteScript(WorkerThread* worker)
    {
        OwnPtr<WaitableEvent> waitEvent = adoptPtr(new WaitableEvent());
        worker->workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&CompositorWorkerThreadTest::executeScriptInWorker, AllowCrossThreadAccess(this),
            AllowCrossThreadAccess(worker), AllowCrossThreadAccess(waitEvent.get())));
        waitEvent->wait();
    }

private:
    void executeScriptInWorker(WorkerThread* worker, WaitableEvent* waitEvent)
    {
        EXPECT_GT(worker->workerBackingThread().workerScriptCount(), 0u);
        WorkerOrWorkletScriptController* scriptController = worker->workerGlobalScope()->scriptController();
        bool evaluateResult = scriptController->evaluate(ScriptSourceCode("var counter = 0; ++counter;"));
        ASSERT_UNUSED(evaluateResult, evaluateResult);
        waitEvent->signal();
    }

    OwnPtr<DummyPageHolder> m_page;
    RefPtr<SecurityOrigin> m_securityOrigin;
    OwnPtr<InProcessWorkerObjectProxy> m_objectProxy;
    CompositorWorkerTestPlatform m_testPlatform;
};

TEST_F(CompositorWorkerThreadTest, Basic)
{
    OwnPtr<CompositorWorkerThread> compositorWorker = createCompositorWorker();
    checkWorkerCanExecuteScript(compositorWorker.get());
    compositorWorker->terminateAndWait();
}

// Tests that the same WebThread is used for new workers if the WebThread is still alive.
TEST_F(CompositorWorkerThreadTest, CreateSecondAndTerminateFirst)
{
    // Create the first worker and wait until it is initialized.
    OwnPtr<CompositorWorkerThread> firstWorker = createCompositorWorker();
    WebThreadSupportingGC* firstThread = &firstWorker->workerBackingThread().backingThread();
    checkWorkerCanExecuteScript(firstWorker.get());
    v8::Isolate* firstIsolate = firstWorker->isolate();
    ASSERT_TRUE(firstIsolate);

    // Create the second worker and immediately destroy the first worker.
    OwnPtr<CompositorWorkerThread> secondWorker = createCompositorWorker();
    firstWorker->terminateAndWait();

    // Wait until the second worker is initialized. Verify that the second worker is using the same
    // thread and Isolate as the first worker.
    WebThreadSupportingGC* secondThread = &secondWorker->workerBackingThread().backingThread();
    ASSERT_EQ(firstThread, secondThread);

    v8::Isolate* secondIsolate = secondWorker->isolate();
    ASSERT_TRUE(secondIsolate);
    EXPECT_EQ(firstIsolate, secondIsolate);

    // Verify that the worker can still successfully execute script.
    checkWorkerCanExecuteScript(secondWorker.get());

    secondWorker->terminateAndWait();
}

// Tests that a new WebThread is created if all existing workers are terminated before a new worker is created.
TEST_F(CompositorWorkerThreadTest, TerminateFirstAndCreateSecond)
{
    // Create the first worker, wait until it is initialized, and terminate it.
    OwnPtr<CompositorWorkerThread> compositorWorker = createCompositorWorker();
    WorkerBackingThread* workerBackingThread = &compositorWorker->workerBackingThread();
    WebThreadSupportingGC* firstThread = &compositorWorker->workerBackingThread().backingThread();
    checkWorkerCanExecuteScript(compositorWorker.get());

    ASSERT_EQ(1u, workerBackingThread->workerScriptCount());
    compositorWorker->terminateAndWait();

    ASSERT_EQ(0u, workerBackingThread->workerScriptCount());

    // Create the second worker. The backing thread is same.
    compositorWorker = createCompositorWorker();
    WebThreadSupportingGC* secondThread = &compositorWorker->workerBackingThread().backingThread();
    EXPECT_EQ(firstThread, secondThread);
    checkWorkerCanExecuteScript(compositorWorker.get());
    ASSERT_EQ(1u, workerBackingThread->workerScriptCount());

    compositorWorker->terminateAndWait();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worker is created while another is terminating.
TEST_F(CompositorWorkerThreadTest, CreatingSecondDuringTerminationOfFirst)
{
    OwnPtr<CompositorWorkerThread> firstWorker = createCompositorWorker();
    checkWorkerCanExecuteScript(firstWorker.get());
    v8::Isolate* firstIsolate = firstWorker->isolate();
    ASSERT_TRUE(firstIsolate);

    // Request termination of the first worker and create the second worker
    // as soon as possible.
    EXPECT_EQ(1u, firstWorker->workerBackingThread().workerScriptCount());
    firstWorker->terminate();
    // We don't wait for its termination.
    // Note: We rely on the assumption that the termination steps don't run
    // on the worker thread so quickly. This could be a source of flakiness.

    OwnPtr<CompositorWorkerThread> secondWorker = createCompositorWorker();

    v8::Isolate* secondIsolate = secondWorker->isolate();
    ASSERT_TRUE(secondIsolate);
    EXPECT_EQ(firstIsolate, secondIsolate);

    // Verify that the isolate can run some scripts correctly in the second worker.
    checkWorkerCanExecuteScript(secondWorker.get());
    secondWorker->terminateAndWait();
}

} // namespace blink
