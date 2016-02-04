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

#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoaderClientWrapper.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/Functional.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContextTask;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class WebWaitableEvent;
class WorkerGlobalScope;
class WorkerLoaderProxy;
struct CrossThreadResourceRequestData;

class WorkerThreadableLoader final : public ThreadableLoader, private ThreadableLoaderClientWrapper::ResourceTimingClient {
    USING_FAST_MALLOC(WorkerThreadableLoader);
public:
    static void loadResourceSynchronously(WorkerGlobalScope&, const ResourceRequest&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
    static PassRefPtr<WorkerThreadableLoader> create(WorkerGlobalScope& workerGlobalScope, ThreadableLoaderClient* client, const ResourceRequest& request, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
    {
        return adoptRef(new WorkerThreadableLoader(workerGlobalScope, client, request, options, resourceLoaderOptions, LoadAsynchronously));
    }

    ~WorkerThreadableLoader() override;

    void overrideTimeout(unsigned long timeout) override;

    void cancel() override;

private:
    enum BlockingBehavior {
        LoadSynchronously,
        LoadAsynchronously
    };

    // Creates a loader on the main thread and bridges communication between
    // the main thread and the worker context's thread where WorkerThreadableLoader runs.
    //
    // Regarding the bridge and lifetimes of items used in callbacks, there are a few cases:
    //
    // all cases. All tasks posted from the worker context's thread are ok because
    //    the last task posted always is "mainThreadDestroy", so MainThreadBridge is
    //    around for all tasks that use it on the main thread.
    //
    // case 1. worker.terminate is called.
    //    In this case, no more tasks are posted from the worker object's thread to the worker
    //    context's thread -- WorkerGlobalScopeProxy implementation enforces this.
    //
    // case 2. xhr gets aborted and the worker context continues running.
    //    The ThreadableLoaderClientWrapper has the underlying client cleared, so no more calls
    //    go through it.  All tasks posted from the worker object's thread to the worker context's
    //    thread do "ThreadableLoaderClientWrapper::ref" (automatically inside of the cross thread copy
    //    done in createCrossThreadTask), so the ThreadableLoaderClientWrapper instance is there until all
    //    tasks are executed.
    class MainThreadBridgeBase : public ThreadableLoaderClient {
    public:
        // All executed on the worker context's thread.
        MainThreadBridgeBase(PassRefPtr<ThreadableLoaderClientWrapper>, PassRefPtr<WorkerLoaderProxy>);
        void overrideTimeout(unsigned long timeoutMilliseconds);
        void cancel();
        void destroy();

        // All executed on the main thread.
        void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final;
        void didReceiveResponse(unsigned long identifier, const ResourceResponse&, PassOwnPtr<WebDataConsumerHandle>) final;
        void didReceiveData(const char*, unsigned dataLength) final;
        void didDownloadData(int dataLength) final;
        void didReceiveCachedMetadata(const char*, int dataLength) final;
        void didFinishLoading(unsigned long identifier, double finishTime) final;
        void didFail(const ResourceError&) final;
        void didFailAccessControlCheck(const ResourceError&) final;
        void didFailRedirectCheck() final;
        void didReceiveResourceTiming(const ResourceTimingInfo&) final;

    protected:
        ~MainThreadBridgeBase() override;

        // Posts a task to the main thread to run mainThreadCreateLoader().
        void createLoader(const ResourceRequest&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, const ReferrerPolicy&, const String&);

        WorkerLoaderProxy* loaderProxy()
        {
            return m_loaderProxy.get();
        }

        PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper()
        {
            return m_workerClientWrapper;
        }

    private:
        // The following methods are overridden by the subclasses to implement
        // code to forward did.* method invocations to the worker context's
        // thread which is specialized for sync and async case respectively.
        virtual void forwardTaskToWorker(PassOwnPtr<ExecutionContextTask>) = 0;
        virtual void forwardTaskToWorkerOnLoaderDone(PassOwnPtr<ExecutionContextTask>) = 0;

        // All executed on the main thread.
        void mainThreadCreateLoader(PassOwnPtr<CrossThreadResourceRequestData>, ThreadableLoaderOptions, ResourceLoaderOptions, const ReferrerPolicy, const String& outgoingReferrer, ExecutionContext*);
        void mainThreadDestroy(ExecutionContext*);
        void mainThreadOverrideTimeout(unsigned long timeoutMilliseconds, ExecutionContext*);
        void mainThreadCancel(ExecutionContext*);

        // Only to be used on the main thread.
        RefPtr<ThreadableLoader> m_mainThreadLoader;

        // ThreadableLoaderClientWrapper is to be used on the worker context thread.
        // The ref counting is done on either thread:
        // - worker context's thread: held by the tasks
        // - main thread: held by MainThreadBridgeBase
        // Therefore, this must be a ThreadSafeRefCounted.
        RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;

        // Used on the worker context thread.
        RefPtr<WorkerLoaderProxy> m_loaderProxy;
    };

    class MainThreadAsyncBridge final : public MainThreadBridgeBase {
    public:
        MainThreadAsyncBridge(WorkerGlobalScope&, PassRefPtr<ThreadableLoaderClientWrapper>, const ResourceRequest&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, const ReferrerPolicy, const String& outgoingReferrer);

    private:
        ~MainThreadAsyncBridge() override;

        void forwardTaskToWorker(PassOwnPtr<ExecutionContextTask>) override;
        void forwardTaskToWorkerOnLoaderDone(PassOwnPtr<ExecutionContextTask>) override;
    };

    class MainThreadSyncBridge final : public MainThreadBridgeBase {
    public:
        MainThreadSyncBridge(WorkerGlobalScope&, PassRefPtr<ThreadableLoaderClientWrapper>, const ResourceRequest&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, const ReferrerPolicy, const String& outgoingReferrer);

    private:
        ~MainThreadSyncBridge() override;

        void forwardTaskToWorker(PassOwnPtr<ExecutionContextTask>) override;
        void forwardTaskToWorkerOnLoaderDone(PassOwnPtr<ExecutionContextTask>) override;

        bool m_done;
        OwnPtr<WebWaitableEvent> m_loaderDoneEvent;
        // Thread-safety: |m_clientTasks| can be written (i.e. Closures are added)
        // on the main thread only before |m_loaderDoneEvent| is signaled and can be read
        // on the worker context thread only after |m_loaderDoneEvent| is signaled.
        Vector<OwnPtr<ExecutionContextTask>> m_clientTasks;
        Mutex m_lock;
    };

    WorkerThreadableLoader(WorkerGlobalScope&, ThreadableLoaderClient*, const ResourceRequest&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, BlockingBehavior);

    void didReceiveResourceTiming(const ResourceTimingInfo&) override;

    RefPtrWillBePersistent<WorkerGlobalScope> m_workerGlobalScope;
    RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;

    MainThreadBridgeBase* m_bridge;
};

} // namespace blink

#endif // WorkerThreadableLoader_h
