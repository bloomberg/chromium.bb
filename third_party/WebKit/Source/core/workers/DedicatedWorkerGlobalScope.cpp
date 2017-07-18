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

#include "core/workers/DedicatedWorkerGlobalScope.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

DedicatedWorkerGlobalScope* DedicatedWorkerGlobalScope::Create(
    DedicatedWorkerThread* thread,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    double time_origin) {
  DedicatedWorkerGlobalScope* context = new DedicatedWorkerGlobalScope(
      creation_params->script_url, creation_params->user_agent, thread,
      time_origin, std::move(creation_params->starter_origin_privilege_data),
      creation_params->worker_clients);
  context->ApplyContentSecurityPolicyFromVector(
      *creation_params->content_security_policy_headers);
  context->SetWorkerSettings(std::move(creation_params->worker_settings));
  if (!creation_params->referrer_policy.IsNull())
    context->ParseAndSetReferrerPolicy(creation_params->referrer_policy);
  context->SetAddressSpace(creation_params->address_space);
  OriginTrialContext::AddTokens(context,
                                creation_params->origin_trial_tokens.get());
  return context;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    const KURL& url,
    const String& user_agent,
    DedicatedWorkerThread* thread,
    double time_origin,
    std::unique_ptr<SecurityOrigin::PrivilegeData>
        starter_origin_privilege_data,
    WorkerClients* worker_clients)
    : WorkerGlobalScope(url,
                        user_agent,
                        thread,
                        time_origin,
                        std::move(starter_origin_privilege_data),
                        worker_clients) {}

DedicatedWorkerGlobalScope::~DedicatedWorkerGlobalScope() {}

const AtomicString& DedicatedWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::DedicatedWorkerGlobalScope;
}

void DedicatedWorkerGlobalScope::postMessage(
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

InProcessWorkerObjectProxy& DedicatedWorkerGlobalScope::WorkerObjectProxy()
    const {
  return static_cast<DedicatedWorkerThread*>(GetThread())->WorkerObjectProxy();
}

DEFINE_TRACE(DedicatedWorkerGlobalScope) {
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
