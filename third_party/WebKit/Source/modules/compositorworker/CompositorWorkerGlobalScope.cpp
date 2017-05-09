// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerGlobalScope.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/CompositorWorkerProxyClient.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/EventTargetModules.h"
#include "modules/compositorworker/CompositorWorkerThread.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

CompositorWorkerGlobalScope* CompositorWorkerGlobalScope::Create(
    CompositorWorkerThread* thread,
    std::unique_ptr<WorkerThreadStartupData> startup_data,
    double time_origin) {
  // Note: startupData is finalized on return. After the relevant parts has been
  // passed along to the created 'context'.
  CompositorWorkerGlobalScope* context = new CompositorWorkerGlobalScope(
      startup_data->script_url_, startup_data->user_agent_, thread, time_origin,
      std::move(startup_data->starter_origin_privilege_data_),
      startup_data->worker_clients_);
  context->ApplyContentSecurityPolicyFromVector(
      *startup_data->content_security_policy_headers_);
  if (!startup_data->referrer_policy_.IsNull())
    context->ParseAndSetReferrerPolicy(startup_data->referrer_policy_);
  context->SetAddressSpace(startup_data->address_space_);
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
