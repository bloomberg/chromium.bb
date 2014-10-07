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

#ifndef WebSharedWorkerImpl_h
#define WebSharedWorkerImpl_h

#include "public/web/WebSharedWorker.h"

#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerScriptLoaderClient.h"
#include "core/workers/WorkerThread.h"
#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebSharedWorkerClient.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/WeakPtr.h"

namespace blink {

class ConsoleMessage;
class ResourceResponse;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebWorkerClient;
class WebSecurityOrigin;
class WebString;
class WebURL;
class WebView;
class WebWorker;
class WebSharedWorkerClient;
class WorkerInspectorProxy;

// This class is used by the worker process code to talk to the SharedWorker implementation.
// It can't use it directly since it uses WebKit types, so this class converts the data types.
// When the SharedWorker object wants to call WorkerReportingProxy, this class will
// convert to Chrome data types first and then call the supplied WebCommonWorkerClient.
class WebSharedWorkerImpl final
    : public WorkerReportingProxy
    , public WorkerLoaderProxy
    , public WebFrameClient
    , public WebSharedWorker
    , public WebDevToolsAgentClient {
public:
    explicit WebSharedWorkerImpl(WebSharedWorkerClient*);

    // WorkerReportingProxy methods:
    virtual void reportException(
        const WTF::String&, int, int, const WTF::String&) override;
    virtual void reportConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage>) override;
    virtual void postMessageToPageInspector(const WTF::String&) override;
    virtual void updateInspectorStateCookie(const WTF::String&) override;
    virtual void workerGlobalScopeStarted(WorkerGlobalScope*) override;
    virtual void workerGlobalScopeClosed() override;
    virtual void workerThreadTerminated() override;
    virtual void willDestroyWorkerGlobalScope() override { }

    // WorkerLoaderProxy methods:
    virtual void postTaskToLoader(PassOwnPtr<ExecutionContextTask>) override;
    virtual bool postTaskToWorkerGlobalScope(PassOwnPtr<ExecutionContextTask>) override;

    // WebFrameClient methods to support resource loading thru the 'shadow page'.
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebLocalFrame*, WebApplicationCacheHostClient*) override;
    virtual void didFinishDocumentLoad(WebLocalFrame*) override;

    // WebDevToolsAgentClient overrides.
    virtual void sendMessageToInspectorFrontend(const WebString&) override;
    virtual void saveAgentRuntimeState(const WebString&) override;
    virtual void resumeStartup() override;

    // WebSharedWorker methods:
    virtual void startWorkerContext(const WebURL&, const WebString& name, const WebString& contentSecurityPolicy, WebContentSecurityPolicyType) override;
    virtual void connect(WebMessagePortChannel*) override;
    virtual void terminateWorkerContext() override;
    virtual void clientDestroyed() override;

    virtual void pauseWorkerContextOnStart() override;
    virtual void resumeWorkerContext() override;
    virtual void attachDevTools(const WebString& hostId) override;
    virtual void reattachDevTools(const WebString& hostId, const WebString& savedState) override;
    virtual void detachDevTools() override;
    virtual void dispatchDevToolsMessage(const WebString&) override;

private:
    class Loader;

    virtual ~WebSharedWorkerImpl();

    WebSharedWorkerClient* client() { return m_client->get(); }

    void setWorkerThread(PassRefPtr<WorkerThread> thread) { m_workerThread = thread; }
    WorkerThread* workerThread() { return m_workerThread.get(); }

    // Shuts down the worker thread.
    void stopWorkerThread();

    // Creates the shadow loader used for worker network requests.
    void initializeLoader();

    void loadShadowPage();
    void didReceiveScriptLoaderResponse();
    void onScriptLoaderFinished();

    static void connectTask(ExecutionContext*, PassOwnPtr<WebMessagePortChannel>);
    // Tasks that are run on the main thread.
    void workerGlobalScopeClosedOnMainThread();
    void workerThreadTerminatedOnMainThread();

    void postMessageToPageInspectorOnMainThread(const String& message);

    // 'shadow page' - created to proxy loading requests from the worker.
    RefPtrWillBePersistent<ExecutionContext> m_loadingDocument;
    WebView* m_webView;
    WebFrame* m_mainFrame;
    bool m_askedToTerminate;

    OwnPtr<WorkerInspectorProxy> m_workerInspectorProxy;

    RefPtr<WorkerThread> m_workerThread;

    // This one's initialized and bound to the main thread.
    RefPtr<WeakReference<WebSharedWorkerClient> > m_client;

    // Usually WeakPtr is created by WeakPtrFactory exposed by Client
    // class itself, but here it's implemented by Chrome so we create
    // our own WeakPtr.
    WeakPtr<WebSharedWorkerClient> m_clientWeakPtr;

    bool m_pauseWorkerContextOnStart;
    bool m_isPausedOnStart;

    // Kept around only while main script loading is ongoing.
    OwnPtr<Loader> m_mainScriptLoader;
    WebURL m_url;
    WebString m_name;
    WebString m_contentSecurityPolicy;
    WebContentSecurityPolicyType m_policyType;
};

} // namespace blink

#endif
