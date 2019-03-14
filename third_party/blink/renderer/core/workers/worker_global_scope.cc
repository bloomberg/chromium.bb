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

#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/font_face_set_worker.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/core/frame/dom_timer_coordinator.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {
namespace {

void RemoveURLFromMemoryCacheInternal(const KURL& url) {
  GetMemoryCache()->RemoveURLFromCache(url);
}

}  // namespace

FontFaceSet* WorkerGlobalScope::fonts() {
  return FontFaceSetWorker::From(*this);
}

WorkerGlobalScope::~WorkerGlobalScope() {
  DCHECK(!ScriptController());
  InstanceCounters::DecrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);
}

NOINLINE const KURL& WorkerGlobalScope::Url() const {
  CHECK(url_.IsValid());
  return url_;
}

KURL WorkerGlobalScope::CompleteURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // FIXME: Should we change the KURL constructor to have this behavior?
  if (url.IsNull())
    return KURL();
  // Always use UTF-8 in Workers.
  return KURL(BaseURL(), url);
}

const KURL& WorkerGlobalScope::BaseURL() const {
  return Url();
}

NOINLINE void WorkerGlobalScope::InitializeURL(const KURL& url) {
  CHECK(url_.IsNull());
  DCHECK(url.IsValid());
  if (GetSecurityOrigin()->IsOpaque()) {
    DCHECK(SecurityOrigin::Create(url)->IsOpaque());
  } else {
    DCHECK(GetSecurityOrigin()->IsSameSchemeHostPort(
        SecurityOrigin::Create(url).get()));
  }
  url_ = url;
}

void WorkerGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  closing_ = true;
  WorkerOrWorkletGlobalScope::Dispose();
}

void WorkerGlobalScope::ExceptionUnhandled(int exception_id) {
  ErrorEvent* event = pending_error_events_.Take(exception_id);
  DCHECK(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(thread_, event);
}

WorkerLocation* WorkerGlobalScope::location() const {
  if (!location_)
    location_ = WorkerLocation::Create(Url());
  return location_.Get();
}

WorkerNavigator* WorkerGlobalScope::navigator() const {
  if (!navigator_)
    navigator_ = WorkerNavigator::Create(user_agent_, GetExecutionContext());
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

void WorkerGlobalScope::importScripts(
    const HeapVector<StringOrTrustedScriptURL>& urls,
    ExceptionState& exception_state) {
  Vector<String> string_urls;
  for (const StringOrTrustedScriptURL& stringOrUrl : urls) {
    // TODO(vogelheim): Re-implement Trusted Types logic when supported by
    // workers.
    String string_url = stringOrUrl.IsString()
                            ? stringOrUrl.GetAsString()
                            : stringOrUrl.GetAsTrustedScriptURL()->toString();
    string_urls.push_back(string_url);
  }
  ImportScriptsInternal(string_urls, exception_state);
}

// Implementation of the "import scripts into worker global scope" algorithm:
// https://html.spec.whatwg.org/C/#import-scripts-into-worker-global-scope
void WorkerGlobalScope::ImportScriptsInternal(const Vector<String>& urls,
                                              ExceptionState& exception_state) {
  DCHECK(GetContentSecurityPolicy());
  DCHECK(GetExecutionContext());

  // Step 1: "If worker global scope's type is "module", throw a TypeError
  // exception."
  if (script_type_ == mojom::ScriptType::kModule) {
    exception_state.ThrowTypeError(
        "Module scripts don't support importScripts().");
    return;
  }

  // Step 2: "Let settings object be the current settings object."
  // |this| roughly corresponds to the current settings object.

  // Step 3: "If urls is empty, return."
  if (urls.IsEmpty())
    return;

  // Step 4: "Parse each value in urls relative to settings object. If any fail,
  // throw a "SyntaxError" DOMException."
  Vector<KURL> completed_urls;
  for (const String& url_string : urls) {
    const KURL& url = CompleteURL(url_string);
    if (!url.IsValid()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The URL '" + url_string + "' is invalid.");
      return;
    }
    if (!GetContentSecurityPolicy()->AllowScriptFromSource(
            url, AtomicString(), IntegrityMetadataSet(), kNotParserInserted)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNetworkError,
          "The script at '" + url.ElidedString() + "' failed to load.");
      return;
    }
    completed_urls.push_back(url);
  }

  // Step 5: "For each url in the resulting URL records, run these substeps:"
  for (const KURL& complete_url : completed_urls) {
    KURL response_url;
    String source_code;
    std::unique_ptr<Vector<uint8_t>> cached_meta_data;

    // Step 5.1: "Fetch a classic worker-imported script given url and settings
    // object, passing along any custom perform the fetch steps provided. If
    // this succeeds, let script be the result. Otherwise, rethrow the
    // exception."
    if (!FetchClassicImportedScript(complete_url, &response_url, &source_code,
                                    &cached_meta_data)) {
      // TODO(vogelheim): In case of certain types of failure - e.g. 'nosniff'
      // block - this ought to be a DOMExceptionCode::kSecurityError, but that
      // information presently gets lost on the way.
      exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                        "The script at '" +
                                            complete_url.ElidedString() +
                                            "' failed to load.");
      return;
    }

    // importScripts always uses "no-cors", so simply checking the origin is
    // enough.
    // TODO(yhirano): Remove this ad-hoc logic and use the response type.
    const SanitizeScriptErrors sanitize_script_errors =
        GetSecurityOrigin()->CanReadContent(response_url)
            ? SanitizeScriptErrors::kDoNotSanitize
            : SanitizeScriptErrors::kSanitize;

    // Step 5.2: "Run the classic script script, with the rethrow errors
    // argument set to true."
    ErrorEvent* error_event = nullptr;
    SingleCachedMetadataHandler* handler(
        CreateWorkerScriptCachedMetadataHandler(complete_url,
                                                cached_meta_data.get()));
    ReportingProxy().WillEvaluateImportedClassicScript(
        source_code.length(), cached_meta_data ? cached_meta_data->size() : 0);
    ScriptController()->Evaluate(
        ScriptSourceCode(source_code, ScriptSourceLocationType::kUnknown,
                         handler, response_url),
        sanitize_script_errors, &error_event, GetV8CacheOptions());
    if (error_event) {
      ScriptController()->RethrowExceptionFromImportedScript(error_event,
                                                             exception_state);
      return;
    }
  }
}

// Implementation of the "fetch a classic worker-imported script" algorithm.
// https://html.spec.whatwg.org/C/#fetch-a-classic-worker-imported-script
bool WorkerGlobalScope::FetchClassicImportedScript(
    const KURL& script_url,
    KURL* out_response_url,
    String* out_source_code,
    std::unique_ptr<Vector<uint8_t>>* out_cached_meta_data) {
  // InstalledScriptsManager is now used only for starting installed service
  // workers.
  // TODO(nhiroki): Consider moving this into ServiceWorkerGlobalScope.
  InstalledScriptsManager* installed_scripts_manager =
      GetThread()->GetInstalledScriptsManager();
  if (installed_scripts_manager &&
      installed_scripts_manager->IsScriptInstalled(script_url)) {
    DCHECK(IsServiceWorkerGlobalScope());
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data =
        installed_scripts_manager->GetScriptData(script_url);
    if (!script_data)
      return false;
    *out_response_url = script_url;
    *out_source_code = script_data->TakeSourceText();
    *out_cached_meta_data = script_data->TakeMetaData();
    // TODO(shimazu): Add appropriate probes for inspector.
    return true;
  }

  // If the script wasn't provided by the InstalledScriptsManager, load from
  // ResourceLoader.
  ExecutionContext* execution_context = GetExecutionContext();
  WorkerClassicScriptLoader* classic_script_loader =
      MakeGarbageCollected<WorkerClassicScriptLoader>();
  EnsureFetcher();
  classic_script_loader->LoadSynchronously(
      *execution_context, Fetcher(), script_url,
      mojom::RequestContextType::SCRIPT,
      execution_context->GetSecurityContext().AddressSpace());
  if (classic_script_loader->Failed())
    return false;
  *out_response_url = classic_script_loader->ResponseURL();
  *out_source_code = classic_script_loader->SourceText();
  *out_cached_meta_data = classic_script_loader->ReleaseCachedMetadata();
  probe::ScriptImported(execution_context, classic_script_loader->Identifier(),
                        classic_script_loader->SourceText());
  return true;
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

void WorkerGlobalScope::EvaluateClassicScript(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(!IsContextPaused());
  CHECK(!GetExecutionContext()->IsContextDestroyed())
      << "https://crbug.com/930618: worker global scope was destroyed before "
         "evaluating classic script";
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(GetThread()->GetIsolate());
  if (debugger)
    debugger->ExternalAsyncTaskStarted(stack_id);
  EvaluateClassicScriptInternal(script_url, source_code,
                                std::move(cached_meta_data));
  if (debugger)
    debugger->ExternalAsyncTaskFinished(stack_id);
}

void WorkerGlobalScope::ReceiveMessage(BlinkTransferableMessage message) {
  DCHECK(!IsContextPaused());
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*this, std::move(message.ports));
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(GetThread()->GetIsolate());
  if (debugger)
    debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  UserActivation* user_activation = nullptr;
  if (message.user_activation) {
    user_activation = MakeGarbageCollected<UserActivation>(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }
  DispatchEvent(*MessageEvent::Create(ports, std::move(message.message),
                                      user_activation));
  if (debugger)
    debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
}

void WorkerGlobalScope::EvaluateClassicScriptInternal(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data) {
  DCHECK(IsContextThread());
  SingleCachedMetadataHandler* handler =
      CreateWorkerScriptCachedMetadataHandler(script_url,
                                              cached_meta_data.get());
  DCHECK(!source_code.IsNull());
  ReportingProxy().WillEvaluateClassicScript(
      source_code.length(),
      cached_meta_data.get() ? cached_meta_data->size() : 0);
  // Cross-origin workers are disallowed, so use
  // SanitizeScriptErrors::kDoNotSanitize.
  bool success = ScriptController()->Evaluate(
      ScriptSourceCode(source_code, handler, script_url),
      SanitizeScriptErrors::kDoNotSanitize, nullptr /* error_event */,
      GetV8CacheOptions());
  ReportingProxy().DidEvaluateClassicScript(success);
}

WorkerGlobalScope::WorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread,
    base::TimeTicks time_origin)
    : WorkerOrWorkletGlobalScope(
          thread->GetIsolate(),
          creation_params->global_scope_name,
          creation_params->v8_cache_options,
          creation_params->worker_clients,
          std::move(creation_params->web_worker_fetch_context),
          thread->GetWorkerReportingProxy()),
      script_type_(creation_params->script_type),
      user_agent_(creation_params->user_agent),
      parent_devtools_token_(creation_params->parent_devtools_token),
      thread_(thread),
      timers_(GetTaskRunner(TaskType::kJavascriptTimer)),
      time_origin_(time_origin),
      font_selector_(OffscreenFontSelector::Create(this)),
      animation_frame_provider_(WorkerAnimationFrameProvider::Create(
          this,
          creation_params->begin_frame_provider_params)),
      agent_cluster_id_(creation_params->agent_cluster_id.is_empty()
                            ? base::UnguessableToken::Create()
                            : creation_params->agent_cluster_id),
      csp_apply_mode_(creation_params->csp_apply_mode) {
  InstanceCounters::IncrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);
  scoped_refptr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(creation_params->script_url);
  if (creation_params->starter_origin) {
    security_origin->TransferPrivilegesFrom(
        creation_params->starter_origin->CreatePrivilegeData());
  }
  SetSecurityOrigin(std::move(security_origin));

  // https://html.spec.whatwg.org/C/#run-a-worker
  // 4. Set worker global scope's HTTPS state to response's HTTPS state. [spec
  // text]
  https_state_ = CalculateHttpsState(GetSecurityOrigin(),
                                     creation_params->starter_https_state);

  SetOutsideContentSecurityPolicyHeaders(
      creation_params->content_security_policy_parsed_headers);
  if (csp_apply_mode_ == GlobalScopeCSPApplyMode::kUseCreationParamsCSP) {
    InitContentSecurityPolicyFromVector(
        creation_params->content_security_policy_parsed_headers);
    BindContentSecurityPolicyToExecutionContext();
  }

  SetWorkerSettings(std::move(creation_params->worker_settings));

  // Set the URL and referrer policy here for workers whose script is fetched
  // on the main thread. For off-the-main-thread fetches, they are instead set
  // after the script is fetched.
  if (creation_params->off_main_thread_fetch_option ==
      OffMainThreadWorkerScriptFetchOption::kDisabled) {
    InitializeURL(creation_params->script_url);
    SetReferrerPolicy(creation_params->referrer_policy);
  }

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

  // A FeaturePolicy is created by FeaturePolicy::CreateFromParentPolicy, even
  // if the parent policy is null.
  DCHECK(creation_params->worker_feature_policy);
  SetFeaturePolicy(std::move(creation_params->worker_feature_policy));
}

void WorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  int next_id = ++last_pending_error_event_id_;
  pending_error_events_.Set(next_id, event);
  ReportingProxy().ReportException(event->MessageForConsole(),
                                   event->Location()->Clone(), next_id);
}

void WorkerGlobalScope::RemoveURLFromMemoryCache(const KURL& url) {
  PostCrossThreadTask(*thread_->GetParentExecutionContextTaskRunners()->Get(
                          TaskType::kNetworking),
                      FROM_HERE,
                      CrossThreadBind(&RemoveURLFromMemoryCacheInternal, url));
}

void WorkerGlobalScope::queueMicrotask(V8VoidFunction* callback) {
  Microtask::EnqueueMicrotask(WTF::Bind(
      &V8PersistentCallbackFunction<V8VoidFunction>::InvokeAndReportException,
      WrapPersistent(ToV8PersistentCallbackFunction(callback)), nullptr));
}

int WorkerGlobalScope::requestAnimationFrame(V8FrameRequestCallback* callback,
                                             ExceptionState& exception_state) {
  FrameRequestCallbackCollection::V8FrameCallback* frame_callback =
      FrameRequestCallbackCollection::V8FrameCallback::Create(callback);
  frame_callback->SetUseLegacyTimeBase(true);

  int ret = animation_frame_provider_->RegisterCallback(frame_callback);

  if (ret == WorkerAnimationFrameProvider::kInvalidCallbackId) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "requestAnimationFrame not supported in this Worker.");
  }

  return ret;
}

void WorkerGlobalScope::cancelAnimationFrame(int id) {
  animation_frame_provider_->CancelCallback(id);
}

void WorkerGlobalScope::SetWorkerSettings(
    std::unique_ptr<WorkerSettings> worker_settings) {
  worker_settings_ = std::move(worker_settings);
  worker_settings_->MakeGenericFontFamilySettingsAtomic();
  font_selector_->UpdateGenericFontFamilySettings(
      worker_settings_->GetGenericFontFamilySettings());
}

TrustedTypePolicyFactory* WorkerGlobalScope::trustedTypes() {
  if (!trusted_types_) {
    trusted_types_ = TrustedTypePolicyFactory::Create(this);
  }
  return trusted_types_.Get();
}

void WorkerGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(location_);
  visitor->Trace(navigator_);
  visitor->Trace(timers_);
  visitor->Trace(pending_error_events_);
  visitor->Trace(font_selector_);
  visitor->Trace(animation_frame_provider_);
  visitor->Trace(trusted_types_);
  WorkerOrWorkletGlobalScope::Trace(visitor);
  Supplementable<WorkerGlobalScope>::Trace(visitor);
}

}  // namespace blink
