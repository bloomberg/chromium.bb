// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerGlobalScope.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/CompositorWorkerProxyClient.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "modules/EventTargetModules.h"
#include "modules/compositorworker/CompositorWorkerThread.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

CompositorWorkerGlobalScope* CompositorWorkerGlobalScope::Create(
    CompositorWorkerThread* thread,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    double time_origin) {
  CompositorWorkerGlobalScope* context = new CompositorWorkerGlobalScope(
      creation_params->script_url, creation_params->user_agent, thread,
      time_origin, std::move(creation_params->starter_origin_privilege_data),
      creation_params->worker_clients);
  context->ApplyContentSecurityPolicyFromVector(
      *creation_params->content_security_policy_headers);
  if (!creation_params->referrer_policy.IsNull())
    context->ParseAndSetReferrerPolicy(creation_params->referrer_policy);
  context->SetAddressSpace(creation_params->address_space);
  return context;
}

CompositorWorkerGlobalScope::CompositorWorkerGlobalScope(
    const KURL& url,
    const String& user_agent,
    CompositorWorkerThread* thread,
    double time_origin,
    std::unique_ptr<SecurityOrigin::PrivilegeData>
        starter_origin_privilege_data,
    WorkerClients* worker_clients)
    : WorkerGlobalScope(url,
                        user_agent,
                        thread,
                        time_origin,
                        std::move(starter_origin_privilege_data),
                        worker_clients),
      executing_animation_frame_callbacks_(false),
      callback_collection_(this) {
  CompositorWorkerProxyClient::From(Clients())->SetGlobalScope(this);
}

CompositorWorkerGlobalScope::~CompositorWorkerGlobalScope() {}

void CompositorWorkerGlobalScope::Dispose() {
  WorkerGlobalScope::Dispose();
  CompositorWorkerProxyClient::From(Clients())->Dispose();
}

DEFINE_TRACE(CompositorWorkerGlobalScope) {
  visitor->Trace(callback_collection_);
  WorkerGlobalScope::Trace(visitor);
}

const AtomicString& CompositorWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::CompositorWorkerGlobalScope;
}

void CompositorWorkerGlobalScope::postMessage(
    ScriptState* script_state,
    PassRefPtr<SerializedScriptValue> message,
    const MessagePortArray& ports,
    ExceptionState& exception_state) {
  // Disentangle the port in preparation for sending it to the remote context.
  MessagePortChannelArray channels = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), ports, exception_state);
  if (exception_state.HadException())
    return;
  WorkerObjectProxy().PostMessageToWorkerObject(std::move(message),
                                                std::move(channels));
}

int CompositorWorkerGlobalScope::requestAnimationFrame(
    FrameRequestCallback* callback) {
  const bool should_signal =
      !executing_animation_frame_callbacks_ && callback_collection_.IsEmpty();
  if (should_signal)
    CompositorWorkerProxyClient::From(Clients())->RequestAnimationFrame();
  return callback_collection_.RegisterCallback(callback);
}

void CompositorWorkerGlobalScope::cancelAnimationFrame(int id) {
  callback_collection_.CancelCallback(id);
}

bool CompositorWorkerGlobalScope::ExecuteAnimationFrameCallbacks(
    double high_res_time_ms) {
  AutoReset<bool> temporary_change(&executing_animation_frame_callbacks_, true);
  callback_collection_.ExecuteCallbacks(high_res_time_ms, high_res_time_ms);
  return !callback_collection_.IsEmpty();
}

InProcessWorkerObjectProxy& CompositorWorkerGlobalScope::WorkerObjectProxy()
    const {
  return static_cast<CompositorWorkerThread*>(GetThread())->WorkerObjectProxy();
}

}  // namespace blink
