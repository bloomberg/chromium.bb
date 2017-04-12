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
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  // Note: startupData is finalized on return. After the relevant parts has been
  // passed along to the created 'context'.
  SharedWorkerGlobalScope* context = new SharedWorkerGlobalScope(
      name, startup_data->script_url_, startup_data->user_agent_, thread,
      std::move(startup_data->starter_origin_privilege_data_),
      startup_data->worker_clients_);
  context->ApplyContentSecurityPolicyFromVector(
      *startup_data->content_security_policy_headers_);
  context->SetWorkerSettings(std::move(startup_data->worker_settings_));
  if (!startup_data->referrer_policy_.IsNull())
    context->ParseAndSetReferrerPolicy(startup_data->referrer_policy_);
  context->SetAddressSpace(startup_data->address_space_);
  OriginTrialContext::AddTokens(context,
                                startup_data->origin_trial_tokens_.get());
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
