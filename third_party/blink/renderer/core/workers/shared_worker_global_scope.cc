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

#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"

#include <memory>
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

SharedWorkerGlobalScope::SharedWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    SharedWorkerThread* thread,
    base::TimeTicks time_origin)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin) {}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope() = default;

const AtomicString& SharedWorkerGlobalScope::InterfaceName() const {
  return event_target_names::kSharedWorkerGlobalScope;
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::ImportClassicScript(
    const KURL& script_url,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
  DCHECK(!IsContextPaused());

  // Step 12. "Fetch a classic worker script given url, outside settings,
  // destination, and inside settings."
  auto destination = mojom::RequestContextType::SHARED_WORKER;

  // Step 12.1. "Set request's reserved client to inside settings."
  // The browesr process takes care of this.

  // Step 12.2. "Fetch request, and asynchronously wait to run the remaining
  // steps as part of fetch's process response for the response response."
  WorkerClassicScriptLoader* classic_script_loader =
      MakeGarbageCollected<WorkerClassicScriptLoader>();
  classic_script_loader->LoadTopLevelScriptAsynchronously(
      *this, CreateOutsideSettingsFetcher(outside_settings_object), script_url,
      destination, network::mojom::FetchRequestMode::kSameOrigin,
      network::mojom::FetchCredentialsMode::kSameOrigin,
      GetSecurityContext().AddressSpace(),
      WTF::Bind(&SharedWorkerGlobalScope::DidReceiveResponseForClassicScript,
                WrapWeakPersistent(this),
                WrapPersistent(classic_script_loader)),
      WTF::Bind(&SharedWorkerGlobalScope::DidImportClassicScript,
                WrapWeakPersistent(this), WrapPersistent(classic_script_loader),
                stack_id));
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::ImportModuleScript(
    const KURL& module_url_record,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    network::mojom::FetchCredentialsMode credentials_mode) {
  // Step 12: "Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise."

  // Step 13: "... Fetch a module worker script graph given url, outside
  // settings, destination, the value of the credentials member of options, and
  // inside settings."

  // TODO(nhiroki): Implement module loading for shared workers.
  // (https://crbug.com/824646)
  NOTREACHED();
}

const String SharedWorkerGlobalScope::name() const {
  return Name();
}

void SharedWorkerGlobalScope::Connect(MessagePortChannel channel) {
  DCHECK(!IsContextPaused());
  MessagePort* port = MessagePort::Create(*this);
  port->Entangle(std::move(channel));
  MessageEvent* event =
      MessageEvent::Create(MakeGarbageCollected<MessagePortArray>(1, port),
                           String(), String(), port);
  event->initEvent(event_type_names::kConnect, false, false);
  DispatchEvent(*event);
}

void SharedWorkerGlobalScope::DidReceiveResponseForClassicScript(
    WorkerClassicScriptLoader* classic_script_loader) {
  DCHECK(IsContextThread());
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
  probe::DidReceiveScriptResponse(this, classic_script_loader->Identifier());
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::DidImportClassicScript(
    WorkerClassicScriptLoader* classic_script_loader,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsContextThread());
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());

  // Step 12. "If the algorithm asynchronously completes with null, then:"
  if (classic_script_loader->Failed()) {
    // Step 12.1. "Queue a task to fire an event named error at worker."
    // Step 12.2. "Run the environment discarding steps for inside settings."
    // Step 12.3. "Return."
    ReportingProxy().DidFailToFetchClassicScript();
    return;
  }
  ReportingProxy().DidFetchScript();
  probe::ScriptImported(this, classic_script_loader->Identifier(),
                        classic_script_loader->SourceText());

  // Step 12.3. "Set worker global scope's url to response's url."
  InitializeURL(classic_script_loader->ResponseURL());

  // Step 12.4. "Set worker global scope's HTTPS state to response's HTTPS
  // state."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 12.5. "Set worker global scope's referrer policy to the result of
  // parsing the `Referrer-Policy` header of response."
  auto referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  if (!classic_script_loader->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        classic_script_loader->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
    SetReferrerPolicy(referrer_policy);
  }

  // Step 12.6. "Execute the Initialize a global object's CSP list algorithm
  // on worker global scope and response. [CSP]"
  DCHECK_EQ(GlobalScopeCSPApplyMode::kUseResponseCSP, GetCSPApplyMode());
  if (classic_script_loader->GetContentSecurityPolicy()) {
    InitContentSecurityPolicyFromVector(
        classic_script_loader->GetContentSecurityPolicy()->Headers());
  } else {
    // Initialize CSP with an empty list.
    InitContentSecurityPolicyFromVector(Vector<CSPHeaderAndType>());
  }
  BindContentSecurityPolicyToExecutionContext();

  // Step 12.7. "Asynchronously complete the perform the fetch steps with
  // response."
  EvaluateClassicScript(
      classic_script_loader->ResponseURL(), classic_script_loader->SourceText(),
      classic_script_loader->ReleaseCachedMetadata(), stack_id);
}

void SharedWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

void SharedWorkerGlobalScope::Trace(blink::Visitor* visitor) {
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
