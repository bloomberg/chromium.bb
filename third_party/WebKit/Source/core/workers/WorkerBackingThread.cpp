// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerBackingThread.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IdleTaskRunner.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

WorkerBackingThread::WorkerBackingThread(const char* name, bool shouldCallGCOnShutdown)
    : m_backingThread(WebThreadSupportingGC::create(name))
    , m_isOwningThread(true)
    , m_shouldCallGCOnShutdown(shouldCallGCOnShutdown)
{
}

WorkerBackingThread::WorkerBackingThread(WebThread* thread, bool shouldCallGCOnShutdown)
    : m_backingThread(WebThreadSupportingGC::createForThread(thread))
    , m_isOwningThread(false)
    , m_shouldCallGCOnShutdown(shouldCallGCOnShutdown)
{
}

WorkerBackingThread::~WorkerBackingThread()
{
#if DCHECK_IS_ON()
    MutexLocker locker(m_mutex);
    DCHECK_EQ(0u, m_workerScriptCount);
#endif
}

void WorkerBackingThread::attach()
{
    {
        MutexLocker locker(m_mutex);
        if (++m_workerScriptCount > 1)
            return;
    }
    initialize();
}

void WorkerBackingThread::detach()
{
    {
        MutexLocker locker(m_mutex);
        if (--m_workerScriptCount > 0)
            return;
    }
    shutdown();
}

void WorkerBackingThread::initialize()
{
    DCHECK(!m_isolate);
    m_isolate = V8PerIsolateData::initialize();
    V8Initializer::initializeWorker(m_isolate);
    m_backingThread->initialize();

    OwnPtr<V8IsolateInterruptor> interruptor = adoptPtr(new V8IsolateInterruptor(m_isolate));
    ThreadState::current()->addInterruptor(interruptor.release());
    ThreadState::current()->registerTraceDOMWrappers(m_isolate, V8GCController::traceDOMWrappers);
    if (RuntimeEnabledFeatures::v8IdleTasksEnabled())
        V8PerIsolateData::enableIdleTasks(m_isolate, adoptPtr(new V8IdleTaskRunner(backingThread().platformThread().scheduler())));
    if (m_isOwningThread)
        Platform::current()->didStartWorkerThread();
}

void WorkerBackingThread::shutdown()
{
    if (m_isOwningThread)
        Platform::current()->willStopWorkerThread();

    V8PerIsolateData::willBeDestroyed(m_isolate);
    // TODO(yhirano): Remove this when https://crbug.com/v8/1428 is fixed.
    if (m_shouldCallGCOnShutdown) {
        // This statement runs only in tests.
        V8GCController::collectAllGarbageForTesting(m_isolate);
    }
    m_backingThread->shutdown();

    V8PerIsolateData::destroy(m_isolate);
    m_isolate = nullptr;
}

} // namespace blink
