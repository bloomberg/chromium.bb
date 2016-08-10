/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/WorkerThreadableLoader.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/timing/WorkerGlobalScopePerformance.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/heap/SafePoint.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/debug/Alias.h"
#include <memory>

namespace blink {

namespace {

std::unique_ptr<Vector<char>> createVectorFromMemoryRegion(const char* data, unsigned dataLength)
{
    std::unique_ptr<Vector<char>> buffer = wrapUnique(new Vector<char>(dataLength));
    memcpy(buffer->data(), data, dataLength);
    return buffer;
}

} // namespace

class WorkerThreadableLoader::AsyncTaskForwarder final : public WorkerThreadableLoader::TaskForwarder {
public:
    explicit AsyncTaskForwarder(PassRefPtr<WorkerLoaderProxy> loaderProxy)
        : m_loaderProxy(loaderProxy)
    {
        DCHECK(isMainThread());
    }
    ~AsyncTaskForwarder() override
    {
        DCHECK(isMainThread());
    }

    void forwardTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task) override
    {
        DCHECK(isMainThread());
        m_loaderProxy->postTaskToWorkerGlobalScope(location, std::move(task));
    }
    void forwardTaskWithDoneSignal(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task) override
    {
        DCHECK(isMainThread());
        m_loaderProxy->postTaskToWorkerGlobalScope(location, std::move(task));
    }
    void abort() override
    {
        DCHECK(isMainThread());
    }

private:
    RefPtr<WorkerLoaderProxy> m_loaderProxy;
};

struct WorkerThreadableLoader::TaskWithLocation final {
    TaskWithLocation(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task)
        : m_location(location)
        , m_task(std::move(task))
    {
    }
    TaskWithLocation(TaskWithLocation&& task)
        : TaskWithLocation(task.m_location, std::move(task.m_task))
    {
    }
    ~TaskWithLocation() = default;

    WebTraceLocation m_location;
    std::unique_ptr<ExecutionContextTask> m_task;
};

// Observing functions and wait() need to be called on the worker thread.
// Setting functions and signal() need to be called on the main thread.
// All observing functions must be called after wait() returns, and all
// setting functions must be called before signal() is called.
class WorkerThreadableLoader::WaitableEventWithTasks final : public ThreadSafeRefCounted<WaitableEventWithTasks> {
public:
    static PassRefPtr<WaitableEventWithTasks> create() { return adoptRef(new WaitableEventWithTasks); }

    void signal()
    {
        DCHECK(isMainThread());
        CHECK(!m_isSignalCalled);
        m_isSignalCalled = true;
        m_event.signal();
    }
    void wait()
    {
        DCHECK(!isMainThread());
        CHECK(!m_isWaitDone);
        m_event.wait();
        m_isWaitDone = true;
    }

    // Observing functions
    bool isAborted() const
    {
        DCHECK(!isMainThread());
        CHECK(m_isWaitDone);
        return m_isAborted;
    }
    Vector<TaskWithLocation> take()
    {
        DCHECK(!isMainThread());
        CHECK(m_isWaitDone);
        return std::move(m_tasks);
    }

    // Setting functions
    void append(TaskWithLocation task)
    {
        DCHECK(isMainThread());
        CHECK(!m_isSignalCalled);
        m_tasks.append(std::move(task));
    }
    void setIsAborted()
    {
        DCHECK(isMainThread());
        CHECK(!m_isSignalCalled);
        m_isAborted = true;
    }

private:
    WaitableEventWithTasks() {}

    WaitableEvent m_event;
    Vector<TaskWithLocation> m_tasks;
    bool m_isAborted = false;
    bool m_isSignalCalled = false;
    bool m_isWaitDone = false;
};

class WorkerThreadableLoader::SyncTaskForwarder final : public WorkerThreadableLoader::TaskForwarder {
public:
    explicit SyncTaskForwarder(PassRefPtr<WaitableEventWithTasks> eventWithTasks)
        : m_eventWithTasks(eventWithTasks)
    {
        DCHECK(isMainThread());
    }
    ~SyncTaskForwarder() override
    {
        DCHECK(isMainThread());
    }

    void forwardTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task) override
    {
        DCHECK(isMainThread());
        m_eventWithTasks->append(TaskWithLocation(location, std::move(task)));
    }
    void forwardTaskWithDoneSignal(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task) override
    {
        DCHECK(isMainThread());
        m_eventWithTasks->append(TaskWithLocation(location, std::move(task)));
        m_eventWithTasks->signal();
    }
    void abort() override
    {
        DCHECK(isMainThread());
        m_eventWithTasks->setIsAborted();
        m_eventWithTasks->signal();
    }

private:
    RefPtr<WaitableEventWithTasks> m_eventWithTasks;
};

WorkerThreadableLoader::WorkerThreadableLoader(
    WorkerGlobalScope& workerGlobalScope,
    ThreadableLoaderClient* client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions,
    BlockingBehavior blockingBehavior)
    : m_workerGlobalScope(&workerGlobalScope)
    , m_workerClientWrapper(new ThreadableLoaderClientWrapper(workerGlobalScope, client))
    , m_bridge(new Bridge(m_workerClientWrapper, workerGlobalScope.thread()->workerLoaderProxy(), options, resourceLoaderOptions, blockingBehavior))
{
}

void WorkerThreadableLoader::loadResourceSynchronously(WorkerGlobalScope& workerGlobalScope, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
{
    std::unique_ptr<WorkerThreadableLoader> loader = wrapUnique(new WorkerThreadableLoader(workerGlobalScope, &client, options, resourceLoaderOptions, LoadSynchronously));
    loader->start(request);
}

WorkerThreadableLoader::~WorkerThreadableLoader()
{
    DCHECK(m_workerClientWrapper->done());
    m_bridge->destroy();
}

void WorkerThreadableLoader::start(const ResourceRequest& request)
{
    ResourceRequest requestToPass(request);
    if (!requestToPass.didSetHTTPReferrer())
        requestToPass.setHTTPReferrer(SecurityPolicy::generateReferrer(m_workerGlobalScope->getReferrerPolicy(), request.url(), m_workerGlobalScope->outgoingReferrer()));
    m_bridge->start(requestToPass, *m_workerGlobalScope);
}

void WorkerThreadableLoader::overrideTimeout(unsigned long timeoutMilliseconds)
{
    m_bridge->overrideTimeout(timeoutMilliseconds);
}

void WorkerThreadableLoader::cancel()
{
    m_bridge->cancel();
}

WorkerThreadableLoader::Bridge::Bridge(
    ThreadableLoaderClientWrapper* clientWrapper,
    PassRefPtr<WorkerLoaderProxy> loaderProxy,
    const ThreadableLoaderOptions& threadableLoaderOptions,
    const ResourceLoaderOptions& resourceLoaderOptions,
    BlockingBehavior blockingBehavior)
    : m_clientWrapper(clientWrapper)
    , m_loaderProxy(loaderProxy)
    , m_threadableLoaderOptions(threadableLoaderOptions)
    , m_resourceLoaderOptions(resourceLoaderOptions)
    , m_blockingBehavior(blockingBehavior)
{
    DCHECK(!isMainThread());
}

WorkerThreadableLoader::Bridge::~Bridge()
{
    DCHECK(!isMainThread());
    DCHECK(!m_peer);
}

void WorkerThreadableLoader::Bridge::start(const ResourceRequest& request, const WorkerGlobalScope& workerGlobalScope)
{
    DCHECK(!isMainThread());
    RefPtr<WaitableEventWithTasks> eventWithTasks;
    if (m_blockingBehavior == LoadSynchronously)
        eventWithTasks = WaitableEventWithTasks::create();

    m_loaderProxy->postTaskToLoader(BLINK_FROM_HERE, createCrossThreadTask(
        &Peer::createAndStart,
        wrapCrossThreadPersistent(this),
        m_loaderProxy,
        wrapCrossThreadPersistent(workerGlobalScope.thread()->getWorkerThreadLifecycleContext()),
        request,
        m_threadableLoaderOptions,
        m_resourceLoaderOptions,
        eventWithTasks));

    if (m_blockingBehavior == LoadAsynchronously)
        return;

    {
        SafePointScope scope(BlinkGC::HeapPointersOnStack);
        eventWithTasks->wait();
    }

    if (eventWithTasks->isAborted()) {
        // This thread is going to terminate.
        cancel();
        return;
    }

    for (const auto& task : eventWithTasks->take()) {
        // Store the program counter where the task is posted from, and alias
        // it to ensure it is stored in the crash dump.
        const void* programCounter = task.m_location.program_counter();
        WTF::debug::alias(&programCounter);

        // m_clientTask contains only CallClosureTasks. So, it's ok to pass
        // the nullptr.
        task.m_task->performTask(nullptr);
    }
}

void WorkerThreadableLoader::Bridge::overrideTimeout(unsigned long timeoutMilliseconds)
{
    DCHECK(!isMainThread());
    if (!m_peer)
        return;
    m_loaderProxy->postTaskToLoader(BLINK_FROM_HERE, createCrossThreadTask(&Peer::overrideTimeout, m_peer, timeoutMilliseconds));
}

void WorkerThreadableLoader::Bridge::cancel()
{
    DCHECK(!isMainThread());
    cancelPeer();

    if (m_clientWrapper->done())
        return;
    // If the client hasn't reached a termination state, then transition it
    // by sending a cancellation error.
    // Note: no more client callbacks will be done after this method -- the
    // clearClient() call ensures that.
    ResourceError error(String(), 0, String(), String());
    error.setIsCancellation(true);
    m_clientWrapper->didFail(error);
    m_clientWrapper->clearClient();
}

void WorkerThreadableLoader::Bridge::destroy()
{
    DCHECK(!isMainThread());
    cancelPeer();
    m_clientWrapper->clearClient();
}

void WorkerThreadableLoader::Bridge::didStart(Peer* peer)
{
    DCHECK(!isMainThread());
    DCHECK(!m_peer);
    DCHECK(peer);
    if (m_clientWrapper->done()) {
        // The thread is terminating.
        m_loaderProxy->postTaskToLoader(BLINK_FROM_HERE, createCrossThreadTask(&Peer::cancel, wrapCrossThreadPersistent(peer)));
        return;
    }

    m_peer = peer;
}

DEFINE_TRACE(WorkerThreadableLoader::Bridge)
{
    visitor->trace(m_clientWrapper);
}

void WorkerThreadableLoader::Bridge::cancelPeer()
{
    DCHECK(!isMainThread());
    if (!m_peer)
        return;
    m_loaderProxy->postTaskToLoader(BLINK_FROM_HERE, createCrossThreadTask(&Peer::cancel, m_peer));
    m_peer = nullptr;
}

void WorkerThreadableLoader::Peer::createAndStart(
    Bridge* bridge,
    PassRefPtr<WorkerLoaderProxy> passLoaderProxy,
    WorkerThreadLifecycleContext* workerThreadLifecycleContext,
    std::unique_ptr<CrossThreadResourceRequestData> request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions,
    PassRefPtr<WaitableEventWithTasks> eventWithTasks,
    ExecutionContext* executionContext)
{
    DCHECK(isMainThread());
    TaskForwarder* forwarder;
    RefPtr<WorkerLoaderProxy> loaderProxy = passLoaderProxy;
    if (eventWithTasks)
        forwarder = new SyncTaskForwarder(eventWithTasks);
    else
        forwarder = new AsyncTaskForwarder(loaderProxy);

    Peer* peer = new Peer(forwarder, workerThreadLifecycleContext);
    if (peer->wasContextDestroyedBeforeObserverCreation()) {
        // The thread is already terminating.
        forwarder->abort();
        peer->m_forwarder = nullptr;
        return;
    }
    peer->m_clientWrapper = bridge->clientWrapper();
    peer->start(*toDocument(executionContext), std::move(request), options, resourceLoaderOptions);
    forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&Bridge::didStart, wrapCrossThreadPersistent(bridge), wrapCrossThreadPersistent(peer)));
}

WorkerThreadableLoader::Peer::~Peer()
{
    DCHECK(isMainThread());
    DCHECK(!m_mainThreadLoader);
}

void WorkerThreadableLoader::Peer::overrideTimeout(unsigned long timeoutMilliseconds)
{
    DCHECK(isMainThread());
    if (!m_mainThreadLoader)
        return;
    m_mainThreadLoader->overrideTimeout(timeoutMilliseconds);
}

void WorkerThreadableLoader::Peer::cancel()
{
    DCHECK(isMainThread());
    if (!m_mainThreadLoader)
        return;
    m_mainThreadLoader->cancel();
    m_mainThreadLoader = nullptr;
}

void WorkerThreadableLoader::Peer::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didSendData, clientWrapper, bytesSent, totalBytesToBeSent));
}

void WorkerThreadableLoader::Peer::didReceiveResponse(unsigned long identifier, const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle> handle)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didReceiveResponse, clientWrapper, identifier, response, passed(std::move(handle))));
}

void WorkerThreadableLoader::Peer::didReceiveData(const char* data, unsigned dataLength)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didReceiveData, clientWrapper, passed(createVectorFromMemoryRegion(data, dataLength))));
}

void WorkerThreadableLoader::Peer::didDownloadData(int dataLength)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didDownloadData, clientWrapper, dataLength));
}

void WorkerThreadableLoader::Peer::didReceiveCachedMetadata(const char* data, int dataLength)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didReceiveCachedMetadata, clientWrapper, passed(createVectorFromMemoryRegion(data, dataLength))));
}

void WorkerThreadableLoader::Peer::didFinishLoading(unsigned long identifier, double finishTime)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTaskWithDoneSignal(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didFinishLoading, clientWrapper, identifier, finishTime));
    m_forwarder = nullptr;
}

void WorkerThreadableLoader::Peer::didFail(const ResourceError& error)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTaskWithDoneSignal(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didFail, clientWrapper, error));
    m_forwarder = nullptr;
}

void WorkerThreadableLoader::Peer::didFailAccessControlCheck(const ResourceError& error)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTaskWithDoneSignal(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didFailAccessControlCheck, clientWrapper, error));
    m_forwarder = nullptr;
}

void WorkerThreadableLoader::Peer::didFailRedirectCheck()
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTaskWithDoneSignal(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didFailRedirectCheck, clientWrapper));
    m_forwarder = nullptr;
}

void WorkerThreadableLoader::Peer::didReceiveResourceTiming(const ResourceTimingInfo& info)
{
    DCHECK(isMainThread());
    CrossThreadPersistent<ThreadableLoaderClientWrapper> clientWrapper = m_clientWrapper.get();
    if (!clientWrapper || !m_forwarder)
        return;
    m_forwarder->forwardTask(BLINK_FROM_HERE, createCrossThreadTask(&ThreadableLoaderClientWrapper::didReceiveResourceTiming, clientWrapper, info));
}

void WorkerThreadableLoader::Peer::contextDestroyed()
{
    DCHECK(isMainThread());
    if (m_forwarder) {
        m_forwarder->abort();
        m_forwarder = nullptr;
    }
    m_clientWrapper = nullptr;
    cancel();
}

DEFINE_TRACE(WorkerThreadableLoader::Peer)
{
    visitor->trace(m_forwarder);
    WorkerThreadLifecycleObserver::trace(visitor);
}

WorkerThreadableLoader::Peer::Peer(TaskForwarder* forwarder, WorkerThreadLifecycleContext* context)
    : WorkerThreadLifecycleObserver(context)
    , m_forwarder(forwarder)
{
    DCHECK(isMainThread());
}

void WorkerThreadableLoader::Peer::start(
    Document& document,
    std::unique_ptr<CrossThreadResourceRequestData> request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& originalResourceLoaderOptions)
{
    DCHECK(isMainThread());
    ResourceLoaderOptions resourceLoaderOptions = originalResourceLoaderOptions;
    resourceLoaderOptions.requestInitiatorContext = WorkerContext;
    m_mainThreadLoader = DocumentThreadableLoader::create(document, this, options, resourceLoaderOptions);
    m_mainThreadLoader->start(ResourceRequest(request.get()));
}

} // namespace blink
