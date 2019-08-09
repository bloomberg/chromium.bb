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

#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"

#include <memory>
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebSharedWorkerImpl::WebSharedWorkerImpl(
    WebSharedWorkerClient* client,
    const base::UnguessableToken& appcache_host_id)
    : client_(client),
      creation_address_space_(network::mojom::IPAddressSpace::kPublic),
      appcache_host_(MakeGarbageCollected<ApplicationCacheHostForSharedWorker>(
          appcache_host_id,
          Thread::Current()->GetTaskRunner())) {
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
  appcache_host_->Detach();

  if (!worker_thread_) {
    client_->WorkerScriptLoadFailed();
    // The worker thread hasn't been started yet. Immediately notify the client
    // of worker termination.
    client_->WorkerContextDestroyed();
    // |this| is deleted at this point.
    return;
  }
  worker_thread_->Terminate();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::OnShadowPageInitialized() {
  DCHECK(IsMainThread());
  ContinueStartWorkerContext();
}

void WebSharedWorkerImpl::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  client_->CountFeature(feature);
}

void WebSharedWorkerImpl::DidFetchScript(int64_t app_cache_id) {
  DCHECK(IsMainThread());
  DCHECK(appcache_host_);
  appcache_host_->SelectCacheForSharedWorker(
      app_cache_id, WTF::Bind(&WebSharedWorkerImpl::OnAppCacheSelected,
                              weak_ptr_factory_.GetWeakPtr()));
}

void WebSharedWorkerImpl::DidFailToFetchClassicScript() {
  DCHECK(IsMainThread());
  client_->WorkerScriptLoadFailed();
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidEvaluateClassicScript(bool success) {
  DCHECK(IsMainThread());
  client_->WorkerScriptEvaluated(success);
}

void WebSharedWorkerImpl::DidCloseWorkerGlobalScope() {
  DCHECK(IsMainThread());
  client_->WorkerContextClosed();
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidTerminateWorkerThread() {
  DCHECK(IsMainThread());
  client_->WorkerContextDestroyed();
  // |this| is deleted at this point.
}

void WebSharedWorkerImpl::Connect(MessagePortChannel web_channel) {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;
  // The HTML spec requires to queue a connect event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/C/#shared-workers-and-the-sharedworker-interface
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::ConnectTaskOnWorkerThread,
                          WTF::CrossThreadUnretained(this),
                          WTF::Passed(std::move(web_channel))));
}

void WebSharedWorkerImpl::ConnectTaskOnWorkerThread(
    MessagePortChannel channel) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  DCHECK(worker_thread_->IsCurrentThread());
  auto* scope = To<SharedWorkerGlobalScope>(worker_thread_->GlobalScope());
  scope->Connect(std::move(channel));
}

void WebSharedWorkerImpl::StartWorkerContext(
    const WebURL& script_request_url,
    const WebString& name,
    const WebString& user_agent,
    const WebString& content_security_policy,
    mojom::ContentSecurityPolicyType policy_type,
    network::mojom::IPAddressSpace creation_address_space,
    const base::UnguessableToken& devtools_worker_token,
    PrivacyPreferences privacy_preferences,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    mojo::ScopedMessagePipeHandle content_settings_handle,
    mojo::ScopedMessagePipeHandle interface_provider,
    mojo::ScopedMessagePipeHandle browser_interface_broker) {
  DCHECK(IsMainThread());
  script_request_url_ = script_request_url;
  name_ = name;
  user_agent_ = user_agent;
  creation_address_space_ = creation_address_space;
  // Chrome doesn't use interface versioning.
  content_settings_info_ = mojom::blink::WorkerContentSettingsProxyPtrInfo(
      std::move(content_settings_handle), 0u);
  pending_interface_provider_.set_handle(std::move(interface_provider));

  browser_interface_broker_ =
      mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>(
          std::move(browser_interface_broker),
          mojom::blink::BrowserInterfaceBroker::Version_);

  devtools_worker_token_ = devtools_worker_token;
  // |shadow_page_| must be created after |devtools_worker_token_| because it
  // triggers creation of a InspectorNetworkAgent that tries to access the
  // token.
  shadow_page_ = std::make_unique<WorkerShadowPage>(
      this, std::move(loader_factory), std::move(privacy_preferences));

  // We'll continue in OnShadowPageInitialized().
  shadow_page_->Initialize(script_request_url_);
}

void WebSharedWorkerImpl::OnAppCacheSelected() {
  DCHECK(IsMainThread());
  DCHECK(GetWorkerThread());
  GetWorkerThread()->OnAppCacheSelected();
}

void WebSharedWorkerImpl::ContinueStartWorkerContext() {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;

  // Creates 'outside settings' used in the "Processing model" algorithm in the
  // HTML spec:
  // https://html.spec.whatwg.org/C/#worker-processing-model
  //
  // TODO(nhiroki): According to the spec, the 'outside settings' should
  // correspond to the Document that called 'new SharedWorker()'. The browser
  // process should pass it up to here.
  scoped_refptr<const SecurityOrigin> starter_origin =
      SecurityOrigin::Create(script_request_url_);
  auto* outside_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          /*global_object_url=*/script_request_url_,
          /*base_url=*/script_request_url_, starter_origin,
          network::mojom::ReferrerPolicy::kDefault,
          /*outgoing_referrer=*/String(),
          CalculateHttpsState(starter_origin.get()),
          AllowedByNosniff::MimeTypeCheck::kLax, creation_address_space_,
          /*insecure_request_policy=*/kBlockAllMixedContent,
          FetchClientSettingsObject::InsecureNavigationsSet(),
          /*mixed_autoupgrade_opt_out=*/false);

  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context =
      client_->CreateWorkerFetchContext();
  DCHECK(web_worker_fetch_context);

  // TODO(nhiroki); Set |script_type| to mojom::ScriptType::kModule for module
  // fetch (https://crbug.com/824646).
  mojom::ScriptType script_type = mojom::ScriptType::kClassic;

  bool starter_secure_context =
      starter_origin->IsPotentiallyTrustworthy() ||
      SchemeRegistry::SchemeShouldBypassSecureContextCheck(
          starter_origin->Protocol());

  auto worker_settings = std::make_unique<WorkerSettings>(
      false /* disable_reading_from_canvas */,
      false /* strict_mixed_content_checking */,
      true /* allow_running_of_insecure_content */,
      false /* strictly_block_blockable_mixed_content */,
      GenericFontFamilySettings());

  // Some params (e.g., referrer policy, address space, CSP) passed to
  // GlobalScopeCreationParams are dummy values. They will be updated after
  // worker script fetch on the worker thread.
  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      script_request_url_, script_type,
      OffMainThreadWorkerScriptFetchOption::kEnabled, name_, user_agent_,
      std::move(web_worker_fetch_context), Vector<CSPHeaderAndType>(),
      outside_settings_object->GetReferrerPolicy(),
      outside_settings_object->GetSecurityOrigin(), starter_secure_context,
      outside_settings_object->GetHttpsState(), CreateWorkerClients(),
      std::make_unique<SharedWorkerContentSettingsProxy>(
          std::move(content_settings_info_)),
      base::nullopt /* response_address_space */,
      nullptr /* origin_trial_tokens */, devtools_worker_token_,
      std::move(worker_settings), kV8CacheOptionsDefault,
      nullptr /* worklet_module_response_map */,
      std::move(pending_interface_provider_),
      std::move(browser_interface_broker_), BeginFrameProviderParams(),
      nullptr /* parent_feature_policy */, base::UnguessableToken());
  StartWorkerThread(std::move(creation_params), script_request_url_,
                    String() /* source_code */, *outside_settings_object);
}

void WebSharedWorkerImpl::StartWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const KURL& script_response_url,
    const String& source_code,
    const FetchClientSettingsObjectSnapshot& outside_settings_object) {
  DCHECK(IsMainThread());
  reporting_proxy_ = MakeGarbageCollected<SharedWorkerReportingProxy>(
      this, ParentExecutionContextTaskRunners::Create());
  worker_thread_ = std::make_unique<SharedWorkerThread>(*reporting_proxy_);

  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;

  auto devtools_params = std::make_unique<WorkerDevToolsParams>();
  devtools_params->devtools_worker_token = devtools_worker_token_;
  devtools_params->wait_for_debugger = pause_worker_context_on_start_;
  mojom::blink::DevToolsAgentPtrInfo devtools_agent_ptr_info;
  devtools_params->agent_request = mojo::MakeRequest(&devtools_agent_ptr_info);
  mojom::blink::DevToolsAgentHostRequest devtools_agent_host_request =
      mojo::MakeRequest(&devtools_params->agent_host_ptr_info);

  GetWorkerThread()->Start(std::move(global_scope_creation_params),
                           thread_startup_data, std::move(devtools_params));
  GetWorkerThread()->FetchAndRunClassicScript(
      script_request_url_, outside_settings_object.CopyData(),
      nullptr /* outside_resource_timing_notifier */,
      v8_inspector::V8StackTraceId());

  // We are now ready to inspect worker thread.
  client_->WorkerReadyForInspection(
      devtools_agent_ptr_info.PassHandle(),
      devtools_agent_host_request.PassMessagePipe());
}

WorkerClients* WebSharedWorkerImpl::CreateWorkerClients() {
  auto* worker_clients = MakeGarbageCollected<WorkerClients>();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  return worker_clients;
}

void WebSharedWorkerImpl::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::PauseWorkerContextOnStart() {
  pause_worker_context_on_start_ = true;
}

std::unique_ptr<WebSharedWorker> WebSharedWorker::Create(
    WebSharedWorkerClient* client,
    const base::UnguessableToken& appcache_host_id) {
  return base::WrapUnique(new WebSharedWorkerImpl(client, appcache_host_id));
}

}  // namespace blink
