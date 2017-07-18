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

#include "core/workers/SharedWorkerGlobalScope.h"

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

MessageEvent* CreateConnectEvent(MessagePort* port) {
  MessageEvent* event = MessageEvent::Create(new MessagePortArray(1, port),
                                             String(), String(), port);
  event->initEvent(EventTypeNames::connect, false, false);
  return event;
}

// static
SharedWorkerGlobalScope* SharedWorkerGlobalScope::Create(
    const String& name,
    SharedWorkerThread* thread,
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  SharedWorkerGlobalScope* context = new SharedWorkerGlobalScope(
      name, creation_params->script_url, creation_params->user_agent, thread,
      std::move(creation_params->starter_origin_privilege_data),
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

SharedWorkerGlobalScope::SharedWorkerGlobalScope(
    const String& name,
    const KURL& url,
    const String& user_agent,
    SharedWorkerThread* thread,
    std::unique_ptr<SecurityOrigin::PrivilegeData>
        starter_origin_privilege_data,
    WorkerClients* worker_clients)
    : WorkerGlobalScope(url,
                        user_agent,
                        thread,
                        MonotonicallyIncreasingTime(),
                        std::move(starter_origin_privilege_data),
                        worker_clients),
      name_(name) {}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope() {}

const AtomicString& SharedWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::SharedWorkerGlobalScope;
}

void SharedWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

DEFINE_TRACE(SharedWorkerGlobalScope) {
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
