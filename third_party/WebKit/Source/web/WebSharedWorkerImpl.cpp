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

#include "web/WebSharedWorkerImpl.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Persistent.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebDevToolsAgent.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "public/web/WebWorkerContentSettingsClientProxy.h"
#include "public/web/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "web/IndexedDBClientImpl.h"
#include "web/LocalFileSystemClient.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WorkerContentSettingsClient.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"

namespace blink {

// TODO(toyoshim): Share implementation with WebEmbeddedWorkerImpl as much as
// possible.

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : m_webView(nullptr),
      m_mainFrame(nullptr),
      m_askedToTerminate(false),
      m_workerInspectorProxy(WorkerInspectorProxy::create()),
      m_client(client),
      m_pauseWorkerContextOnStart(false),
      m_isPausedOnStart(false),
      m_creationAddressSpace(WebAddressSpacePublic) {
  DCHECK(isMainThread());
}

WebSharedWorkerImpl::~WebSharedWorkerImpl() {
  DCHECK(isMainThread());
  DCHECK(m_webView);
  // Detach the client before closing the view to avoid getting called back.
  m_mainFrame->setClient(0);

  m_webView->close();
  m_mainFrame->close();
  if (m_loaderProxy)
    m_loaderProxy->detachProvider(this);
}

void WebSharedWorkerImpl::terminateWorkerThread() {
  DCHECK(isMainThread());
  if (m_askedToTerminate)
    return;
  m_askedToTerminate = true;
  if (m_mainScriptLoader) {
    m_mainScriptLoader->cancel();
    m_mainScriptLoader.clear();
    m_client->workerScriptLoadFailed();
    delete this;
    return;
  }
  if (m_workerThread)
    m_workerThread->terminate();
  m_workerInspectorProxy->workerThreadTerminated();
}

void WebSharedWorkerImpl::initializeLoader() {
  DCHECK(isMainThread());

  // Create 'shadow page'. This page is never displayed, it is used to proxy the
  // loading requests from the worker context to the rest of WebKit and Chromium
  // infrastructure.
  DCHECK(!m_webView);
  m_webView = WebView::create(nullptr, WebPageVisibilityStateVisible);
  // FIXME: http://crbug.com/363843. This needs to find a better way to
  // not create graphics layers.
  m_webView->settings()->setAcceleratedCompositingEnabled(false);
  // FIXME: Settings information should be passed to the Worker process from
  // Browser process when the worker is created (similar to
  // RenderThread::OnCreateNewView).
  m_mainFrame = toWebLocalFrameImpl(
      WebLocalFrame::create(WebTreeScopeType::Document, this,
                            Platform::current()->interfaceProvider(), nullptr));
  m_webView->setMainFrame(m_mainFrame.get());
  m_mainFrame->setDevToolsAgentClient(this);

  // If we were asked to pause worker context on start and wait for debugger
  // then it is the good time to do that.
  m_client->workerReadyForInspection();
  if (m_pauseWorkerContextOnStart) {
    m_isPausedOnStart = true;
    return;
  }
  loadShadowPage();
}

WebApplicationCacheHost* WebSharedWorkerImpl::createApplicationCacheHost(
    WebApplicationCacheHostClient* appcacheHostClient) {
  DCHECK(isMainThread());
  return m_client->createApplicationCacheHost(appcacheHostClient);
}

void WebSharedWorkerImpl::loadShadowPage() {
  DCHECK(isMainThread());

  // Construct substitute data source for the 'shadow page'. We only need it
  // to have same origin as the worker so the loading checks work correctly.
  CString content("");
  RefPtr<SharedBuffer> buffer(
      SharedBuffer::create(content.data(), content.length()));
  m_mainFrame->frame()->loader().load(
      FrameLoadRequest(0, ResourceRequest(m_url),
                       SubstituteData(buffer, "text/html", "UTF-8", KURL())));
}

void WebSharedWorkerImpl::willSendRequest(WebLocalFrame* frame,
                                          WebURLRequest& request) {
  DCHECK(isMainThread());
  if (m_networkProvider)
    m_networkProvider->willSendRequest(frame->dataSource(), request);
}

void WebSharedWorkerImpl::didFinishDocumentLoad(WebLocalFrame* frame) {
  DCHECK(isMainThread());
  DCHECK(!m_loadingDocument);
  DCHECK(!m_mainScriptLoader);
  m_networkProvider = WTF::wrapUnique(
      m_client->createServiceWorkerNetworkProvider(frame->dataSource()));
  m_mainScriptLoader = WorkerScriptLoader::create();
  m_mainScriptLoader->setRequestContext(
      WebURLRequest::RequestContextSharedWorker);
  m_loadingDocument = toWebLocalFrameImpl(frame)->frame()->document();

  CrossOriginRequestPolicy crossOriginRequestPolicy =
      (static_cast<KURL>(m_url)).protocolIsData() ? AllowCrossOriginRequests
                                                  : DenyCrossOriginRequests;

  m_mainScriptLoader->loadAsynchronously(
      *m_loadingDocument.get(), m_url, crossOriginRequestPolicy,
      m_creationAddressSpace,
      bind(&WebSharedWorkerImpl::didReceiveScriptLoaderResponse,
           WTF::unretained(this)),
      bind(&WebSharedWorkerImpl::onScriptLoaderFinished,
           WTF::unretained(this)));
  // Do nothing here since onScriptLoaderFinished() might have been already
  // invoked and |this| might have been deleted at this point.
}

bool WebSharedWorkerImpl::isControlledByServiceWorker(
    WebDataSource& dataSource) {
  DCHECK(isMainThread());
  return m_networkProvider &&
         m_networkProvider->isControlledByServiceWorker(dataSource);
}

int64_t WebSharedWorkerImpl::serviceWorkerID(WebDataSource& dataSource) {
  DCHECK(isMainThread());
  if (!m_networkProvider)
    return -1;
  return m_networkProvider->serviceWorkerID(dataSource);
}

void WebSharedWorkerImpl::sendProtocolMessage(int sessionId,
                                              int callId,
                                              const WebString& message,
                                              const WebString& state) {
  DCHECK(isMainThread());
  m_client->sendDevToolsMessage(sessionId, callId, message, state);
}

void WebSharedWorkerImpl::resumeStartup() {
  DCHECK(isMainThread());
  bool isPausedOnStart = m_isPausedOnStart;
  m_isPausedOnStart = false;
  if (isPausedOnStart)
    loadShadowPage();
}

WebDevToolsAgentClient::WebKitClientMessageLoop*
WebSharedWorkerImpl::createClientMessageLoop() {
  DCHECK(isMainThread());
  return m_client->createDevToolsMessageLoop();
}

void WebSharedWorkerImpl::countFeature(UseCounter::Feature feature) {
  DCHECK(isMainThread());
  m_client->countFeature(feature);
}

void WebSharedWorkerImpl::postMessageToPageInspector(const String& message) {
  DCHECK(isMainThread());
  m_workerInspectorProxy->dispatchMessageFromWorker(message);
}

void WebSharedWorkerImpl::didCloseWorkerGlobalScope() {
  DCHECK(isMainThread());
  m_client->workerContextClosed();
  terminateWorkerThread();
}

void WebSharedWorkerImpl::didTerminateWorkerThread() {
  DCHECK(isMainThread());
  m_client->workerContextDestroyed();
  // The lifetime of this proxy is controlled by the worker context.
  delete this;
}

// WorkerLoaderProxyProvider -------------------------------------------------

void WebSharedWorkerImpl::postTaskToLoader(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  DCHECK(m_workerThread->isCurrentThread());
  m_parentFrameTaskRunners->get(TaskType::Networking)
      ->postTask(FROM_HERE, std::move(task));
}

void WebSharedWorkerImpl::postTaskToWorkerGlobalScope(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  DCHECK(isMainThread());
  m_workerThread->postTask(location, std::move(task));
}

ThreadableLoadingContext* WebSharedWorkerImpl::getThreadableLoadingContext() {
  if (!m_loadingContext) {
    m_loadingContext =
        ThreadableLoadingContext::create(*toDocument(m_loadingDocument.get()));
  }
  return m_loadingContext;
}

void WebSharedWorkerImpl::connect(WebMessagePortChannel* webChannel) {
  DCHECK(isMainThread());
  workerThread()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&WebSharedWorkerImpl::connectTaskOnWorkerThread,
                      WTF::crossThreadUnretained(this),
                      WTF::passed(WebMessagePortChannelUniquePtr(webChannel))));
}

void WebSharedWorkerImpl::connectTaskOnWorkerThread(
    WebMessagePortChannelUniquePtr channel) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  DCHECK(m_workerThread->isCurrentThread());
  WorkerGlobalScope* workerGlobalScope =
      toWorkerGlobalScope(m_workerThread->globalScope());
  MessagePort* port = MessagePort::create(*workerGlobalScope);
  port->entangle(std::move(channel));
  SECURITY_DCHECK(workerGlobalScope->isSharedWorkerGlobalScope());
  workerGlobalScope->dispatchEvent(createConnectEvent(port));
}

void WebSharedWorkerImpl::startWorkerContext(
    const WebURL& url,
    const WebString& name,
    const WebString& contentSecurityPolicy,
    WebContentSecurityPolicyType policyType,
    WebAddressSpace creationAddressSpace) {
  DCHECK(isMainThread());
  m_url = url;
  m_name = name;
  m_creationAddressSpace = creationAddressSpace;
  initializeLoader();
}

void WebSharedWorkerImpl::didReceiveScriptLoaderResponse() {
  DCHECK(isMainThread());
  probe::didReceiveScriptResponse(m_loadingDocument,
                                  m_mainScriptLoader->identifier());
  m_client->selectAppCacheID(m_mainScriptLoader->appCacheID());
}

void WebSharedWorkerImpl::onScriptLoaderFinished() {
  DCHECK(isMainThread());
  DCHECK(m_loadingDocument);
  DCHECK(m_mainScriptLoader);
  if (m_askedToTerminate)
    return;
  if (m_mainScriptLoader->failed()) {
    m_mainScriptLoader->cancel();
    m_client->workerScriptLoadFailed();

    // The SharedWorker was unable to load the initial script, so
    // shut it down right here.
    delete this;
    return;
  }

  Document* document = m_mainFrame->frame()->document();
  // FIXME: this document's origin is pristine and without any extra privileges.
  // (crbug.com/254993)
  SecurityOrigin* starterOrigin = document->getSecurityOrigin();

  WorkerClients* workerClients = WorkerClients::create();
  provideLocalFileSystemToWorker(workerClients,
                                 LocalFileSystemClient::create());
  WebSecurityOrigin webSecurityOrigin(m_loadingDocument->getSecurityOrigin());
  provideContentSettingsClientToWorker(
      workerClients,
      WTF::wrapUnique(
          m_client->createWorkerContentSettingsClientProxy(webSecurityOrigin)));
  provideIndexedDBClientToWorker(workerClients,
                                 IndexedDBClientImpl::create(*workerClients));
  ContentSecurityPolicy* contentSecurityPolicy =
      m_mainScriptLoader->releaseContentSecurityPolicy();
  WorkerThreadStartMode startMode =
      m_workerInspectorProxy->workerStartMode(document);
  std::unique_ptr<WorkerSettings> workerSettings =
      WTF::wrapUnique(new WorkerSettings(document->settings()));
  std::unique_ptr<WorkerThreadStartupData> startupData =
      WorkerThreadStartupData::create(
          m_url, m_loadingDocument->userAgent(), m_mainScriptLoader->script(),
          nullptr, startMode,
          contentSecurityPolicy ? contentSecurityPolicy->headers().get()
                                : nullptr,
          m_mainScriptLoader->getReferrerPolicy(), starterOrigin, workerClients,
          m_mainScriptLoader->responseAddressSpace(),
          m_mainScriptLoader->originTrialTokens(), std::move(workerSettings),
          WorkerV8Settings::Default());

  // SharedWorker can sometimes run tasks that are initiated by/associated with
  // a document's frame but these documents can be from a different process. So
  // we intentionally populate the task runners with null document in order to
  // use the thread's default task runner. Note that |m_document| should not be
  // used as it's a dummy document for loading that doesn't represent the frame
  // of any associated document.
  m_parentFrameTaskRunners = ParentFrameTaskRunners::create(nullptr);

  m_loaderProxy = WorkerLoaderProxy::create(this);
  m_reportingProxy = new WebSharedWorkerReportingProxyImpl(
      this, m_parentFrameTaskRunners.get());
  m_workerThread =
      SharedWorkerThread::create(m_name, m_loaderProxy, *m_reportingProxy);
  probe::scriptImported(m_loadingDocument, m_mainScriptLoader->identifier(),
                        m_mainScriptLoader->script());
  m_mainScriptLoader.clear();

  workerThread()->start(std::move(startupData), m_parentFrameTaskRunners.get());
  m_workerInspectorProxy->workerThreadCreated(toDocument(m_loadingDocument),
                                              workerThread(), m_url);
  m_client->workerScriptLoaded();
}

void WebSharedWorkerImpl::terminateWorkerContext() {
  DCHECK(isMainThread());
  terminateWorkerThread();
}

void WebSharedWorkerImpl::pauseWorkerContextOnStart() {
  m_pauseWorkerContextOnStart = true;
}

void WebSharedWorkerImpl::attachDevTools(const WebString& hostId,
                                         int sessionId) {
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->attach(hostId, sessionId);
}

void WebSharedWorkerImpl::reattachDevTools(const WebString& hostId,
                                           int sessionId,
                                           const WebString& savedState) {
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->reattach(hostId, sessionId, savedState);
  resumeStartup();
}

void WebSharedWorkerImpl::detachDevTools() {
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->detach();
}

void WebSharedWorkerImpl::dispatchDevToolsMessage(int sessionId,
                                                  int callId,
                                                  const WebString& method,
                                                  const WebString& message) {
  if (m_askedToTerminate)
    return;
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->dispatchOnInspectorBackend(sessionId, callId, method,
                                              message);
}

WebSharedWorker* WebSharedWorker::create(WebSharedWorkerClient* client) {
  return new WebSharedWorkerImpl(client);
}

}  // namespace blink
