// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

void destroyThread(WebThreadSupportingGC* thread)
{
    ASSERT(isMainThread());
    // The destructor for |thread| will block until all tasks have completed.
    // This guarantees that shutdown will finish before the thread is destroyed.
    delete thread;
}

class CompositorWorkerSharedState {
public:
    static CompositorWorkerSharedState& instance()
    {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CompositorWorkerSharedState, compositorWorkerSharedState, (new CompositorWorkerSharedState()));
        return compositorWorkerSharedState;
    }

    WebThreadSupportingGC* compositorWorkerThread()
    {
        MutexLocker lock(m_mutex);
        if (!m_thread && isMainThread()) {
            ASSERT(!m_workerCount);
            WebThread* platformThread = Platform::current()->compositorThread();
            m_thread = WebThreadSupportingGC::createForThread(platformThread);
        }
        return m_thread.get();
    }

    void initializeBackingThread()
    {
        MutexLocker lock(m_mutex);
        ASSERT(m_thread->isCurrentThread());
        ++m_workerCount;
        if (m_workerCount > 1)
            return;

        m_thread->initialize();

        // Initialize the isolate at the same time.
        ASSERT(!m_isolate);
        m_isolate = V8PerIsolateData::initialize();
        V8Initializer::initializeWorker(m_isolate);

        OwnPtr<V8IsolateInterruptor> interruptor = adoptPtr(new V8IsolateInterruptor(m_isolate));
        ThreadState::current()->addInterruptor(interruptor.release());
        ThreadState::current()->registerTraceDOMWrappers(m_isolate, V8GCController::traceDOMWrappers);
    }

    void shutdownBackingThread()
    {
        MutexLocker lock(m_mutex);
        ASSERT(m_thread->isCurrentThread());
        ASSERT(m_workerCount > 0);
        --m_workerCount;
        if (m_workerCount == 0) {
            m_thread->shutdown();
            Platform::current()->mainThread()->taskRunner()->postTask(
                BLINK_FROM_HERE, threadSafeBind(destroyThread, AllowCrossThreadAccess(m_thread.leakPtr())));
        }
    }

    v8::Isolate* initializeIsolate()
    {
        MutexLocker lock(m_mutex);
        ASSERT(m_thread->isCurrentThread());
        ASSERT(m_isolate);
        // It is safe to use the existing isolate even if TerminateExecution() has been
        // called on it, without calling CancelTerminateExecution().
        return m_isolate;
    }

    void willDestroyIsolate()
    {
        MutexLocker lock(m_mutex);
        ASSERT(m_thread->isCurrentThread());
        if (m_workerCount == 1)
            V8PerIsolateData::willBeDestroyed(m_isolate);
    }

    void destroyIsolate()
    {
        MutexLocker lock(m_mutex);
        if (!m_thread) {
            ASSERT(m_workerCount == 0);
            V8PerIsolateData::destroy(m_isolate);
            m_isolate = nullptr;
        }
    }

    void terminateV8Execution()
    {
        MutexLocker lock(m_mutex);
        ASSERT(isMainThread());
        if (m_workerCount > 1)
            return;

        m_isolate->TerminateExecution();
    }

    bool hasThreadForTest()
    {
        return m_thread;
    }

    bool hasIsolateForTest()
    {
        return m_isolate;
    }

private:
    CompositorWorkerSharedState() {}
    ~CompositorWorkerSharedState() {}

    Mutex m_mutex;
    OwnPtr<WebThreadSupportingGC> m_thread;
    int m_workerCount = 0;
    v8::Isolate* m_isolate = nullptr;
};

} // namespace

PassRefPtr<CompositorWorkerThread> CompositorWorkerThread::create(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerObjectProxy& workerObjectProxy, double timeOrigin)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::create");
    ASSERT(isMainThread());
    return adoptRef(new CompositorWorkerThread(workerLoaderProxy, workerObjectProxy, timeOrigin));
}

CompositorWorkerThread::CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerObjectProxy& workerObjectProxy, double timeOrigin)
    : WorkerThread(workerLoaderProxy, workerObjectProxy)
    , m_workerObjectProxy(workerObjectProxy)
    , m_timeOrigin(timeOrigin)
{
}

CompositorWorkerThread::~CompositorWorkerThread()
{
}

WebThreadSupportingGC* CompositorWorkerThread::sharedBackingThread()
{
    return CompositorWorkerSharedState::instance().compositorWorkerThread();
}

PassRefPtrWillBeRawPtr<WorkerGlobalScope> CompositorWorkerThread::createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::createWorkerGlobalScope");
    return CompositorWorkerGlobalScope::create(this, startupData, m_timeOrigin);
}

WebThreadSupportingGC& CompositorWorkerThread::backingThread()
{
    return *CompositorWorkerSharedState::instance().compositorWorkerThread();
}

void CompositorWorkerThread::initializeBackingThread()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::initializeBackingThread");
    CompositorWorkerSharedState::instance().initializeBackingThread();
}

void CompositorWorkerThread::shutdownBackingThread()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::shutdownBackingThread");
    CompositorWorkerSharedState::instance().shutdownBackingThread();
}

v8::Isolate* CompositorWorkerThread::initializeIsolate()
{
    return CompositorWorkerSharedState::instance().initializeIsolate();
}

void CompositorWorkerThread::willDestroyIsolate()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::willDestroyIsolate");
    CompositorWorkerSharedState::instance().willDestroyIsolate();
}

void CompositorWorkerThread::destroyIsolate()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::destroyIsolate");
    CompositorWorkerSharedState::instance().destroyIsolate();
}

void CompositorWorkerThread::terminateV8Execution()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::terminateV8Execution");
    CompositorWorkerSharedState::instance().terminateV8Execution();
}

bool CompositorWorkerThread::hasThreadForTest()
{
    return CompositorWorkerSharedState::instance().hasThreadForTest();
}

bool CompositorWorkerThread::hasIsolateForTest()
{
    return CompositorWorkerSharedState::instance().hasIsolateForTest();
}

} // namespace blink
