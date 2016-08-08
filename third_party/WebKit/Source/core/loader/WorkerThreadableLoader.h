/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WorkerThreadableLoader_h
#define WorkerThreadableLoader_h

#include "core/dom/ExecutionContextTask.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoaderClientWrapper.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ResourceError;
class ResourceRequest;
class ResourceResponse;
class WorkerGlobalScope;
class WorkerLoaderProxy;
struct CrossThreadResourceRequestData;

// TODO(yhirano): Draw a diagram to illustrate the class relationship.
// TODO(yhirano): Rename inner classes so that readers can see in which thread
// they are living easily.
class WorkerThreadableLoader final : public ThreadableLoader {
    USING_FAST_MALLOC(WorkerThreadableLoader);
public:
    static void loadResourceSynchronously(WorkerGlobalScope&, const ResourceRequest&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
    static std::unique_ptr<WorkerThreadableLoader> create(WorkerGlobalScope& workerGlobalScope, ThreadableLoaderClient* client, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
    {
        return wrapUnique(new WorkerThreadableLoader(workerGlobalScope, client, options, resourceLoaderOptions, LoadAsynchronously));
    }

    ~WorkerThreadableLoader() override;

    void start(const ResourceRequest&) override;
    void overrideTimeout(unsigned long timeout) override;
    void cancel() override;

private:
    enum BlockingBehavior {
        LoadSynchronously,
        LoadAsynchronously
    };

    // A TaskForwarder forwards an ExecutionContextTask to the worker thread.
    class TaskForwarder : public GarbageCollectedFinalized<TaskForwarder> {
    public:
        virtual ~TaskForwarder() {}
        virtual void forwardTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>) = 0;
        virtual void forwardTaskWithDoneSignal(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>) = 0;
        virtual void abort() = 0;

        DEFINE_INLINE_VIRTUAL_TRACE() {}
    };
    class AsyncTaskForwarder;
    struct TaskWithLocation;
    class WaitableEventWithTasks;
    class SyncTaskForwarder;

    class Peer;
    // A Bridge instance lives in the worker thread and requests the associated
    // Peer (which lives in the main thread) to process loading tasks.
    class Bridge final : public GarbageCollectedFinalized<Bridge> {
    public:
        Bridge(ThreadableLoaderClientWrapper*, PassRefPtr<WorkerLoaderProxy>, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, BlockingBehavior);
        ~Bridge();

        void start(const ResourceRequest&, const WorkerGlobalScope&);
        void overrideTimeout(unsigned long timeoutMilliseconds);
        void cancel();
        void destroy();

        void didStart(Peer*);

        // This getter function is thread safe.
        ThreadableLoaderClientWrapper* clientWrapper() { return m_clientWrapper.get(); }

        DECLARE_VIRTUAL_TRACE();

    private:
        void cancelPeer();

        const Member<ThreadableLoaderClientWrapper> m_clientWrapper;
        const RefPtr<WorkerLoaderProxy> m_loaderProxy;
        const ThreadableLoaderOptions m_threadableLoaderOptions;
        const ResourceLoaderOptions m_resourceLoaderOptions;
        const BlockingBehavior m_blockingBehavior;

        // |*m_peer| lives in the main thread.
        CrossThreadPersistent<Peer> m_peer;
    };

    // A Peer instance lives in the main thread. It is a ThreadableLoaderClient
    // for a DocumentThreadableLoader and forward notifications to the
    // ThreadableLoaderClientWrapper which lives in the worker thread.
    class Peer final : public GarbageCollectedFinalized<Peer>, public ThreadableLoaderClient, public WorkerThreadLifecycleObserver {
        USING_GARBAGE_COLLECTED_MIXIN(Peer);
    public:
        static void createAndStart(
            Bridge*,
            PassRefPtr<WorkerLoaderProxy>,
            WorkerThreadLifecycleContext*,
            std::unique_ptr<CrossThreadResourceRequestData>,
            const ThreadableLoaderOptions&,
            const ResourceLoaderOptions&,
            PassRefPtr<WaitableEventWithTasks>,
            ExecutionContext*);
        ~Peer() override;

        void overrideTimeout(unsigned long timeoutMillisecond);
        void cancel();

        void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
        void didReceiveResponse(unsigned long identifier, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) override;
        void didReceiveData(const char*, unsigned dataLength) override;
        void didDownloadData(int dataLength) override;
        void didReceiveCachedMetadata(const char*, int dataLength) override;
        void didFinishLoading(unsigned long identifier, double finishTime) override;
        void didFail(const ResourceError&) override;
        void didFailAccessControlCheck(const ResourceError&) override;
        void didFailRedirectCheck() override;
        void didReceiveResourceTiming(const ResourceTimingInfo&) override;

        void contextDestroyed() override;

        DECLARE_TRACE();

    private:
        Peer(TaskForwarder*, WorkerThreadLifecycleContext*);
        void start(Document&, std::unique_ptr<CrossThreadResourceRequestData>, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);

        Member<TaskForwarder> m_forwarder;
        std::unique_ptr<ThreadableLoader> m_mainThreadLoader;

        // |*m_clientWrapper| lives in the worker thread.
        CrossThreadWeakPersistent<ThreadableLoaderClientWrapper> m_clientWrapper;
    };

    WorkerThreadableLoader(WorkerGlobalScope&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, BlockingBehavior);

    Persistent<WorkerGlobalScope> m_workerGlobalScope;
    const Persistent<ThreadableLoaderClientWrapper> m_workerClientWrapper;
    const Persistent<Bridge> m_bridge;
};

} // namespace blink

#endif // WorkerThreadableLoader_h
