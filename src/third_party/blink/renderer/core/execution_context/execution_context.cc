/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/policy_disposition.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/execution_context_csp_delegate.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

ExecutionContext::ExecutionContext(v8::Isolate* isolate, Agent* agent)
    : isolate_(isolate),
      security_context_(this),
      agent_(agent),
      circular_sequential_id_(0),
      in_dispatch_error_event_(false),
      lifecycle_state_(mojom::FrameLifecycleState::kRunning),
      is_context_destroyed_(false),
      csp_delegate_(MakeGarbageCollected<ExecutionContextCSPDelegate>(*this)),
      window_interaction_tokens_(0),
      referrer_policy_(network::mojom::ReferrerPolicy::kDefault),
      address_space_(network::mojom::blink::IPAddressSpace::kUnknown),
      origin_trial_context_(MakeGarbageCollected<OriginTrialContext>(this)) {
  DCHECK(agent_);
}

ExecutionContext::~ExecutionContext() {
  DCHECK(is_context_destroyed_);
}

// static
ExecutionContext* ExecutionContext::From(const ScriptState* script_state) {
  v8::HandleScope scope(script_state->GetIsolate());
  return ToExecutionContext(script_state->GetContext());
}

// static
ExecutionContext* ExecutionContext::From(v8::Local<v8::Context> context) {
  return ToExecutionContext(context);
}

// static
ExecutionContext* ExecutionContext::ForCurrentRealm(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToExecutionContext(info.GetIsolate()->GetCurrentContext());
}

// static
ExecutionContext* ExecutionContext::ForCurrentRealm(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  auto ctx = info.GetIsolate()->GetCurrentContext();
  if (ctx.IsEmpty())
    return nullptr;
  return ToExecutionContext(ctx);
}

// static
ExecutionContext* ExecutionContext::ForRelevantRealm(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToExecutionContext(info.Holder()->CreationContext());
}

// static
ExecutionContext* ExecutionContext::ForRelevantRealm(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  auto ctx = info.Holder()->CreationContext();
  if (ctx.IsEmpty())
    return nullptr;
  return ToExecutionContext(ctx);
}

void ExecutionContext::SetIsInBackForwardCache(bool value) {
  is_in_back_forward_cache_ = value;
}

void ExecutionContext::SetLifecycleState(mojom::FrameLifecycleState state) {
  if (lifecycle_state_ == state)
    return;
  lifecycle_state_ = state;
  ContextLifecycleNotifier::observers().ForEachObserver(
      [&](ContextLifecycleObserver* observer) {
        if (!observer->IsExecutionContextLifecycleObserver())
          return;
        ExecutionContextLifecycleObserver* execution_context_observer =
            static_cast<ExecutionContextLifecycleObserver*>(observer);
        if (execution_context_observer->ObserverType() !=
            ExecutionContextLifecycleObserver::kStateObjectType)
          return;
        ExecutionContextLifecycleStateObserver* state_observer =
            static_cast<ExecutionContextLifecycleStateObserver*>(
                execution_context_observer);
#if DCHECK_IS_ON()
        DCHECK_EQ(state_observer->GetExecutionContext(), this);
        DCHECK(state_observer->UpdateStateIfNeededCalled());
#endif
        state_observer->ContextLifecycleStateChanged(state);
      });
}

void ExecutionContext::NotifyContextDestroyed() {
  is_context_destroyed_ = true;
  ContextLifecycleNotifier::NotifyContextDestroyed();
}

unsigned ExecutionContext::ContextLifecycleStateObserverCountForTesting()
    const {
  DCHECK(!ContextLifecycleNotifier::observers().IsIteratingOverObservers());
  unsigned lifecycle_state_observers = 0;
  ContextLifecycleNotifier::observers().ForEachObserver(
      [&](ContextLifecycleObserver* observer) {
        if (!observer->IsExecutionContextLifecycleObserver())
          return;
        if (static_cast<ExecutionContextLifecycleObserver*>(observer)
                ->ObserverType() !=
            ExecutionContextLifecycleObserver::kStateObjectType)
          return;
        lifecycle_state_observers++;
      });
  return lifecycle_state_observers;
}

bool ExecutionContext::SharedArrayBufferTransferAllowed() const {
  if (RuntimeEnabledFeatures::SharedArrayBufferEnabled(this) ||
      CrossOriginIsolatedCapability()) {
    return true;
  }
#if defined(OS_ANDROID)
  return false;
#else
  return RuntimeEnabledFeatures::UnrestrictedSharedArrayBufferEnabled(this);
#endif
}

namespace {
mojom::blink::InspectorIssueInfoPtr CreateSharedArrayBufferIssue(
    const SourceLocation* source_location) {
  auto details = mojom::blink::InspectorIssueDetails::New();
  auto issue_details = mojom::blink::SharedArrayBufferIssueDetails::New();
  auto affected_location = mojom::blink::AffectedLocation::New();
  affected_location->url = source_location->Url();
  affected_location->line = source_location->LineNumber() - 1;
  affected_location->column = source_location->ColumnNumber();
  affected_location->script_id =
      WTF::String::Number(source_location->ScriptId());
  issue_details->affected_location = std::move(affected_location);
  details->sab_issue_details = std::move(issue_details);
  return mojom::blink::InspectorIssueInfo::New(
      mojom::blink::InspectorIssueCode::kSharedArrayBufferIssue,
      std::move(details));
}
}  // namespace

bool ExecutionContext::CheckSharedArrayBufferTransferAllowedAndReport() {
  const bool allowed = SharedArrayBufferTransferAllowed();
  // File an issue if the transfer is prohibited, or if it will be prohibited
  // in the future, and the problem is encountered for the first time in this
  // execution context. This preserves postMessage performance during the
  // transition period.
  if (!allowed || (!has_filed_shared_array_buffer_transfer_issue_ &&
                   !CrossOriginIsolatedCapability())) {
    has_filed_shared_array_buffer_transfer_issue_ = true;
    auto source_location = SourceLocation::Capture(this);
    auto issue = CreateSharedArrayBufferIssue(source_location.get());
    issue->details->sab_issue_details->is_warning = allowed;
    issue->details->sab_issue_details->type =
        mojom::blink::SharedArrayBufferIssueType::kTransferIssue;
    AddInspectorIssue(std::move(issue));
  }
  return allowed;
}

void ExecutionContext::FileSharedArrayBufferCreationIssue() {
  // This is performance critical, only do it once per context.
  if (has_filed_shared_array_buffer_creation_issue_)
    return;
  has_filed_shared_array_buffer_creation_issue_ = true;
  auto source_location = SourceLocation::Capture(this);
  auto issue = CreateSharedArrayBufferIssue(source_location.get());
  // In enforced mode, the SAB constructor isn't available.
  issue->details->sab_issue_details->is_warning = true;
  issue->details->sab_issue_details->type =
      mojom::blink::SharedArrayBufferIssueType::kCreationIssue;
  AddInspectorIssue(std::move(issue));
}

void ExecutionContext::AddConsoleMessageImpl(mojom::ConsoleMessageSource source,
                                             mojom::ConsoleMessageLevel level,
                                             const String& message,
                                             bool discard_duplicates) {
  AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(source, level, message),
      discard_duplicates);
}

void ExecutionContext::DispatchErrorEvent(
    ErrorEvent* error_event,
    SanitizeScriptErrors sanitize_script_errors) {
  if (in_dispatch_error_event_) {
    pending_exceptions_.push_back(error_event);
    return;
  }

  // First report the original exception and only then all the nested ones.
  if (!DispatchErrorEventInternal(error_event, sanitize_script_errors))
    ExceptionThrown(error_event);

  if (pending_exceptions_.IsEmpty())
    return;
  for (ErrorEvent* e : pending_exceptions_)
    ExceptionThrown(e);
  pending_exceptions_.clear();
}

bool ExecutionContext::DispatchErrorEventInternal(
    ErrorEvent* error_event,
    SanitizeScriptErrors sanitize_script_errors) {
  EventTarget* target = ErrorEventTarget();
  if (!target)
    return false;

  if (sanitize_script_errors == SanitizeScriptErrors::kSanitize) {
    error_event = ErrorEvent::CreateSanitizedError(
        ToScriptState(this, *error_event->World()));
  }

  DCHECK(!in_dispatch_error_event_);
  in_dispatch_error_event_ = true;
  target->DispatchEvent(*error_event);
  in_dispatch_error_event_ = false;
  return error_event->defaultPrevented();
}

bool ExecutionContext::IsContextPaused() const {
  return lifecycle_state_ == mojom::blink::FrameLifecycleState::kPaused;
}

WebURLLoader::DeferType ExecutionContext::DeferType() const {
  if (is_in_back_forward_cache_) {
    DCHECK_EQ(lifecycle_state_, mojom::blink::FrameLifecycleState::kFrozen);
    return WebURLLoader::DeferType::kDeferredWithBackForwardCache;
  } else if (lifecycle_state_ == mojom::blink::FrameLifecycleState::kFrozen ||
             lifecycle_state_ == mojom::blink::FrameLifecycleState::kPaused) {
    return WebURLLoader::DeferType::kDeferred;
  }
  return WebURLLoader::DeferType::kNotDeferred;
}

bool ExecutionContext::IsLoadDeferred() const {
  return lifecycle_state_ == mojom::blink::FrameLifecycleState::kPaused ||
         lifecycle_state_ == mojom::blink::FrameLifecycleState::kFrozen;
}

int ExecutionContext::CircularSequentialID() {
  ++circular_sequential_id_;
  if (circular_sequential_id_ > ((1U << 31) - 1U))
    circular_sequential_id_ = 1;

  return circular_sequential_id_;
}

PublicURLManager& ExecutionContext::GetPublicURLManager() {
  if (!public_url_manager_)
    public_url_manager_ = MakeGarbageCollected<PublicURLManager>(this);
  return *public_url_manager_;
}

ContentSecurityPolicyDelegate&
ExecutionContext::GetContentSecurityPolicyDelegate() {
  return *csp_delegate_;
}

scoped_refptr<const DOMWrapperWorld> ExecutionContext::GetCurrentWorld() const {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();

  // This can be called before we enter v8, hence the context might be empty.
  if (v8_context.IsEmpty())
    return nullptr;

  return &DOMWrapperWorld::Current(isolate);
}

ContentSecurityPolicy*
ExecutionContext::GetContentSecurityPolicyForCurrentWorld() {
  return GetContentSecurityPolicyForWorld(GetCurrentWorld().get());
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicyForWorld(
    const DOMWrapperWorld* world) {
  // Only documents support isolated worlds and only isolated worlds can have
  // their own CSP distinct from the main world CSP. Hence just return the main
  // world's content security policy by default.
  return GetContentSecurityPolicy();
}

const SecurityOrigin* ExecutionContext::GetSecurityOrigin() const {
  return security_context_.GetSecurityOrigin();
}

SecurityOrigin* ExecutionContext::GetMutableSecurityOrigin() {
  return security_context_.GetMutableSecurityOrigin();
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicy() const {
  return security_context_.GetContentSecurityPolicy();
}

network::mojom::blink::WebSandboxFlags ExecutionContext::GetSandboxFlags()
    const {
  return security_context_.GetSandboxFlags();
}

bool ExecutionContext::IsSandboxed(
    network::mojom::blink::WebSandboxFlags mask) const {
  return security_context_.IsSandboxed(mask);
}

const base::UnguessableToken& ExecutionContext::GetAgentClusterID() const {
  return GetAgent()->cluster_id();
}

void ExecutionContext::AllowWindowInteraction() {
  ++window_interaction_tokens_;
}

void ExecutionContext::ConsumeWindowInteraction() {
  if (window_interaction_tokens_ == 0)
    return;
  --window_interaction_tokens_;
}

bool ExecutionContext::IsWindowInteractionAllowed() const {
  return window_interaction_tokens_ > 0;
}

bool ExecutionContext::IsSecureContext(String& error_message) const {
  if (!IsSecureContext()) {
    error_message = SecurityOrigin::IsPotentiallyTrustworthyErrorMessage();
    return false;
  }
  return true;
}

// https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
String ExecutionContext::OutgoingReferrer() const {
  // Step 3.1: "If environment's global object is a Window object, then"
  // This case is implemented in Document::OutgoingReferrer().

  // Step 3.2: "Otherwise, let referrerSource be environment's creation URL."
  return Url().StrippedForUseAsReferrer();
}

void ExecutionContext::ParseAndSetReferrerPolicy(
    const String& policy,
    const ReferrerPolicySource source) {
  network::mojom::ReferrerPolicy referrer_policy;
  bool policy_is_valid = false;

  if (source == kPolicySourceHttpHeader) {
    policy_is_valid = SecurityPolicy::ReferrerPolicyFromHeaderValue(
        policy, kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  } else if (source == kPolicySourceMetaTag) {
    policy_is_valid = (SecurityPolicy::ReferrerPolicyFromString(
        policy, kSupportReferrerPolicyLegacyKeywords, &referrer_policy));
  } else {
    NOTREACHED();
    return;
  }

  if (policy_is_valid) {
    SetReferrerPolicy(referrer_policy);
  } else {
    String error_reason;
    if (source == kPolicySourceMetaTag && policy.Contains(',')) {
      // Only a single token is permitted for Meta-specified policies
      // (https://crbug.com/1093914).
      error_reason =
          "A policy specified by a meta element must contain only one token.";
    } else {
      error_reason =
          "The value '" + policy + "' is not one of " +
          ((source == kPolicySourceMetaTag)
               ? "'always', 'default', 'never', 'origin-when-crossorigin', "
               : "") +
          "'no-referrer', 'no-referrer-when-downgrade', 'origin', "
          "'origin-when-cross-origin', 'same-origin', 'strict-origin', "
          "'strict-origin-when-cross-origin', or 'unsafe-url'.";
    }

    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kError,
        "Failed to set referrer policy: " + error_reason +
            " The referrer policy has been left unchanged."));
  }
}

void ExecutionContext::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  // When a referrer policy has already been set, the latest value takes
  // precedence.
  UseCounter::Count(this, WebFeature::kSetReferrerPolicy);
  if (referrer_policy_ != network::mojom::ReferrerPolicy::kDefault)
    UseCounter::Count(this, WebFeature::kResetReferrerPolicy);

  referrer_policy_ = referrer_policy;
}

void ExecutionContext::RemoveURLFromMemoryCache(const KURL& url) {
  GetMemoryCache()->RemoveURLFromCache(url);
}

void ExecutionContext::Trace(Visitor* visitor) const {
  visitor->Trace(security_context_);
  visitor->Trace(agent_);
  visitor->Trace(public_url_manager_);
  visitor->Trace(pending_exceptions_);
  visitor->Trace(csp_delegate_);
  visitor->Trace(timers_);
  visitor->Trace(origin_trial_context_);
  ContextLifecycleNotifier::Trace(visitor);
  ConsoleLogger::Trace(visitor);
  Supplementable<ExecutionContext>::Trace(visitor);
}

bool ExecutionContext::IsSameAgentCluster(
    const base::UnguessableToken& other_id) const {
  base::UnguessableToken this_id = GetAgentClusterID();
  // If the AgentClusterID is empty then it should never be the same (e.g.
  // currently for worklets).
  if (this_id.is_empty() || other_id.is_empty())
    return false;
  return this_id == other_id;
}

v8::MicrotaskQueue* ExecutionContext::GetMicrotaskQueue() const {
  DCHECK(GetAgent());
  DCHECK(GetAgent()->event_loop());
  return GetAgent()->event_loop()->microtask_queue();
}

bool ExecutionContext::FeatureEnabled(OriginTrialFeature feature) const {
  return origin_trial_context_->IsFeatureEnabled(feature);
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::FeaturePolicyFeature feature,
    ReportOptions report_on_failure,
    const String& message) const {
  if (report_on_failure == ReportOptions::kReportOnFailure) {
    // We are expecting a violation report in case the feature is disabled in
    // the context. Therefore, this qualifies as a potential violation (i.e.,
    // if the feature was disabled it would generate a report).
    CountPotentialFeaturePolicyViolation(feature);
  }

  bool should_report;
  bool enabled = security_context_.IsFeatureEnabled(feature, &should_report);

  if (should_report && report_on_failure == ReportOptions::kReportOnFailure) {
    mojom::blink::PolicyDisposition disposition =
        enabled ? mojom::blink::PolicyDisposition::kReport
                : mojom::blink::PolicyDisposition::kEnforce;
    ReportFeaturePolicyViolation(feature, disposition, message);
  }
  return enabled;
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    ReportOptions report_option,
    const String& message,
    const String& source_file) const {
  DCHECK(GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type() ==
         mojom::blink::PolicyValueType::kBool);
  return IsFeatureEnabled(feature, PolicyValue::CreateBool(true), report_option,
                          message, source_file);
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    PolicyValue threshold_value,
    ReportOptions report_option,
    const String& message,
    const String& source_file) const {
  // The default value for any feature should be true unless restricted by
  // document policy
  if (!RuntimeEnabledFeatures::DocumentPolicyEnabled())
    return true;

  SecurityContext::FeatureStatus status =
      security_context_.IsFeatureEnabled(feature, threshold_value);
  if (status.should_report &&
      report_option == ReportOptions::kReportOnFailure) {
    // If both |enabled| and |should_report| are true, the usage must have
    // violated the report-only policy, i.e. |disposition| ==
    // mojom::blink::PolicyDisposition::kReport.
    ReportDocumentPolicyViolation(
        feature,
        status.enabled ? mojom::blink::PolicyDisposition::kReport
                       : mojom::blink::PolicyDisposition::kEnforce,
        message, source_file);
  }
  return status.enabled;
}

bool ExecutionContext::RequireTrustedTypes() const {
  return security_context_.TrustedTypesRequiredByPolicy() &&
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(this);
}

String ExecutionContext::addressSpaceForBindings() const {
  switch (address_space_) {
    case network::mojom::IPAddressSpace::kPublic:
    case network::mojom::IPAddressSpace::kUnknown:
      return "public";

    case network::mojom::IPAddressSpace::kPrivate:
      return "private";

    case network::mojom::IPAddressSpace::kLocal:
      return "local";
  }
  NOTREACHED();
  return "public";
}

}  // namespace blink
