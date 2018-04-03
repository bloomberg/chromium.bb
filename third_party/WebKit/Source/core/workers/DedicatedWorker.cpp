// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/DedicatedWorker.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreInitializer.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/MessageEvent.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/DedicatedWorkerMessagingProxy.h"
#include "core/workers/WorkerClassicScriptLoader.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "core/workers/WorkerModuleFetchCoordinator.h"
#include "core/workers/WorkerOrWorkletModuleFetchCoordinator.h"
#include "platform/bindings/ScriptState.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/web/WebFrameClient.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-blink.h"
#include "third_party/WebKit/public/platform/dedicated_worker_factory.mojom-blink.h"

namespace blink {

namespace {

service_manager::mojom::blink::InterfaceProviderPtrInfo
ConnectToWorkerInterfaceProvider(
    Document* document,
    scoped_refptr<const SecurityOrigin> script_origin) {
  mojom::blink::DedicatedWorkerFactoryPtr worker_factory;
  document->GetInterfaceProvider()->GetInterface(&worker_factory);
  service_manager::mojom::blink::InterfaceProviderPtrInfo
      interface_provider_ptr;
  worker_factory->CreateDedicatedWorker(
      script_origin, mojo::MakeRequest(&interface_provider_ptr));
  return interface_provider_ptr;
}

}  // namespace

DedicatedWorker* DedicatedWorker::Create(ExecutionContext* context,
                                         const String& url,
                                         const WorkerOptions& options,
                                         ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  Document* document = ToDocument(context);
  UseCounter::Count(context, WebFeature::kWorkerStart);
  if (!document->GetPage()) {
    exception_state.ThrowDOMException(kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }

  KURL script_url = ResolveURL(context, url, exception_state,
                               WebURLRequest::kRequestContextScript);
  if (!script_url.IsValid()) {
    // Don't throw an exception here because it's already thrown in
    // ResolveURL().
    return nullptr;
  }

  // TODO(nhiroki): Remove this flag check once module loading for
  // DedicatedWorker is enabled by default (https://crbug.com/680046).
  if (options.type() == "module" &&
      !RuntimeEnabledFeatures::ModuleDedicatedWorkerEnabled()) {
    exception_state.ThrowTypeError(
        "Module scripts are not supported on DedicatedWorker yet. You can try "
        "the feature with '--enable-experimental-web-platform-features' flag "
        "(see https://crbug.com/680046)");
    return nullptr;
  }

  DedicatedWorker* worker = new DedicatedWorker(context, script_url, options);
  worker->Start();
  return worker;
}

DedicatedWorker::DedicatedWorker(ExecutionContext* context,
                                 const KURL& script_url,
                                 const WorkerOptions& options)
    : AbstractWorker(context),
      script_url_(script_url),
      options_(options),
      context_proxy_(new DedicatedWorkerMessagingProxy(context, this)),
      module_fetch_coordinator_(WorkerModuleFetchCoordinator::Create(
          ToDocument(context)->Fetcher())) {
  DCHECK(IsMainThread());
  DCHECK(script_url_.IsValid());
  DCHECK(context_proxy_);
}

DedicatedWorker::~DedicatedWorker() {
  DCHECK(IsMainThread());
  context_proxy_->ParentObjectDestroyed();
}

void DedicatedWorker::postMessage(ScriptState* script_state,
                                  scoped_refptr<SerializedScriptValue> message,
                                  const MessagePortArray& ports,
                                  ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  // Disentangle the port in preparation for sending it to the remote context.
  auto channels = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), ports, exception_state);
  if (exception_state.HadException())
    return;
  v8_inspector::V8StackTraceId stack_id =
      MainThreadDebugger::Instance()->StoreCurrentStackTrace(
          "Worker.postMessage");
  context_proxy_->PostMessageToWorkerGlobalScope(std::move(message),
                                                 std::move(channels), stack_id);
}

void DedicatedWorker::Start() {
  DCHECK(IsMainThread());

  v8_inspector::V8StackTraceId stack_id =
      MainThreadDebugger::Instance()->StoreCurrentStackTrace("Worker Created");

  if (options_.type() == "classic") {
    network::mojom::FetchRequestMode fetch_request_mode =
        network::mojom::FetchRequestMode::kSameOrigin;
    network::mojom::FetchCredentialsMode fetch_credentials_mode =
        network::mojom::FetchCredentialsMode::kSameOrigin;
    if (script_url_.ProtocolIsData()) {
      fetch_request_mode = network::mojom::FetchRequestMode::kNoCORS;
      fetch_credentials_mode = network::mojom::FetchCredentialsMode::kInclude;
    }
    classic_script_loader_ = WorkerClassicScriptLoader::Create();
    classic_script_loader_->LoadAsynchronously(
        *GetExecutionContext(), script_url_,
        WebURLRequest::kRequestContextWorker, fetch_request_mode,
        fetch_credentials_mode,
        GetExecutionContext()->GetSecurityContext().AddressSpace(),
        WTF::Bind(&DedicatedWorker::OnResponse, WrapPersistent(this)),
        WTF::Bind(&DedicatedWorker::OnFinished, WrapPersistent(this),
                  stack_id));
    return;
  }
  if (options_.type() == "module") {
    // Specify empty source code here because module scripts will be fetched on
    // the worker thread as opposed to classic scripts that are fetched on the
    // main thread.
    context_proxy_->StartWorkerGlobalScope(CreateGlobalScopeCreationParams(),
                                           options_, script_url_, stack_id,
                                           String() /* source_code */);
    return;
  }
  NOTREACHED() << "Invalid type: " << options_.type();
}

void DedicatedWorker::terminate() {
  DCHECK(IsMainThread());
  context_proxy_->TerminateGlobalScope();
}

void DedicatedWorker::ContextDestroyed(ExecutionContext*) {
  DCHECK(IsMainThread());
  if (classic_script_loader_)
    classic_script_loader_->Cancel();
  module_fetch_coordinator_->Dispose();
  terminate();
}

bool DedicatedWorker::HasPendingActivity() const {
  DCHECK(IsMainThread());
  // The worker context does not exist while loading, so we must ensure that the
  // worker object is not collected, nor are its event listeners.
  return context_proxy_->HasPendingActivity() || classic_script_loader_;
}

WorkerClients* DedicatedWorker::CreateWorkerClients() {
  Document* document = ToDocument(GetExecutionContext());
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());

  WorkerClients* worker_clients = WorkerClients::Create();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  CoreInitializer::GetInstance().ProvideIndexedDBClientToWorker(
      *worker_clients);
  ProvideContentSettingsClientToWorker(
      worker_clients, web_frame->Client()->CreateWorkerContentSettingsClient());

  return worker_clients;
}

void DedicatedWorker::OnResponse() {
  DCHECK(IsMainThread());
  probe::didReceiveScriptResponse(GetExecutionContext(),
                                  classic_script_loader_->Identifier());
}

void DedicatedWorker::OnFinished(const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsMainThread());
  if (classic_script_loader_->Canceled()) {
    // Do nothing.
  } else if (classic_script_loader_->Failed()) {
    DispatchEvent(Event::CreateCancelable(EventTypeNames::error));
  } else {
    ReferrerPolicy referrer_policy = kReferrerPolicyDefault;
    if (!classic_script_loader_->GetReferrerPolicy().IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          classic_script_loader_->GetReferrerPolicy(),
          kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
    }
    std::unique_ptr<GlobalScopeCreationParams> creation_params =
        CreateGlobalScopeCreationParams();
    creation_params->referrer_policy = referrer_policy;
    context_proxy_->StartWorkerGlobalScope(
        std::move(creation_params), options_, script_url_, stack_id,
        classic_script_loader_->SourceText());
    probe::scriptImported(GetExecutionContext(),
                          classic_script_loader_->Identifier(),
                          classic_script_loader_->SourceText());
  }
  classic_script_loader_ = nullptr;
}

std::unique_ptr<GlobalScopeCreationParams>
DedicatedWorker::CreateGlobalScopeCreationParams() {
  Document* document = ToDocument(GetExecutionContext());
  const SecurityOrigin* starter_origin = document->GetSecurityOrigin();
  base::UnguessableToken devtools_worker_token =
      document->GetFrame() ? document->GetFrame()->GetDevToolsFrameToken()
                           : base::UnguessableToken::Create();
  return std::make_unique<GlobalScopeCreationParams>(
      script_url_, GetExecutionContext()->UserAgent(),
      document->GetContentSecurityPolicy()->Headers().get(),
      kReferrerPolicyDefault, starter_origin, document->IsSecureContext(),
      CreateWorkerClients(), document->AddressSpace(),
      OriginTrialContext::GetTokens(document).get(), devtools_worker_token,
      std::make_unique<WorkerSettings>(document->GetSettings()),
      kV8CacheOptionsDefault, module_fetch_coordinator_.Get(),
      ConnectToWorkerInterfaceProvider(document,
                                       SecurityOrigin::Create(script_url_)));
}

const AtomicString& DedicatedWorker::InterfaceName() const {
  return EventTargetNames::Worker;
}

void DedicatedWorker::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_proxy_);
  visitor->Trace(module_fetch_coordinator_);
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
