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
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SuspendableObject.h"
#include "core/events/ErrorEvent.h"
#include "core/events/Event.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerLocation.h"
#include "core/workers/WorkerNavigator.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/InstanceCounters.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
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
  return KURL(url_, url);
}

void WorkerGlobalScope::Dispose() {
  DCHECK(GetThread()->IsCurrentThread());

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

void WorkerGlobalScope::ReportFeature(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK(thread_);
  thread_->GetWorkerReportingProxy().CountFeature(feature);
}

void WorkerGlobalScope::ReportDeprecation(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK(thread_);
  thread_->GetWorkerReportingProxy().CountDeprecation(feature);
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
    RefPtr<WorkerScriptLoader> script_loader(WorkerScriptLoader::Create());
    script_loader->LoadSynchronously(
        execution_context, complete_url, WebURLRequest::kRequestContextScript,
        execution_context.GetSecurityContext().AddressSpace());

    // If the fetching attempt failed, throw a NetworkError exception and
    // abort all these steps.
    if (script_loader->Failed()) {
      exception_state.ThrowDOMException(
          kNetworkError, "The script at '" + complete_url.ElidedString() +
                             "' failed to load.");
      return;
    }

    probe::scriptImported(&execution_context, script_loader->Identifier(),
                          script_loader->SourceText());

    ErrorEvent* error_event = nullptr;
    std::unique_ptr<Vector<char>> cached_meta_data(
        script_loader->ReleaseCachedMetadata());
    CachedMetadataHandler* handler(CreateWorkerScriptCachedMetadataHandler(
        complete_url, cached_meta_data.get()));
    GetThread()->GetWorkerReportingProxy().WillEvaluateImportedScript(
        script_loader->SourceText().length(),
        script_loader->CachedMetadata()
            ? script_loader->CachedMetadata()->size()
            : 0);
    ScriptController()->Evaluate(ScriptSourceCode(script_loader->SourceText(),
                                                  script_loader->ResponseURL()),
                                 &error_event, handler, v8_cache_options_);
    if (error_event) {
      ScriptController()->RethrowExceptionFromImportedScript(error_event,
                                                             exception_state);
      return;
    }
  }
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
  GetThread()->GetWorkerReportingProxy().ReportConsoleMessage(
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

ExecutionContext* WorkerGlobalScope::GetExecutionContext() const {
  return const_cast<WorkerGlobalScope*>(this);
}

WorkerGlobalScope::WorkerGlobalScope(
    const KURL& url,
    const String& user_agent,
    WorkerThread* thread,
    double time_origin,
    std::unique_ptr<SecurityOrigin::PrivilegeData>
        starter_origin_privilage_data,
    WorkerClients* worker_clients)
    : WorkerOrWorkletGlobalScope(thread->GetIsolate(), worker_clients),
      url_(url),
      user_agent_(user_agent),
      v8_cache_options_(kV8CacheOptionsDefault),
      thread_(thread),
      event_queue_(WorkerEventQueue::Create(this)),
      timers_(TaskRunnerHelper::Get(TaskType::kTimer, this)),
      time_origin_(time_origin) {
  InstanceCounters::IncrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);
  SetSecurityOrigin(SecurityOrigin::Create(url));
  if (starter_origin_privilage_data)
    GetSecurityOrigin()->TransferPrivilegesFrom(
        std::move(starter_origin_privilage_data));
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

void WorkerGlobalScope::ApplyContentSecurityPolicyFromVector(
    const Vector<CSPHeaderAndType>& headers) {
  if (!GetContentSecurityPolicy()) {
    ContentSecurityPolicy* csp = ContentSecurityPolicy::Create();
    SetContentSecurityPolicy(csp);
  }
  for (const auto& policy_and_type : headers)
    GetContentSecurityPolicy()->DidReceiveHeader(
        policy_and_type.first, policy_and_type.second,
        kContentSecurityPolicyHeaderSourceHTTP);
  GetContentSecurityPolicy()->BindToExecutionContext(GetExecutionContext());
}

void WorkerGlobalScope::SetWorkerSettings(
    std::unique_ptr<WorkerSettings> worker_settings) {
  worker_settings_ = std::move(worker_settings);
}

void WorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  int next_id = ++last_pending_error_event_id_;
  pending_error_events_.Set(next_id, event);
  GetThread()->GetWorkerReportingProxy().ReportException(
      event->MessageForConsole(), event->Location()->Clone(), next_id);
}

void WorkerGlobalScope::RemoveURLFromMemoryCache(const KURL& url) {
  thread_->GetParentFrameTaskRunners()
      ->Get(TaskType::kNetworking)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&RemoveURLFromMemoryCacheInternal, url));
}

KURL WorkerGlobalScope::VirtualCompleteURL(const String& url) const {
  return CompleteURL(url);
}

DEFINE_TRACE(WorkerGlobalScope) {
  visitor->Trace(location_);
  visitor->Trace(navigator_);
  visitor->Trace(event_queue_);
  visitor->Trace(timers_);
  visitor->Trace(event_listeners_);
  visitor->Trace(pending_error_events_);
  EventTargetWithInlineData::Trace(visitor);
  SecurityContext::Trace(visitor);
  WorkerOrWorkletGlobalScope::Trace(visitor);
  Supplementable<WorkerGlobalScope>::Trace(visitor);
}

}  // namespace blink
