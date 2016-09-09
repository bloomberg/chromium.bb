// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AbstractAnimationWorkletThread.h"

#include "core/workers/WorkerBackingThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// This is a singleton class holding the animation worklet thread in this
// renderer process. BackingThreadHolder::m_thread is cleared by
// ModulesInitializer::shutdown.
// See WorkerThread::terminateAndWaitForAllWorkers for the process shutdown
// case.
class BackingThreadHolder {
public:
    static BackingThreadHolder* instance()
    {
        MutexLocker locker(holderInstanceMutex());
        return s_instance;
    }

    static void ensureInstance()
    {
        if (!s_instance)
            s_instance = new BackingThreadHolder;
    }

    static void clear()
    {
        MutexLocker locker(holderInstanceMutex());
        if (s_instance) {
            s_instance->shutdownAndWait();
            delete s_instance;
            s_instance = nullptr;
        }
    }

    static void createForTest()
    {
        MutexLocker locker(holderInstanceMutex());
        DCHECK_EQ(nullptr, s_instance);
        s_instance = new BackingThreadHolder(WorkerBackingThread::createForTest(Platform::current()->compositorThread()));
    }

    WorkerBackingThread* thread() { return m_thread.get(); }

private:
    BackingThreadHolder(std::unique_ptr<WorkerBackingThread> useBackingThread = nullptr)
        : m_thread(useBackingThread ? std::move(useBackingThread) : WorkerBackingThread::create(Platform::current()->compositorThread()))
    {
        DCHECK(isMainThread());
        m_thread->backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&BackingThreadHolder::initializeOnThread, crossThreadUnretained(this)));
    }

    static Mutex& holderInstanceMutex()
    {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, holderMutex, new Mutex);
        return holderMutex;
    }

    void initializeOnThread()
    {
        MutexLocker locker(holderInstanceMutex());
        DCHECK(!m_initialized);
        m_thread->initialize();
        m_initialized = true;
    }

    void shutdownAndWait()
    {
        DCHECK(isMainThread());
        WaitableEvent doneEvent;
        m_thread->backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&BackingThreadHolder::shutdownOnThread, crossThreadUnretained(this), crossThreadUnretained(&doneEvent)));
        doneEvent.wait();
    }

    void shutdownOnThread(WaitableEvent* doneEvent)
    {
        m_thread->shutdown();
        doneEvent->signal();
    }

    std::unique_ptr<WorkerBackingThread> m_thread;
    bool m_initialized = false;

    static BackingThreadHolder* s_instance;
};

BackingThreadHolder* BackingThreadHolder::s_instance = nullptr;

} // namespace

AbstractAnimationWorkletThread::AbstractAnimationWorkletThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
    : WorkerThread(std::move(workerLoaderProxy), workerReportingProxy)
{
}

AbstractAnimationWorkletThread::~AbstractAnimationWorkletThread()
{
}

WorkerBackingThread& AbstractAnimationWorkletThread::workerBackingThread()
{
    return *BackingThreadHolder::instance()->thread();
}

void collectAllGarbageOnThread(WaitableEvent* doneEvent)
{
    blink::ThreadState::current()->collectAllGarbage();
    doneEvent->signal();
}

void AbstractAnimationWorkletThread::collectAllGarbage()
{
    DCHECK(isMainThread());
    WaitableEvent doneEvent;
    BackingThreadHolder* instance = BackingThreadHolder::instance();
    if (!instance)
        return;
    instance->thread()->backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&collectAllGarbageOnThread, crossThreadUnretained(&doneEvent)));
    doneEvent.wait();
}

void AbstractAnimationWorkletThread::ensureSharedBackingThread()
{
    DCHECK(isMainThread());
    BackingThreadHolder::ensureInstance();
}

void AbstractAnimationWorkletThread::clearSharedBackingThread()
{
    DCHECK(isMainThread());
    BackingThreadHolder::clear();
}

void AbstractAnimationWorkletThread::createSharedBackingThreadForTest()
{
    BackingThreadHolder::createForTest();
}

} // namespace blink
