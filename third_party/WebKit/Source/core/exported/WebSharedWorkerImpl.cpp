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

#include "core/exported/WebSharedWorkerImpl.h"

#include <memory>
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/CoreInitializer.h"
#include "core/dom/Document.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/loader/WorkerFetchContext.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/SharedWorkerContentSettingsProxy.h"
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClassicScriptLoader.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Persistent.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Functional.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebWorkerFetchContext.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "public/web/WebSettings.h"
#include "public/web/devtools_agent.mojom-blink.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"

namespace blink {

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : worker_inspector_proxy_(WorkerInspectorProxy::Create()),
      client_(client),
      creation_address_space_(mojom::IPAddressSpace::kPublic) {
  DCHECK(IsMainThread());
}

WebSharedWorkerImpl::~WebSharedWorkerImpl() {
  DCHECK(IsMainThread());
}

void WebSharedWorkerImpl::TerminateWorkerThread() {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;
  if (shadow_page_ && !shadow_page_->WasInitialized()) {
    client_->WorkerScriptLoadFailed();
    delete this;
    return;
  }
  if (main_script_loader_) {
    main_script_loader_->Cancel();
    main_script_loader_ = nullptr;
    client_->WorkerScriptLoadFailed();
    delete this;
    return;
  }
  if (worker_thread_)
    worker_thread_->Terminate();
  worker_inspector_proxy_->WorkerThreadTerminated();
}

std::unique_ptr<WebApplicationCacheHost>
WebSharedWorkerImpl::CreateApplicationCacheHost(
    WebApplicationCacheHostClient* appcache_host_client) {
  DCHECK(IsMainThread());
  return client_->CreateApplicationCacheHost(appcache_host_client);
}

void WebSharedWorkerImpl::OnShadowPageInitialized() {
  DCHECK(IsMainThread());
  DCHECK(!main_script_loader_);
  shadow_page_->DocumentLoader()->SetServiceWorkerNetworkProvider(
      client_->CreateServiceWorkerNetworkProvider());
  main_script_loader_ = WorkerClassicScriptLoader::Create();

  network::mojom::FetchRequestMode fetch_request_mode =
      network::mojom::FetchRequestMode::kSameOrigin;
  network::mojom::FetchCredentialsMode fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kSameOrigin;
  if ((static_cast<KURL>(url_)).ProtocolIsData()) {
    fetch_request_mode = network::mojom::FetchRequestMode::kNoCORS;
    fetch_credentials_mode = network::mojom::FetchCredentialsMode::kInclude;
  }

  main_script_loader_->LoadAsynchronously(
      *shadow_page_->GetDocument(), url_,
      WebURLRequest::kRequestContextSharedWorker, fetch_request_mode,
      fetch_credentials_mode, creation_address_space_,
      Bind(&WebSharedWorkerImpl::DidReceiveScriptLoaderResponse,
           WTF::Unretained(this)),
      Bind(&WebSharedWorkerImpl::OnScriptLoaderFinished,
           WTF::Unretained(this)));
  // Do nothing here since OnScriptLoaderFinished() might have been already
  // invoked and |this| might have been deleted at this point.
}

void WebSharedWorkerImpl::ResumeStartup() {
  DCHECK(IsMainThread());
  bool is_paused_on_start = is_paused_on_start_;
  is_paused_on_start_ = false;
  if (is_paused_on_start) {
    // We'll continue in OnShadowPageInitialized().
    shadow_page_->Initialize(url_);
  }
}

const base::UnguessableToken& WebSharedWorkerImpl::GetDevToolsWorkerToken() {
  return devtools_worker_token_;
}

void WebSharedWorkerImpl::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  client_->CountFeature(feature);
}

void WebSharedWorkerImpl::PostMessageToPageInspector(int session_id,
                                                     const String& message) {
  DCHECK(IsMainThread());
  worker_inspector_proxy_->DispatchMessageFromWorker(session_id, message);
}

void WebSharedWorkerImpl::DidCloseWorkerGlobalScope() {
  DCHECK(IsMainThread());
  client_->WorkerContextClosed();
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::DidTerminateWorkerThread() {
  DCHECK(IsMainThread());
  client_->WorkerContextDestroyed();
  // The lifetime of this proxy is controlled by the worker context.
  delete this;
}

void WebSharedWorkerImpl::Connect(MessagePortChannel web_channel) {
  DCHECK(IsMainThread());
  // The HTML spec requires to queue a connect event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/multipage/workers.html#shared-workers-and-the-sharedworker-interface
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBind(&WebSharedWorkerImpl::ConnectTaskOnWorkerThread,
                      WTF::CrossThreadUnretained(this),
                      WTF::Passed(std::move(web_channel))));
}

void WebSharedWorkerImpl::ConnectTaskOnWorkerThread(
    MessagePortChannel channel) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  DCHECK(worker_thread_->IsCurrentThread());
  WorkerGlobalScope* worker_global_scope =
      ToWorkerGlobalScope(worker_thread_->GlobalScope());
  MessagePort* port = MessagePort::Create(*worker_global_scope);
  port->Entangle(std::move(channel));
  SECURITY_DCHECK(worker_global_scope->IsSharedWorkerGlobalScope());
  worker_global_scope->DispatchEvent(CreateConnectEvent(port));
}

void WebSharedWorkerImpl::StartWorkerContext(
    const WebURL& url,
    const WebString& name,
    const WebString& content_security_policy,
    WebContentSecurityPolicyType policy_type,
    mojom::IPAddressSpace creation_address_space,
    const base::UnguessableToken& devtools_worker_token,
    mojo::ScopedMessagePipeHandle content_settings_handle,
    mojo::ScopedMessagePipeHandle interface_provider) {
  DCHECK(IsMainThread());
  url_ = url;
  name_ = name;
  creation_address_space_ = creation_address_space;
  // Chrome doesn't use interface versioning.
  content_settings_info_ = mojom::blink::WorkerContentSettingsProxyPtrInfo(
      std::move(content_settings_handle), 0u);
  pending_interface_provider_.set_handle(std::move(interface_provider));

  devtools_worker_token_ = devtools_worker_token;
  shadow_page_ = std::make_unique<WorkerShadowPage>(this);

  // If we were asked to pause worker context on start and wait for debugger
  // then now is a good time to do that.
  client_->WorkerReadyForInspection();
  if (pause_worker_context_on_start_) {
    is_paused_on_start_ = true;
    return;
  }

  // We'll continue in OnShadowPageInitialized().
  shadow_page_->Initialize(url_);
}

void WebSharedWorkerImpl::DidReceiveScriptLoaderResponse() {
  DCHECK(IsMainThread());
  probe::didReceiveScriptResponse(shadow_page_->GetDocument(),
                                  main_script_loader_->Identifier());
  client_->SelectAppCacheID(main_script_loader_->AppCacheID());
}

void WebSharedWorkerImpl::OnScriptLoaderFinished() {
  DCHECK(IsMainThread());
  DCHECK(main_script_loader_);
  if (asked_to_terminate_)
    return;
  if (main_script_loader_->Failed()) {
    main_script_loader_->Cancel();
    client_->WorkerScriptLoadFailed();

    // The SharedWorker was unable to load the initial script, so
    // shut it down right here.
    delete this;
    return;
  }

  // FIXME: this document's origin is pristine and without any extra privileges
  // (e.g. GrantUniversalAccess) that can be overriden in regular documents
  // via WebPreference by embedders. (crbug.com/254993)
  Document* document = shadow_page_->GetDocument();
  const SecurityOrigin* starter_origin = document->GetSecurityOrigin();
  bool starter_secure_context = document->IsSecureContext();

  WorkerClients* worker_clients = WorkerClients::Create();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  CoreInitializer::GetInstance().ProvideIndexedDBClientToWorker(
      *worker_clients);

  ProvideContentSettingsClientToWorker(
      worker_clients, std::make_unique<SharedWorkerContentSettingsProxy>(
                          std::move(content_settings_info_)));

  std::unique_ptr<WebWorkerFetchContext> web_worker_fetch_context =
      client_->CreateWorkerFetchContext(
          shadow_page_->DocumentLoader()->GetServiceWorkerNetworkProvider());
  DCHECK(web_worker_fetch_context);
  web_worker_fetch_context->SetApplicationCacheHostID(
      shadow_page_->GetDocument()
          ->Fetcher()
          ->Context()
          .ApplicationCacheHostID());
  ProvideWorkerFetchContextToWorker(worker_clients,
                                    std::move(web_worker_fetch_context));

  ContentSecurityPolicy* content_security_policy =
      main_script_loader_->ReleaseContentSecurityPolicy();
  ReferrerPolicy referrer_policy = kReferrerPolicyDefault;
  if (!main_script_loader_->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        main_script_loader_->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  }
  auto worker_settings = std::make_unique<WorkerSettings>(
      shadow_page_->GetDocument()->GetFrame()->GetSettings());

  // TODO(nhiroki); Set the coordinator for module fetch.
  // (https://crbug.com/680046)
  WorkerOrWorkletModuleFetchCoordinator* module_fetch_coordinator = nullptr;

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          url_, document->UserAgent(),
          content_security_policy ? content_security_policy->Headers().get()
                                  : nullptr,
          referrer_policy, starter_origin, starter_secure_context,
          worker_clients, main_script_loader_->ResponseAddressSpace(),
          main_script_loader_->OriginTrialTokens(), devtools_worker_token_,
          std::move(worker_settings), kV8CacheOptionsDefault,
          module_fetch_coordinator, std::move(pending_interface_provider_));
  String source_code = main_script_loader_->SourceText();

  // SharedWorker can sometimes run tasks that are initiated by/associated with
  // a document's frame but these documents can be from a different process. So
  // we intentionally populate the task runners with default task runners of the
  // main thread. Note that |m_document| should not be used as it's a dummy
  // document for loading that doesn't represent the frame of any associated
  // document.
  ParentFrameTaskRunners* task_runners = ParentFrameTaskRunners::Create();

  reporting_proxy_ = new SharedWorkerReportingProxy(this, task_runners);
  worker_thread_ = std::make_unique<SharedWorkerThread>(
      name_, ThreadableLoadingContext::Create(*document), *reporting_proxy_);
  probe::scriptImported(document, main_script_loader_->Identifier(),
                        main_script_loader_->SourceText());
  main_script_loader_ = nullptr;

  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;

  GetWorkerThread()->Start(
      std::move(global_scope_creation_params), thread_startup_data,
      worker_inspector_proxy_->ShouldPauseOnWorkerStart(document),
      task_runners);
  worker_inspector_proxy_->WorkerThreadCreated(document, GetWorkerThread(),
                                               url_);
  // TODO(nhiroki): Support module workers (https://crbug.com/680046).
  GetWorkerThread()->EvaluateClassicScript(url_, source_code,
                                           nullptr /* cached_meta_data */,
                                           v8_inspector::V8StackTraceId());
  client_->WorkerScriptLoaded();
}

void WebSharedWorkerImpl::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::PauseWorkerContextOnStart() {
  pause_worker_context_on_start_ = true;
}

void WebSharedWorkerImpl::BindDevToolsAgent(
    mojo::ScopedInterfaceEndpointHandle devtools_agent_request) {
  shadow_page_->BindDevToolsAgent(mojom::blink::DevToolsAgentAssociatedRequest(
      std::move(devtools_agent_request)));
}

WebSharedWorker* WebSharedWorker::Create(WebSharedWorkerClient* client) {
  return new WebSharedWorkerImpl(client);
}

}  // namespace blink
