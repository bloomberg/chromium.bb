// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

// This is a singleton class holding the compositor worker thread in this
// renderrer process. BackingThreadHolst::m_thread will never be cleared,
// but Oilpan and V8 are detached from the thread when the last compositor
// worker thread is gone.
// See WorkerThread::terminateAndWaitForAllWorkers for the process shutdown
// case.
class BackingThreadHolder {
public:
    static BackingThreadHolder& instance()
    {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(BackingThreadHolder, holder, new BackingThreadHolder);
        return holder;
    }

    WorkerBackingThread* thread() { return m_thread.get(); }
    void resetForTest()
    {
        ASSERT(!m_thread || (m_thread->workerScriptCount() == 0));
        m_thread = nullptr;
        m_thread = WorkerBackingThread::createForTest(Platform::current()->compositorThread());
    }

private:
    BackingThreadHolder() : m_thread(WorkerBackingThread::create(Platform::current()->compositorThread())) {}

    OwnPtr<WorkerBackingThread> m_thread;
};

} // namespace

PassOwnPtr<CompositorWorkerThread> CompositorWorkerThread::create(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, InProcessWorkerObjectProxy& workerObjectProxy, double timeOrigin)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::create");
    ASSERT(isMainThread());
    return adoptPtr(new CompositorWorkerThread(workerLoaderProxy, workerObjectProxy, timeOrigin));
}

CompositorWorkerThread::CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, InProcessWorkerObjectProxy& workerObjectProxy, double timeOrigin)
    : WorkerThread(workerLoaderProxy, workerObjectProxy)
    , m_workerObjectProxy(workerObjectProxy)
    , m_timeOrigin(timeOrigin)
{
}

CompositorWorkerThread::~CompositorWorkerThread()
{
}

WorkerBackingThread& CompositorWorkerThread::workerBackingThread()
{
    return *BackingThreadHolder::instance().thread();
}

WorkerGlobalScope*CompositorWorkerThread::createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::createWorkerGlobalScope");
    return CompositorWorkerGlobalScope::create(this, std::move(startupData), m_timeOrigin);
}

void CompositorWorkerThread::resetSharedBackingThreadForTest()
{
    BackingThreadHolder::instance().resetForTest();
}

} // namespace blink
