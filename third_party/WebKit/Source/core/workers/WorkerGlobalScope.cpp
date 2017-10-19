/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/workers/WorkerGlobalScope.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/css/OffscreenFontSelector.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SuspendableObject.h"
#include "core/dom/events/Event.h"
#include "core/events/ErrorEvent.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InstalledScriptsManager.h"
#include "core/workers/WorkerLocation.h"
#include "core/workers/WorkerNavigator.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/InstanceCounters.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLRequest.h"

namespace blink {
namespace {

void RemoveURLFromMemoryCacheInternal(const KURL& url) {
  GetMemoryCache()->RemoveURLFromCache(url);
}

}  // namespace

WorkerGlobalScope::~WorkerGlobalScope() {
  DCHECK(!ScriptController());
  InstanceCounters::DecrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);
}

KURL WorkerGlobalScope::CompleteURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // FIXME: Should we change the KURL constructor to have this behavior?
  if (url.IsNull())
    return KURL();
  // Always use UTF-8 in Workers.
  return KURL(BaseURL(), url);
}

void WorkerGlobalScope::EvaluateClassicScript(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<char>> cached_meta_data) {
  DCHECK(IsContextThread());
  CachedMetadataHandler* handler = CreateWorkerScriptCachedMetadataHandler(
      script_url, cached_meta_data.get());
  DCHECK(!source_code.IsNull());
  ReportingProxy().WillEvaluateWorkerScript(
      source_code.length(),
      cached_meta_data.get() ? cached_meta_data->size() : 0);
  bool success = ScriptController()->Evaluate(
      ScriptSourceCode(source_code, script_url), nullptr /* error_event */,
      handler, v8_cache_options_);
  ReportingProxy().DidEvaluateWorkerScript(success);
}

void WorkerGlobalScope::Dispose() {
  DCHECK(IsContextThread());

  // Event listeners would keep DOMWrapperWorld objects alive for too long.
  // Also, they have references to JS objects, which become dangling once Heap
  // is destroyed.
  closing_ = true;
  HeapHashSet<Member<V8AbstractEventListener>> listeners;
  listeners.swap(event_listeners_);
  while (!listeners.IsEmpty()) {
    for (const auto& listener : listeners)
      listener->ClearListenerObject();
    listeners.clear();
    // Pick up any additions made while iterating.
    listeners.swap(event_listeners_);
  }
  RemoveAllEventListeners();

  event_queue_->Close();
  WorkerOrWorkletGlobalScope::Dispose();
}

void WorkerGlobalScope::ExceptionUnhandled(int exception_id) {
  ErrorEvent* event = pending_error_events_.Take(exception_id);
  DCHECK(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(thread_, event);
}

void WorkerGlobalScope::RegisterEventListener(
    V8AbstractEventListener* event_listener) {
  // TODO(sof): remove once crbug.com/677654 has been diagnosed.
  CHECK(&ThreadState::FromObject(this)->Heap() ==
        &ThreadState::FromObject(event_listener)->Heap());
  bool new_entry = event_listeners_.insert(event_listener).is_new_entry;
  CHECK(new_entry);
}

void WorkerGlobalScope::DeregisterEventListener(
    V8AbstractEventListener* event_listener) {
  auto it = event_listeners_.find(event_listener);
  CHECK(it != event_listeners_.end() || closing_);
  event_listeners_.erase(it);
}

WorkerLocation* WorkerGlobalScope::location() const {
  if (!location_)
    location_ = WorkerLocation::Create(url_);
  return location_.Get();
}

WorkerNavigator* WorkerGlobalScope::navigator() const {
  if (!navigator_)
    navigator_ = WorkerNavigator::Create(user_agent_);
  return navigator_.Get();
}

void WorkerGlobalScope::close() {
  // Let current script run to completion, but tell the worker micro task
  // runner to tear down the thread after this task.
  closing_ = true;
}

String WorkerGlobalScope::origin() const {
  return GetSecurityOrigin()->ToString();
}

void WorkerGlobalScope::importScripts(const Vector<String>& urls,
                                      ExceptionState& exception_state) {
  DCHECK(GetContentSecurityPolicy());
  DCHECK(GetExecutionContext());

  ExecutionContext& execution_context = *this->GetExecutionContext();
  Vector<KURL> completed_urls;
  for (const String& url_string : urls) {
    const KURL& url = execution_context.CompleteURL(url_string);
    if (!url.IsValid()) {
      exception_state.ThrowDOMException(
          kSyntaxError, "The URL '" + url_string + "' is invalid.");
      return;
    }
    if (!GetContentSecurityPolicy()->AllowScriptFromSource(
            url, AtomicString(), IntegrityMetadataSet(), kNotParserInserted)) {
      exception_state.ThrowDOMException(
          kNetworkError,
          "The script at '" + url.ElidedString() + "' failed to load.");
      return;
    }
    completed_urls.push_back(url);
  }

  for (const KURL& complete_url : completed_urls) {
    KURL response_url;
    String source_code;
    std::unique_ptr<Vector<char>> cached_meta_data;
    LoadResult result = LoadResult::kNotHandled;
    if (RuntimeEnabledFeatures::ServiceWorkerScriptStreamingEnabled()) {
      result = LoadingScriptFromInstalledScriptsManager(
          complete_url, &response_url, &source_code, &cached_meta_data);
    }

    // If the script wasn't provided by the InstalledScriptsManager, load from
    // ResourceLoader.
    if (result == LoadResult::kNotHandled) {
      result = LoadingScriptFromWorkerScriptLoader(
          complete_url, &response_url, &source_code, &cached_meta_data);
    }

    if (result != LoadResult::kSuccess) {
      exception_state.ThrowDOMException(
          kNetworkError, "The script at '" + complete_url.ElidedString() +
                             "' failed to load.");
      return;
    }

    ErrorEvent* error_event = nullptr;
    CachedMetadataHandler* handler(CreateWorkerScriptCachedMetadataHandler(
        complete_url, cached_meta_data.get()));
    ReportingProxy().WillEvaluateImportedScript(
        source_code.length(), cached_meta_data ? cached_meta_data->size() : 0);
    ScriptController()->Evaluate(ScriptSourceCode(source_code, response_url),
                                 &error_event, handler, v8_cache_options_);
    if (error_event) {
      ScriptController()->RethrowExceptionFromImportedScript(error_event,
                                                             exception_state);
      return;
    }
  }
}

WorkerGlobalScope::LoadResult
WorkerGlobalScope::LoadingScriptFromInstalledScriptsManager(
    const KURL& script_url,
    KURL* out_response_url,
    String* out_source_code,
    std::unique_ptr<Vector<char>>* out_cached_meta_data) {
  if (!GetThread()->GetInstalledScriptsManager() ||
      !GetThread()->GetInstalledScriptsManager()->IsScriptInstalled(
          script_url)) {
    return LoadResult::kNotHandled;
  }
  InstalledScriptsManager::ScriptData script_data;
  InstalledScriptsManager::ScriptStatus status =
      GetThread()->GetInstalledScriptsManager()->GetScriptData(script_url,
                                                               &script_data);
  switch (status) {
    case InstalledScriptsManager::ScriptStatus::kFailed:
      return LoadResult::kFailed;
    case InstalledScriptsManager::ScriptStatus::kSuccess:
      *out_response_url = script_url;
      *out_source_code = script_data.TakeSourceText();
      *out_cached_meta_data = script_data.TakeMetaData();
      // TODO(shimazu): Add appropriate probes for inspector.
      return LoadResult::kSuccess;
  }

  NOTREACHED();
  return LoadResult::kFailed;
}

WorkerGlobalScope::LoadResult
WorkerGlobalScope::LoadingScriptFromWorkerScriptLoader(
    const KURL& script_url,
    KURL* out_response_url,
    String* out_source_code,
    std::unique_ptr<Vector<char>>* out_cached_meta_data) {
  ExecutionContext* execution_context = GetExecutionContext();
  RefPtr<WorkerScriptLoader> script_loader(WorkerScriptLoader::Create());
  script_loader->LoadSynchronously(
      *execution_context, script_url, WebURLRequest::kRequestContextScript,
      execution_context->GetSecurityContext().AddressSpace());

  // If the fetching attempt failed, throw a NetworkError exception and
  // abort all these steps.
  if (script_loader->Failed())
    return LoadResult::kFailed;

  *out_response_url = script_loader->ResponseURL();
  *out_source_code = script_loader->SourceText();
  *out_cached_meta_data = script_loader->ReleaseCachedMetadata();
  probe::scriptImported(execution_context, script_loader->Identifier(),
                        script_loader->SourceText());
  return LoadResult::kSuccess;
}

v8::Local<v8::Object> WorkerGlobalScope::Wrap(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context) {
  LOG(FATAL) << "WorkerGlobalScope must never be wrapped with wrap method.  "
                "The global object of ECMAScript environment is used as the "
                "wrapper.";
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkerGlobalScope::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  LOG(FATAL) << "WorkerGlobalScope must never be wrapped with wrap method.  "
                "The global object of ECMAScript environment is used as the "
                "wrapper.";
  return v8::Local<v8::Object>();
}

bool WorkerGlobalScope::HasPendingActivity() const {
  // The worker global scope wrapper is kept alive as long as its execution
  // context is alive.
  return !ExecutionContext::IsContextDestroyed();
}

bool WorkerGlobalScope::IsContextThread() const {
  return GetThread()->IsCurrentThread();
}

void WorkerGlobalScope::AddConsoleMessage(ConsoleMessage* console_message) {
  DCHECK(IsContextThread());
  ReportingProxy().ReportConsoleMessage(
      console_message->Source(), console_message->Level(),
      console_message->Message(), console_message->Location());
  GetThread()->GetConsoleMessageStorage()->AddConsoleMessage(this,
                                                             console_message);
}

WorkerEventQueue* WorkerGlobalScope::GetEventQueue() const {
  return event_queue_.Get();
}

CoreProbeSink* WorkerGlobalScope::GetProbeSink() {
  if (IsClosing())
    return nullptr;
  if (WorkerInspectorController* controller =
          GetThread()->GetWorkerInspectorController())
    return controller->GetProbeSink();
  return nullptr;
}

bool WorkerGlobalScope::IsSecureContext(String& error_message) const {
  // Until there are APIs that are available in workers and that
  // require a privileged context test that checks ancestors, just do
  // a simple check here. Once we have a need for a real
  // |isSecureContext| check here, we can check the responsible
  // document for a privileged context at worker creation time, pass
  // it in via WorkerThreadStartupData, and check it here.
  if (GetSecurityOrigin()->IsPotentiallyTrustworthy())
    return true;
  error_message = GetSecurityOrigin()->IsPotentiallyTrustworthyErrorMessage();
  return false;
}

service_manager::InterfaceProvider* WorkerGlobalScope::GetInterfaceProvider() {
  return &interface_provider_;
}

ExecutionContext* WorkerGlobalScope::GetExecutionContext() const {
  return const_cast<WorkerGlobalScope*>(this);
}

WorkerGlobalScope::WorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread,
    double time_origin)
    : WorkerOrWorkletGlobalScope(thread->GetIsolate(),
                                 creation_params->worker_clients,
                                 thread->GetWorkerReportingProxy()),
      url_(creation_params->script_url),
      user_agent_(creation_params->user_agent),
      v8_cache_options_(creation_params->v8_cache_options),
      thread_(thread),
      event_queue_(WorkerEventQueue::Create(this)),
      timers_(TaskRunnerHelper::Get(TaskType::kJavascriptTimer, this)),
      time_origin_(time_origin),
      font_selector_(OffscreenFontSelector::Create(this)) {
  InstanceCounters::IncrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);
  SetSecurityOrigin(SecurityOrigin::Create(url_));
  if (creation_params->starter_origin) {
    GetSecurityOrigin()->TransferPrivilegesFrom(
        creation_params->starter_origin->CreatePrivilegeData());
  }
  ApplyContentSecurityPolicyFromVector(
      *creation_params->content_security_policy_parsed_headers);
  SetWorkerSettings(std::move(creation_params->worker_settings));
  if (!creation_params->referrer_policy.IsNull())
    ParseAndSetReferrerPolicy(creation_params->referrer_policy);
  SetAddressSpace(creation_params->address_space);
  OriginTrialContext::AddTokens(this,
                                creation_params->origin_trial_tokens.get());
  // TODO(sammc): Require a valid |creation_params->interface_provider| once all
  // worker types provide a valid |creation_params->interface_provider|.
  if (creation_params->interface_provider.is_valid()) {
    interface_provider_.Bind(
        mojo::MakeProxy(service_manager::mojom::InterfaceProviderPtrInfo(
            creation_params->interface_provider.PassHandle(),
            service_manager::mojom::InterfaceProvider::Version_)));
  }
}

void WorkerGlobalScope::ApplyContentSecurityPolicyFromHeaders(
    const ContentSecurityPolicyResponseHeaders& headers) {
  if (!GetContentSecurityPolicy()) {
    ContentSecurityPolicy* csp = ContentSecurityPolicy::Create();
    SetContentSecurityPolicy(csp);
  }
  GetContentSecurityPolicy()->DidReceiveHeaders(headers);
  GetContentSecurityPolicy()->BindToExecutionContext(GetExecutionContext());
}

void WorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  int next_id = ++last_pending_error_event_id_;
  pending_error_events_.Set(next_id, event);
  ReportingProxy().ReportException(event->MessageForConsole(),
                                   event->Location()->Clone(), next_id);
}

void WorkerGlobalScope::RemoveURLFromMemoryCache(const KURL& url) {
  thread_->GetParentFrameTaskRunners()
      ->Get(TaskType::kNetworking)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&RemoveURLFromMemoryCacheInternal, url));
}

void WorkerGlobalScope::SetWorkerSettings(
    std::unique_ptr<WorkerSettings> worker_settings) {
  worker_settings_ = std::move(worker_settings);
  worker_settings_->MakeGenericFontFamilySettingsAtomic();
  font_selector_->UpdateGenericFontFamilySettings(
      worker_settings_->GetGenericFontFamilySettings());
}

void WorkerGlobalScope::ApplyContentSecurityPolicyFromVector(
    const Vector<CSPHeaderAndType>& headers) {
  if (!GetContentSecurityPolicy()) {
    ContentSecurityPolicy* csp = ContentSecurityPolicy::Create();
    SetContentSecurityPolicy(csp);
  }
  for (const auto& policy_and_type : headers) {
    GetContentSecurityPolicy()->DidReceiveHeader(
        policy_and_type.first, policy_and_type.second,
        kContentSecurityPolicyHeaderSourceHTTP);
  }
  GetContentSecurityPolicy()->BindToExecutionContext(GetExecutionContext());
}

void WorkerGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(location_);
  visitor->Trace(navigator_);
  visitor->Trace(event_queue_);
  visitor->Trace(timers_);
  visitor->Trace(event_listeners_);
  visitor->Trace(pending_error_events_);
  visitor->Trace(font_selector_);
  EventTargetWithInlineData::Trace(visitor);
  SecurityContext::Trace(visitor);
  WorkerOrWorkletGlobalScope::Trace(visitor);
  Supplementable<WorkerGlobalScope>::Trace(visitor);
}

void WorkerGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  EventTargetWithInlineData::TraceWrappers(visitor);
  Supplementable<WorkerGlobalScope>::TraceWrappers(visitor);
}

}  // namespace blink
