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

#include <memory>
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebContentSecurityPolicy.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebSharedWorkerClient.h"
#include "web/WebSharedWorkerReportingProxyImpl.h"
#include "wtf/RefPtr.h"

namespace blink {

class ParentFrameTaskRunners;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebLocalFrameImpl;
class WebServiceWorkerNetworkProvider;
class WebSharedWorkerClient;
class WebString;
class WebURL;
class WebView;
class WorkerInspectorProxy;
class WorkerScriptLoader;

// This class is used by the worker process code to talk to the SharedWorker
// implementation. This is basically accessed on the main thread, but some
// methods must be called from a worker thread. Such methods are suffixed with
// *OnWorkerThread or have header comments.
class WebSharedWorkerImpl final : public WebFrameClient,
                                  public WebSharedWorker,
                                  public WebDevToolsAgentClient,
                                  private WorkerLoaderProxyProvider {
 public:
  explicit WebSharedWorkerImpl(WebSharedWorkerClient*);

  // WebFrameClient methods to support resource loading thru the 'shadow page'.
  WebApplicationCacheHost* createApplicationCacheHost(
      WebApplicationCacheHostClient*) override;
  void willSendRequest(WebLocalFrame*, WebURLRequest&) override;
  void didFinishDocumentLoad(WebLocalFrame*) override;
  bool isControlledByServiceWorker(WebDataSource&) override;
  int64_t serviceWorkerID(WebDataSource&) override;

  // WebDevToolsAgentClient overrides.
  void sendProtocolMessage(int sessionId,
                           int callId,
                           const WebString&,
                           const WebString&) override;
  void resumeStartup() override;
  WebDevToolsAgentClient::WebKitClientMessageLoop* createClientMessageLoop()
      override;

  // WebSharedWorker methods:
  void startWorkerContext(const WebURL&,
                          const WebString& name,
                          const WebString& contentSecurityPolicy,
                          WebContentSecurityPolicyType,
                          WebAddressSpace) override;
  void connect(WebMessagePortChannel*) override;
  void terminateWorkerContext() override;

  void pauseWorkerContextOnStart() override;
  void attachDevTools(const WebString& hostId, int sessionId) override;
  void reattachDevTools(const WebString& hostId,
                        int sesionId,
                        const WebString& savedState) override;
  void detachDevTools() override;
  void dispatchDevToolsMessage(int sessionId,
                               int callId,
                               const WebString& method,
                               const WebString& message) override;

  // Callback methods for WebSharedWorkerReportingProxyImpl.
  void countFeature(UseCounter::Feature);
  void postMessageToPageInspector(const String& message);
  void didCloseWorkerGlobalScope();
  void didTerminateWorkerThread();

 private:
  ~WebSharedWorkerImpl() override;

  WorkerThread* workerThread() { return m_workerThread.get(); }

  // Shuts down the worker thread.
  void terminateWorkerThread();

  // Creates the shadow loader used for worker network requests.
  void initializeLoader();

  void loadShadowPage();
  void didReceiveScriptLoaderResponse();
  void onScriptLoaderFinished();

  void connectTaskOnWorkerThread(WebMessagePortChannelUniquePtr);

  // WorkerLoaderProxyProvider
  // postTaskToLoader() must be called from a worker thread.
  void postTaskToLoader(const WebTraceLocation&,
                        std::unique_ptr<WTF::CrossThreadClosure>) override;
  void postTaskToWorkerGlobalScope(
      const WebTraceLocation&,
      std::unique_ptr<WTF::CrossThreadClosure>) override;
  ThreadableLoadingContext* getThreadableLoadingContext() override;

  // 'shadow page' - created to proxy loading requests from the worker.
  // Will be accessed by worker thread when posting tasks.
  Persistent<ExecutionContext> m_loadingDocument;
  Persistent<ThreadableLoadingContext> m_loadingContext;
  WebView* m_webView;
  Persistent<WebLocalFrameImpl> m_mainFrame;
  bool m_askedToTerminate;

  std::unique_ptr<WebServiceWorkerNetworkProvider> m_networkProvider;

  Persistent<WorkerInspectorProxy> m_workerInspectorProxy;

  // Owned by the main thread, but will be accessed by the worker.
  CrossThreadPersistent<ParentFrameTaskRunners> m_parentFrameTaskRunners;

  Persistent<WebSharedWorkerReportingProxyImpl> m_reportingProxy;
  std::unique_ptr<WorkerThread> m_workerThread;

  WebSharedWorkerClient* m_client;

  bool m_pauseWorkerContextOnStart;
  bool m_isPausedOnStart;

  // Kept around only while main script loading is ongoing.
  RefPtr<WorkerScriptLoader> m_mainScriptLoader;

  RefPtr<WorkerLoaderProxy> m_loaderProxy;

  WebURL m_url;
  WebString m_name;
  WebAddressSpace m_creationAddressSpace;
};

}  // namespace blink

#endif  // WebSharedWorkerImpl_h
