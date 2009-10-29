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

#ifndef WebWorkerImpl_h
#define WebWorkerImpl_h

#include "WebWorker.h"

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class WorkerThread;
}

namespace WebKit {
class WebView;

// This class is used by the worker process code to talk to the WebCore::Worker
// implementation.  It can't use it directly since it uses WebKit types, so this
// class converts the data types.  When the WebCore::Worker object wants to call
// WebCore::WorkerObjectProxy, this class will conver to Chrome data types first
// and then call the supplied WebWorkerClient.
class WebWorkerImpl : public WebCore::WorkerObjectProxy
                    , public WebCore::WorkerLoaderProxy
                    , public WebWorker {
public:
    explicit WebWorkerImpl(WebWorkerClient* client);

    // WebCore::WorkerObjectProxy methods:
    virtual void postMessageToWorkerObject(
        PassRefPtr<WebCore::SerializedScriptValue>,
        PassOwnPtr<WebCore::MessagePortChannelArray>);
    virtual void postExceptionToWorkerObject(
        const WebCore::String&, int, const WebCore::String&);
    virtual void postConsoleMessageToWorkerObject(
        WebCore::MessageDestination, WebCore::MessageSource, WebCore::MessageType,
        WebCore::MessageLevel, const WebCore::String&, int, const WebCore::String&);
    virtual void confirmMessageFromWorkerObject(bool);
    virtual void reportPendingActivity(bool);
    virtual void workerContextDestroyed();

    // WebCore::WorkerLoaderProxy methods:
    virtual void postTaskToLoader(PassRefPtr<WebCore::ScriptExecutionContext::Task>);
    virtual void postTaskForModeToWorkerContext(
        PassRefPtr<WebCore::ScriptExecutionContext::Task>, const WebCore::String& mode);

    // WebWorker methods:
    virtual void startWorkerContext(const WebURL&, const WebString&, const WebString&);
    virtual void terminateWorkerContext();
    virtual void postMessageToWorkerContext(const WebString&, const WebMessagePortChannelArray&);
    virtual void workerObjectDestroyed();
    virtual void clientDestroyed();

    WebWorkerClient* client() {return m_client;}

    // Executes the given task on the main thread.
    static void dispatchTaskToMainThread(PassRefPtr<WebCore::ScriptExecutionContext::Task>);

private:
    virtual ~WebWorkerImpl();

    // Tasks that are run on the worker thread.
    static void postMessageToWorkerContextTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        const WebCore::String& message,
        PassOwnPtr<WebCore::MessagePortChannelArray> channels);

    // Function used to invoke tasks on the main thread.
    static void invokeTaskMethod(void*);

    // Tasks that are run on the main thread.
    static void postMessageTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        WebCore::String message,
        PassOwnPtr<WebCore::MessagePortChannelArray> channels);
    static void postExceptionTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        const WebCore::String& message,
        int lineNumber,
        const WebCore::String& sourceURL);
    static void postConsoleMessageTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        int destination,
        int source,
        int type,
        int level,
        const WebCore::String& message,
        int lineNumber,
        const WebCore::String& sourceURL);
    static void confirmMessageTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        bool hasPendingActivity);
    static void reportPendingActivityTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        bool hasPendingActivity);
    static void workerContextDestroyedTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr);

    WebWorkerClient* m_client;

    // 'shadow page' - created to proxy loading requests from the worker.
    RefPtr<WebCore::ScriptExecutionContext> m_loadingDocument;
    WebView* m_webView;
    bool m_askedToTerminate;

    RefPtr<WebCore::WorkerThread> m_workerThread;
};

} // namespace WebKit

#endif // ENABLE(WORKERS)

#endif
