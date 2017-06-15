// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/InProcessWorkerBase.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "platform/bindings/ScriptState.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

InProcessWorkerBase::InProcessWorkerBase(ExecutionContext* context)
    : AbstractWorker(context), context_proxy_(nullptr) {}

InProcessWorkerBase::~InProcessWorkerBase() {
  DCHECK(IsMainThread());
  if (!context_proxy_)
    return;
  context_proxy_->ParentObjectDestroyed();
}

void InProcessWorkerBase::postMessage(ScriptState* script_state,
                                      PassRefPtr<SerializedScriptValue> message,
                                      const MessagePortArray& ports,
                                      ExceptionState& exception_state) {
  DCHECK(context_proxy_);
  // Disentangle the port in preparation for sending it to the remote context.
  MessagePortChannelArray channels = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), ports, exception_state);
  if (exception_state.HadException())
    return;
  context_proxy_->PostMessageToWorkerGlobalScope(std::move(message),
                                                 std::move(channels));
}

bool InProcessWorkerBase::Initialize(ExecutionContext* context,
                                     const String& url,
                                     ExceptionState& exception_state) {
  // TODO(mkwst): Revisit the context as
  // https://drafts.css-houdini.org/worklets/ evolves.
  KURL script_url =
      ResolveURL(url, exception_state, WebURLRequest::kRequestContextScript);
  if (script_url.IsEmpty())
    return false;

  WebURLRequest::FetchRequestMode fetch_request_mode =
      script_url.ProtocolIsData() ? WebURLRequest::kFetchRequestModeNoCORS
                                  : WebURLRequest::kFetchRequestModeSameOrigin;

  script_loader_ = WorkerScriptLoader::Create();
  script_loader_->LoadAsynchronously(
      *context, script_url, fetch_request_mode,
      context->GetSecurityContext().AddressSpace(),
      WTF::Bind(&InProcessWorkerBase::OnResponse, WrapPersistent(this)),
      WTF::Bind(&InProcessWorkerBase::OnFinished, WrapPersistent(this)));

  context_proxy_ = CreateInProcessWorkerMessagingProxy(context);

  return true;
}

void InProcessWorkerBase::terminate() {
  if (context_proxy_)
    context_proxy_->TerminateGlobalScope();
}

void InProcessWorkerBase::ContextDestroyed(ExecutionContext*) {
  if (script_loader_)
    script_loader_->Cancel();
  terminate();
}

bool InProcessWorkerBase::HasPendingActivity() const {
  // The worker context does not exist while loading, so we must ensure that the
  // worker object is not collected, nor are its event listeners.
  return (context_proxy_ && context_proxy_->HasPendingActivity()) ||
         script_loader_;
}

void InProcessWorkerBase::OnResponse() {
  probe::didReceiveScriptResponse(GetExecutionContext(),
                                  script_loader_->Identifier());
}

void InProcessWorkerBase::OnFinished() {
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

DEFINE_TRACE(InProcessWorkerBase) {
  visitor->Trace(context_proxy_);
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
