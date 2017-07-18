/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/loader/ThreadableLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerClients.h"
#include "modules/EventTargetModules.h"
#include "modules/fetch/GlobalFetch.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "modules/serviceworkers/ServiceWorkerScriptCachedMetadataHandler.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"

namespace blink {

ServiceWorkerGlobalScope* ServiceWorkerGlobalScope::Create(
    ServiceWorkerThread* thread,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    double time_origin) {
  ServiceWorkerGlobalScope* context = new ServiceWorkerGlobalScope(
      creation_params->script_url, creation_params->user_agent, thread,
      time_origin, std::move(creation_params->starter_origin_privilege_data),
      creation_params->worker_clients);

  context->SetV8CacheOptions(creation_params->v8_cache_options);
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

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    const KURL& url,
    const String& user_agent,
    ServiceWorkerThread* thread,
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
      did_evaluate_script_(false),
      script_count_(0),
      script_total_size_(0),
      script_cached_metadata_total_size_(0) {}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() {}

void ServiceWorkerGlobalScope::CountScript(size_t script_size,
                                           size_t cached_metadata_size) {
  ++script_count_;
  script_total_size_ += script_size;
  script_cached_metadata_total_size_ += cached_metadata_size;
}

void ServiceWorkerGlobalScope::DidEvaluateWorkerScript() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, script_count_histogram,
                                  ("ServiceWorker.ScriptCount", 1, 1000, 50));
  script_count_histogram.Count(script_count_);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, script_total_size_histogram,
      ("ServiceWorker.ScriptTotalSize", 1000, 5000000, 50));
  script_total_size_histogram.Count(script_total_size_);
  if (script_cached_metadata_total_size_) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, cached_metadata_histogram,
        ("ServiceWorker.ScriptCachedMetadataTotalSize", 1000, 50000000, 50));
    cached_metadata_histogram.Count(script_cached_metadata_total_size_);
  }
  did_evaluate_script_ = true;
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* script_state,
                                              const RequestInfo& input,
                                              const Dictionary& init,
                                              ExceptionState& exception_state) {
  return GlobalFetch::fetch(script_state, *this, input, init, exception_state);
}

ServiceWorkerClients* ServiceWorkerGlobalScope::clients() {
  if (!clients_)
    clients_ = ServiceWorkerClients::Create();
  return clients_;
}

ServiceWorkerRegistration* ServiceWorkerGlobalScope::registration() {
  return registration_;
}

ScriptPromise ServiceWorkerGlobalScope::skipWaiting(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: short-term fix, see details at:
  // https://codereview.chromium.org/535193002/.
  if (!execution_context)
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->SkipWaiting(
          WTF::MakeUnique<CallbackPromiseAdapter<void, void>>(resolver));
  return promise;
}

void ServiceWorkerGlobalScope::SetRegistration(
    std::unique_ptr<WebServiceWorkerRegistration::Handle> handle) {
  if (!GetExecutionContext())
    return;
  registration_ = ServiceWorkerRegistration::GetOrCreate(
      GetExecutionContext(), WTF::WrapUnique(handle.release()));
}

bool ServiceWorkerGlobalScope::AddEventListenerInternal(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  if (did_evaluate_script_) {
    String message = String::Format(
        "Event handler of '%s' event must be added on the initial evaluation "
        "of worker script.",
        event_type.Utf8().data());
    AddConsoleMessage(ConsoleMessage::Create(kJSMessageSource,
                                             kWarningMessageLevel, message));
  }
  return WorkerGlobalScope::AddEventListenerInternal(event_type, listener,
                                                     options);
}

const AtomicString& ServiceWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::ServiceWorkerGlobalScope;
}

void ServiceWorkerGlobalScope::DispatchExtendableEvent(
    Event* event,
    WaitUntilObserver* observer) {
  observer->WillDispatchEvent();
  DispatchEvent(event);

  // Check if the worker thread is forcibly terminated during the event
  // because of timeout etc.
  observer->DidDispatchEvent(GetThread()->IsForciblyTerminated());
}

void ServiceWorkerGlobalScope::DispatchExtendableEventWithRespondWith(
    Event* event,
    WaitUntilObserver* wait_until_observer,
    RespondWithObserver* respond_with_observer) {
  wait_until_observer->WillDispatchEvent();
  respond_with_observer->WillDispatchEvent();
  DispatchEventResult dispatch_result = DispatchEvent(event);
  respond_with_observer->DidDispatchEvent(dispatch_result);
  // false is okay because waitUntil() for events with respondWith() doesn't
  // care about the promise rejection or an uncaught runtime script error.
  wait_until_observer->DidDispatchEvent(false /* event_dispatch_failed */);
}

DEFINE_TRACE(ServiceWorkerGlobalScope) {
  visitor->Trace(clients_);
  visitor->Trace(registration_);
  WorkerGlobalScope::Trace(visitor);
}

void ServiceWorkerGlobalScope::importScripts(const Vector<String>& urls,
                                             ExceptionState& exception_state) {
  // Bust the MemoryCache to ensure script requests reach the browser-side
  // and get added to and retrieved from the ServiceWorker's script cache.
  // FIXME: Revisit in light of the solution to crbug/388375.
  for (Vector<String>::const_iterator it = urls.begin(); it != urls.end(); ++it)
    GetExecutionContext()->RemoveURLFromMemoryCache(CompleteURL(*it));
  WorkerGlobalScope::importScripts(urls, exception_state);
}

CachedMetadataHandler*
ServiceWorkerGlobalScope::CreateWorkerScriptCachedMetadataHandler(
    const KURL& script_url,
    const Vector<char>* meta_data) {
  return ServiceWorkerScriptCachedMetadataHandler::Create(this, script_url,
                                                          meta_data);
}

void ServiceWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

}  // namespace blink
