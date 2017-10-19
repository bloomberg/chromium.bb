// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/DedicatedWorker.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreInitializer.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/frame/UseCounter.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/DedicatedWorkerMessagingProxy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "core/workers/WorkerScriptLoader.h"
#include "platform/bindings/ScriptState.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/web/WebFrameClient.h"

namespace blink {

DedicatedWorker* DedicatedWorker::Create(ExecutionContext* context,
                                         const String& url,
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
  if (!script_url.IsValid())
    return nullptr;

  DedicatedWorker* worker = new DedicatedWorker(context, script_url);
  worker->Start();
  return worker;
}

DedicatedWorker::DedicatedWorker(ExecutionContext* context,
                                 const KURL& script_url)
    : AbstractWorker(context),
      script_url_(script_url),
      context_proxy_(CreateMessagingProxy(context)) {
  DCHECK(IsMainThread());
  DCHECK(script_url_.IsValid());
  DCHECK(context_proxy_);
}

DedicatedWorker::~DedicatedWorker() {
  DCHECK(IsMainThread());
  context_proxy_->ParentObjectDestroyed();
}

void DedicatedWorker::postMessage(ScriptState* script_state,
                                  RefPtr<SerializedScriptValue> message,
                                  const MessagePortArray& ports,
                                  ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  // Disentangle the port in preparation for sending it to the remote context.
  auto channels = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), ports, exception_state);
  if (exception_state.HadException())
    return;
  context_proxy_->PostMessageToWorkerGlobalScope(std::move(message),
                                                 std::move(channels));
}

void DedicatedWorker::Start() {
  DCHECK(IsMainThread());
  WebURLRequest::FetchRequestMode fetch_request_mode =
      WebURLRequest::kFetchRequestModeSameOrigin;
  WebURLRequest::FetchCredentialsMode fetch_credentials_mode =
      WebURLRequest::kFetchCredentialsModeSameOrigin;
  if (script_url_.ProtocolIsData()) {
    fetch_request_mode = WebURLRequest::kFetchRequestModeNoCORS;
    fetch_credentials_mode = WebURLRequest::kFetchCredentialsModeInclude;
  }

  script_loader_ = WorkerScriptLoader::Create();
  script_loader_->LoadAsynchronously(
      *GetExecutionContext(), script_url_, WebURLRequest::kRequestContextWorker,
      fetch_request_mode, fetch_credentials_mode,
      GetExecutionContext()->GetSecurityContext().AddressSpace(),
      WTF::Bind(&DedicatedWorker::OnResponse, WrapPersistent(this)),
      WTF::Bind(&DedicatedWorker::OnFinished, WrapPersistent(this)));
}

void DedicatedWorker::terminate() {
  DCHECK(IsMainThread());
  context_proxy_->TerminateGlobalScope();
}

void DedicatedWorker::ContextDestroyed(ExecutionContext*) {
  DCHECK(IsMainThread());
  if (script_loader_)
    script_loader_->Cancel();
  terminate();
}

bool DedicatedWorker::HasPendingActivity() const {
  DCHECK(IsMainThread());
  // The worker context does not exist while loading, so we must ensure that the
  // worker object is not collected, nor are its event listeners.
  return context_proxy_->HasPendingActivity() || script_loader_;
}

DedicatedWorkerMessagingProxy* DedicatedWorker::CreateMessagingProxy(
    ExecutionContext* context) {
  DCHECK(IsMainThread());
  Document* document = ToDocument(context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());

  WorkerClients* worker_clients = WorkerClients::Create();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  CoreInitializer::GetInstance().ProvideIndexedDBClientToWorker(
      *worker_clients);
  ProvideContentSettingsClientToWorker(
      worker_clients, web_frame->Client()->CreateWorkerContentSettingsClient());
  return new DedicatedWorkerMessagingProxy(context, this, worker_clients);
}

void DedicatedWorker::OnResponse() {
  DCHECK(IsMainThread());
  probe::didReceiveScriptResponse(GetExecutionContext(),
                                  script_loader_->Identifier());
}

void DedicatedWorker::OnFinished() {
  DCHECK(IsMainThread());
  if (script_loader_->Canceled()) {
    // Do nothing.
  } else if (script_loader_->Failed()) {
    DispatchEvent(Event::CreateCancelable(EventTypeNames::error));
  } else {
    context_proxy_->StartWorkerGlobalScope(
        script_loader_->Url(), GetExecutionContext()->UserAgent(),
        script_loader_->SourceText(), script_loader_->GetReferrerPolicy());
    probe::scriptImported(GetExecutionContext(), script_loader_->Identifier(),
                          script_loader_->SourceText());
  }
  script_loader_ = nullptr;
}

const AtomicString& DedicatedWorker::InterfaceName() const {
  return EventTargetNames::Worker;
}

void DedicatedWorker::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_proxy_);
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
