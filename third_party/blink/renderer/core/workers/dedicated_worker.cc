// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/dedicated_worker.h"

#include <utility>
#include "base/feature_list.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

// Indicates whether the origin of worker top-level script's request URL is
// same-origin as the parent execution context's origin or not.
// This is used for UMA and thus the existing values should not be changed.
enum class WorkerTopLevelScriptOriginType {
  kSameOrigin = 0,
  kDataUrl = 1,

  // Cross-origin worker request URL (e.g. https://example.com/worker.js)
  // from an chrome-extension: page.
  kCrossOriginFromExtension = 2,

  // Cross-origin worker request URL from a non chrome-extension: page.
  // There are no known cases for this, and we investigate whether there are
  // really no occurrences.
  kCrossOriginOthers = 3,

  kMaxValue = kCrossOriginOthers
};

void CountTopLevelScriptRequestUrlOriginType(
    const SecurityOrigin& context_origin,
    const KURL& request_url) {
  WorkerTopLevelScriptOriginType origin_type;
  if (request_url.ProtocolIsData()) {
    origin_type = WorkerTopLevelScriptOriginType::kDataUrl;
  } else if (context_origin.IsSameSchemeHostPort(
                 SecurityOrigin::Create(request_url).get())) {
    origin_type = WorkerTopLevelScriptOriginType::kSameOrigin;
  } else if (context_origin.Protocol() == "chrome-extension") {
    // Note: using "chrome-extension" scheme check here is a layering
    // violation. Do not use this except for UMA purpose.
    origin_type = WorkerTopLevelScriptOriginType::kCrossOriginFromExtension;
  } else {
    origin_type = WorkerTopLevelScriptOriginType::kCrossOriginOthers;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Worker.TopLevelScript.OriginType.RequestUrl.DedicatedWorker",
      origin_type);
}

}  // namespace

DedicatedWorker* DedicatedWorker::Create(ExecutionContext* context,
                                         const String& url,
                                         const WorkerOptions* options,
                                         ExceptionState& exception_state) {
  DCHECK(context->IsContextThread());
  UseCounter::Count(context, WebFeature::kWorkerStart);
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }

  KURL script_request_url = ResolveURL(context, url, exception_state,
                                       mojom::RequestContextType::SCRIPT);
  if (!script_request_url.IsValid()) {
    // Don't throw an exception here because it's already thrown in
    // ResolveURL().
    return nullptr;
  }

  // TODO(nhiroki): Remove this flag check once module loading for
  // DedicatedWorker is enabled by default (https://crbug.com/680046).
  if (options->type() == "module" &&
      !RuntimeEnabledFeatures::ModuleDedicatedWorkerEnabled()) {
    exception_state.ThrowTypeError(
        "Module scripts are not supported on DedicatedWorker yet. You can try "
        "the feature with '--enable-experimental-web-platform-features' flag "
        "(see https://crbug.com/680046)");
    return nullptr;
  }

  if (context->IsWorkerGlobalScope())
    UseCounter::Count(context, WebFeature::kNestedDedicatedWorker);

  DedicatedWorker* worker = MakeGarbageCollected<DedicatedWorker>(
      context, script_request_url, options);
  worker->UpdateStateIfNeeded();
  worker->Start();
  return worker;
}

DedicatedWorker::DedicatedWorker(ExecutionContext* context,
                                 const KURL& script_request_url,
                                 const WorkerOptions* options)
    : AbstractWorker(context),
      script_request_url_(script_request_url),
      options_(options),
      context_proxy_(
          MakeGarbageCollected<DedicatedWorkerMessagingProxy>(context, this)),
      factory_client_(
          Platform::Current()->CreateDedicatedWorkerHostFactoryClient(
              this,
              GetExecutionContext()->GetInterfaceProvider())) {
  DCHECK(context->IsContextThread());
  DCHECK(script_request_url_.IsValid());
  DCHECK(context_proxy_);
}

DedicatedWorker::~DedicatedWorker() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  context_proxy_->ParentObjectDestroyed();
}

void DedicatedWorker::postMessage(ScriptState* script_state,
                                  const ScriptValue& message,
                                  Vector<ScriptValue>& transfer,
                                  ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.IsEmpty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void DedicatedWorker::postMessage(ScriptState* script_state,
                                  const ScriptValue& message,
                                  const PostMessageOptions* options,
                                  ExceptionState& exception_state) {
  DCHECK(GetExecutionContext()->IsContextThread());

  BlinkTransferableMessage transferable_message;
  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);
  transferable_message.message = serialized_message;

  // Disentangle the port in preparation for sending it to the remote context.
  transferable_message.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException())
    return;
  transferable_message.user_activation =
      PostMessageHelper::CreateUserActivationSnapshot(GetExecutionContext(),
                                                      options);

  transferable_message.sender_stack_trace_id =
      ThreadDebugger::From(script_state->GetIsolate())
          ->StoreCurrentStackTrace("Worker.postMessage");
  context_proxy_->PostMessageToWorkerGlobalScope(
      std::move(transferable_message));
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void DedicatedWorker::Start() {
  DCHECK(GetExecutionContext()->IsContextThread());

  // This needs to be done after the UpdateStateIfNeeded is called as
  // calling into the debugger can cause a breakpoint.
  v8_stack_trace_id_ = ThreadDebugger::From(GetExecutionContext()->GetIsolate())
                           ->StoreCurrentStackTrace("Worker Created");
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*GetExecutionContext()))
    scope->EnsureFetcher();
  if (blink::features::IsPlzDedicatedWorkerEnabled()) {
    mojom::blink::BlobURLTokenPtr blob_url_token;
    if (script_request_url_.ProtocolIs("blob") &&
        BlobUtils::MojoBlobURLsEnabled()) {
      GetExecutionContext()->GetPublicURLManager().Resolve(
          script_request_url_, MakeRequest(&blob_url_token));
    }

    factory_client_->CreateWorkerHost(
        script_request_url_,
        WebSecurityOrigin(GetExecutionContext()->GetSecurityOrigin()),
        blob_url_token.PassInterface().PassHandle());
    // Continue in OnScriptLoadStarted() or OnScriptLoadStartFailed().
    return;
  }

  factory_client_->CreateWorkerHostDeprecated(
      WebSecurityOrigin(GetExecutionContext()->GetSecurityOrigin()));

  if (base::FeatureList::IsEnabled(
          features::kOffMainThreadDedicatedWorkerScriptFetch) ||
      options_->type() == "module") {
    // Specify empty source code here because scripts will be fetched on the
    // worker thread.
    ContinueStart(
        script_request_url_, OffMainThreadWorkerScriptFetchOption::kEnabled,
        network::mojom::ReferrerPolicy::kDefault,
        base::nullopt /* response_address_space */, String() /* source_code */);
    return;
  }
  if (options_->type() == "classic") {
    // Legacy code path (to be deprecated, see https://crbug.com/835717):
    // A worker thread will start after scripts are fetched on the current
    // thread.
    classic_script_loader_ = MakeGarbageCollected<WorkerClassicScriptLoader>();
    classic_script_loader_->LoadTopLevelScriptAsynchronously(
        *GetExecutionContext(), GetExecutionContext()->Fetcher(),
        script_request_url_, mojom::RequestContextType::WORKER,
        network::mojom::FetchRequestMode::kSameOrigin,
        network::mojom::FetchCredentialsMode::kSameOrigin,
        WTF::Bind(&DedicatedWorker::OnResponse, WrapPersistent(this)),
        WTF::Bind(&DedicatedWorker::OnFinished, WrapPersistent(this)));
    return;
  }
  NOTREACHED() << "Invalid type: " << options_->type();
}

void DedicatedWorker::terminate() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  context_proxy_->TerminateGlobalScope();
}

BeginFrameProviderParams DedicatedWorker::CreateBeginFrameProviderParams() {
  DCHECK(GetExecutionContext()->IsContextThread());
  // If we don't have a frame or we are not in Document, some of the SinkIds
  // won't be initialized. If that's the case, the Worker will initialize it by
  // itself later.
  BeginFrameProviderParams begin_frame_provider_params;
  if (auto* document = DynamicTo<Document>(GetExecutionContext())) {
    LocalFrame* frame = document->GetFrame();
    WebLayerTreeView* layer_tree_view = nullptr;
    if (frame && frame->GetPage()) {
      layer_tree_view =
          frame->GetPage()->GetChromeClient().GetWebLayerTreeView(frame);
      if (layer_tree_view) {
        begin_frame_provider_params.parent_frame_sink_id =
            layer_tree_view->GetFrameSinkId();
      }
    }
    begin_frame_provider_params.frame_sink_id =
        Platform::Current()->GenerateFrameSinkId();
  }
  return begin_frame_provider_params;
}

void DedicatedWorker::ContextDestroyed(ExecutionContext*) {
  DCHECK(GetExecutionContext()->IsContextThread());
  if (classic_script_loader_)
    classic_script_loader_->Cancel();
  factory_client_.reset();
  terminate();
}

bool DedicatedWorker::HasPendingActivity() const {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  // The worker context does not exist while loading, so we must ensure that the
  // worker object is not collected, nor are its event listeners.
  return context_proxy_->HasPendingActivity() || classic_script_loader_;
}

void DedicatedWorker::OnWorkerHostCreated(
    mojo::ScopedMessagePipeHandle interface_provider) {
  DCHECK(!interface_provider_);
  interface_provider_ = service_manager::mojom::blink::InterfaceProviderPtrInfo(
      std::move(interface_provider),
      service_manager::mojom::blink::InterfaceProvider::Version_);
}

void DedicatedWorker::OnScriptLoadStarted() {
  DCHECK(features::IsPlzDedicatedWorkerEnabled());
  // Specify empty source code here because scripts will be fetched on the
  // worker thread.
  ContinueStart(
      script_request_url_, OffMainThreadWorkerScriptFetchOption::kEnabled,
      network::mojom::ReferrerPolicy::kDefault,
      base::nullopt /* response_address_space */, String() /* source_code */);
}

void DedicatedWorker::OnScriptLoadStartFailed() {
  DCHECK(features::IsPlzDedicatedWorkerEnabled());
  context_proxy_->DidFailToFetchScript();
  factory_client_.reset();
}

void DedicatedWorker::DispatchErrorEventForScriptFetchFailure() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  // TODO(nhiroki): Add a console error message.
  DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
}

WorkerClients* DedicatedWorker::CreateWorkerClients() {
  auto* worker_clients = MakeGarbageCollected<WorkerClients>();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  CoreInitializer::GetInstance().ProvideIndexedDBClientToWorker(
      *worker_clients);

  std::unique_ptr<WebContentSettingsClient> client;
  if (auto* document = DynamicTo<Document>(GetExecutionContext())) {
    LocalFrame* frame = document->GetFrame();
    client = frame->Client()->CreateWorkerContentSettingsClient();
  } else if (GetExecutionContext()->IsWorkerGlobalScope()) {
    WebContentSettingsClient* web_worker_content_settings_client =
        WorkerContentSettingsClient::From(*GetExecutionContext())
            ->GetWebContentSettingsClient();
    if (web_worker_content_settings_client)
      client = web_worker_content_settings_client->Clone();
  }

  ProvideContentSettingsClientToWorker(worker_clients, std::move(client));
  return worker_clients;
}

void DedicatedWorker::OnResponse() {
  DCHECK(GetExecutionContext()->IsContextThread());
  probe::DidReceiveScriptResponse(GetExecutionContext(),
                                  classic_script_loader_->Identifier());
}

void DedicatedWorker::OnFinished() {
  DCHECK(GetExecutionContext()->IsContextThread());
  if (classic_script_loader_->Canceled()) {
    // Do nothing.
  } else if (classic_script_loader_->Failed()) {
    context_proxy_->DidFailToFetchScript();
  } else {
    CountTopLevelScriptRequestUrlOriginType(
        *GetExecutionContext()->GetSecurityOrigin(), script_request_url_);

    network::mojom::ReferrerPolicy referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
    if (!classic_script_loader_->GetReferrerPolicy().IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          classic_script_loader_->GetReferrerPolicy(),
          kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
    }
    const KURL script_response_url = classic_script_loader_->ResponseURL();
    DCHECK(script_request_url_ == script_response_url ||
           SecurityOrigin::AreSameSchemeHostPort(script_request_url_,
                                                 script_response_url));
    ContinueStart(
        script_response_url, OffMainThreadWorkerScriptFetchOption::kDisabled,
        referrer_policy, classic_script_loader_->ResponseAddressSpace(),
        classic_script_loader_->SourceText());
    probe::ScriptImported(GetExecutionContext(),
                          classic_script_loader_->Identifier(),
                          classic_script_loader_->SourceText());
  }
  classic_script_loader_ = nullptr;
}

void DedicatedWorker::ContinueStart(
    const KURL& script_url,
    OffMainThreadWorkerScriptFetchOption off_main_thread_fetch_option,
    network::mojom::ReferrerPolicy referrer_policy,
    base::Optional<mojom::IPAddressSpace> response_address_space,
    const String& source_code) {
  auto* outside_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          GetExecutionContext()
              ->Fetcher()
              ->GetProperties()
              .GetFetchClientSettingsObject());
  context_proxy_->StartWorkerGlobalScope(
      CreateGlobalScopeCreationParams(script_url, off_main_thread_fetch_option,
                                      referrer_policy, response_address_space),
      options_, script_url, *outside_settings_object, v8_stack_trace_id_,
      source_code);
}

std::unique_ptr<GlobalScopeCreationParams>
DedicatedWorker::CreateGlobalScopeCreationParams(
    const KURL& script_url,
    OffMainThreadWorkerScriptFetchOption off_main_thread_fetch_option,
    network::mojom::ReferrerPolicy referrer_policy,
    base::Optional<mojom::IPAddressSpace> response_address_space) {
  base::UnguessableToken parent_devtools_token;
  std::unique_ptr<WorkerSettings> settings;
  if (auto* document = DynamicTo<Document>(GetExecutionContext())) {
    if (document->GetFrame())
      parent_devtools_token = document->GetFrame()->GetDevToolsFrameToken();
    settings = std::make_unique<WorkerSettings>(document->GetSettings());
  } else {
    WorkerGlobalScope* worker_global_scope =
        To<WorkerGlobalScope>(GetExecutionContext());
    parent_devtools_token =
        worker_global_scope->GetThread()->GetDevToolsWorkerToken();
    settings = WorkerSettings::Copy(worker_global_scope->GetWorkerSettings());
  }

  mojom::ScriptType script_type = (options_->type() == "classic")
                                      ? mojom::ScriptType::kClassic
                                      : mojom::ScriptType::kModule;

  DCHECK(interface_provider_);
  return std::make_unique<GlobalScopeCreationParams>(
      script_url, script_type, off_main_thread_fetch_option, options_->name(),
      GetExecutionContext()->UserAgent(), CreateWebWorkerFetchContext(),
      GetExecutionContext()->GetContentSecurityPolicy()->Headers(),
      referrer_policy, GetExecutionContext()->GetSecurityOrigin(),
      GetExecutionContext()->IsSecureContext(),
      GetExecutionContext()->GetHttpsState(), CreateWorkerClients(),
      response_address_space,
      OriginTrialContext::GetTokens(GetExecutionContext()).get(),
      parent_devtools_token, std::move(settings), kV8CacheOptionsDefault,
      nullptr /* worklet_module_responses_map */,
      std::move(interface_provider_), CreateBeginFrameProviderParams(),
      GetExecutionContext()->GetSecurityContext().GetFeaturePolicy(),
      GetExecutionContext()->GetAgentClusterID());
}

scoped_refptr<WebWorkerFetchContext>
DedicatedWorker::CreateWebWorkerFetchContext() {
  // This worker is being created by the document.
  if (auto* document = DynamicTo<Document>(GetExecutionContext())) {
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context;
    LocalFrame* frame = document->GetFrame();
    if (features::IsPlzDedicatedWorkerEnabled()) {
      web_worker_fetch_context =
          frame->Client()->CreateWorkerFetchContextForPlzDedicatedWorker(
              factory_client_.get());
    } else {
      web_worker_fetch_context = frame->Client()->CreateWorkerFetchContext();
      web_worker_fetch_context->SetApplicationCacheHostID(
          frame->Loader()
              .GetDocumentLoader()
              ->GetApplicationCacheHost()
              ->GetHostID());
    }
    web_worker_fetch_context->SetIsOnSubframe(!frame->IsMainFrame());
    return web_worker_fetch_context;
  }

  // This worker is being created by an existing worker (i.e., nested workers).
  // Clone the worker fetch context from the parent's one.
  // TODO(nhiroki): Create WebWorkerFetchContext using |factory_client_| when
  // PlzDedicatedWorker is enabled (https://crbug.com/906991).
  auto* scope = To<WorkerGlobalScope>(GetExecutionContext());
  return static_cast<WorkerFetchContext&>(scope->Fetcher()->Context())
      .GetWebWorkerFetchContext()
      ->CloneForNestedWorker(scope->GetTaskRunner(TaskType::kNetworking));
}

const AtomicString& DedicatedWorker::InterfaceName() const {
  return event_target_names::kWorker;
}

void DedicatedWorker::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  DCHECK(GetExecutionContext()->IsContextThread());
  switch (state) {
    case mojom::FrameLifecycleState::kPaused:
      // Do not do anything in this case. kPaused is only used
      // for when the main thread is paused we shouldn't worry
      // about pausing the worker thread in this case.
      break;
    case mojom::FrameLifecycleState::kFrozen:
    case mojom::FrameLifecycleState::kFrozenAutoResumeMedia:
      if (!requested_frozen_) {
        requested_frozen_ = true;
        context_proxy_->Freeze();
      }
      break;
    case mojom::FrameLifecycleState::kRunning:
      if (requested_frozen_) {
        context_proxy_->Resume();
        requested_frozen_ = false;
      }
      break;
  }
}

void DedicatedWorker::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_proxy_);
  visitor->Trace(options_);
  visitor->Trace(classic_script_loader_);
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
