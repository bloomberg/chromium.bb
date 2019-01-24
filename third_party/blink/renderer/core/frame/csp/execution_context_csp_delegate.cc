// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/execution_context_csp_delegate.h"

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/security_policy_violation_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_service_proxy_ptr_holder.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ExecutionContextCSPDelegate::ExecutionContextCSPDelegate(
    ExecutionContext& execution_context)
    : execution_context_(&execution_context) {}

void ExecutionContextCSPDelegate::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  ContentSecurityPolicyDelegate::Trace(visitor);
}

const SecurityOrigin* ExecutionContextCSPDelegate::GetSecurityOrigin() {
  return execution_context_->GetSecurityOrigin();
}

const KURL& ExecutionContextCSPDelegate::Url() const {
  return execution_context_->Url();
}

void ExecutionContextCSPDelegate::SetSandboxFlags(SandboxFlags mask) {
  GetSecurityContext().EnforceSandboxFlags(mask);
}

void ExecutionContextCSPDelegate::SetAddressSpace(mojom::IPAddressSpace space) {
  GetSecurityContext().SetAddressSpace(space);
}

void ExecutionContextCSPDelegate::SetRequireTrustedTypes() {
  if (origin_trials::TrustedDOMTypesEnabled(execution_context_))
    GetSecurityContext().SetRequireTrustedTypes();
}

void ExecutionContextCSPDelegate::AddInsecureRequestPolicy(
    WebInsecureRequestPolicy policy) {
  SecurityContext& security_context = GetSecurityContext();

  Document* document = GetDocument();

  // Step 2. Set settings’s insecure requests policy to Upgrade. [spec text]
  // Upgrade Insecure Requests: Update the policy.
  security_context.SetInsecureRequestPolicy(
      security_context.GetInsecureRequestPolicy() | policy);
  if (document)
    document->DidEnforceInsecureRequestPolicy();

  // Upgrade Insecure Requests: Update the set of insecure URLs to upgrade.
  if (policy & kUpgradeInsecureRequests) {
    // Spec: Enforcing part of:
    // https://w3c.github.io/webappsec-upgrade-insecure-requests/#delivery
    // Step 3. Let tuple be a tuple of the protected resource’s URL's host and
    // port. [spec text]
    // Step 4. Insert tuple into settings’s upgrade insecure navigations set.
    // [spec text]
    Count(WebFeature::kUpgradeInsecureRequestsEnabled);
    if (!Url().Host().IsEmpty()) {
      uint32_t hash = Url().Host().Impl()->GetHash();
      security_context.AddInsecureNavigationUpgrade(hash);
      if (document)
        document->DidEnforceInsecureNavigationsSet();
    }
  }
}

std::unique_ptr<SourceLocation>
ExecutionContextCSPDelegate::GetSourceLocation() {
  return SourceLocation::Capture(execution_context_);
}

base::Optional<uint16_t> ExecutionContextCSPDelegate::GetStatusCode() {
  base::Optional<uint16_t> status_code;

  // TODO(mkwst): We only have status code information for Documents. It would
  // be nice to get them for Workers as well.
  Document* document = GetDocument();
  if (document && !SecurityOrigin::IsSecure(document->Url()) &&
      document->Loader()) {
    status_code = document->Loader()->GetResponse().HttpStatusCode();
  }

  return status_code;
}

String ExecutionContextCSPDelegate::GetDocumentReferrer() {
  String referrer;

  // TODO(mkwst): We only have referrer information for Documents. It would be
  // nice to get them for Workers as well.
  if (Document* document = GetDocument())
    referrer = document->referrer();
  return referrer;
}

void ExecutionContextCSPDelegate::DispatchViolationEvent(
    const SecurityPolicyViolationEventInit& violation_data,
    Element* element) {
  execution_context_->GetTaskRunner(TaskType::kNetworking)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(
              &ExecutionContextCSPDelegate::DispatchViolationEventInternal,
              WrapPersistent(this), WrapPersistent(&violation_data),
              WrapPersistent(element)));
}

void ExecutionContextCSPDelegate::PostViolationReport(
    const SecurityPolicyViolationEventInit& violation_data,
    const String& stringified_report,
    bool is_frame_ancestors_violation,
    const Vector<String>& report_endpoints,
    bool use_reporting_api) {
  DCHECK_EQ(is_frame_ancestors_violation,
            ContentSecurityPolicy::DirectiveType::kFrameAncestors ==
                ContentSecurityPolicy::GetDirectiveType(
                    violation_data.effectiveDirective()));

  // TODO(mkwst): Support POSTing violation reports from a Worker.
  Document* document = GetDocument();
  if (!document)
    return;

  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return;

  scoped_refptr<EncodedFormData> report =
      EncodedFormData::Create(stringified_report.Utf8());

  DEFINE_STATIC_LOCAL(ReportingServiceProxyPtrHolder,
                      reporting_service_proxy_holder, ());

  for (const auto& report_endpoint : report_endpoints) {
    if (use_reporting_api) {
      // https://w3c.github.io/webappsec-csp/#report-violation
      // Step 3.5. If violation’s policy’s directive set contains a directive
      // named "report-to" (directive): [spec text]
      //
      // https://w3c.github.io/reporting/#queue-report
      // Step 2. If url was not provided by the caller, let url be settings’s
      // creation URL. [spec text]
      reporting_service_proxy_holder.QueueCspViolationReport(
          Url(), report_endpoint, &violation_data);
      continue;
    }

    // Use the frame's document to complete the endpoint URL, overriding its URL
    // with the blocked document's URL.
    // https://w3c.github.io/webappsec-csp/#report-violation
    // Step 3.4.2.1. Let endpoint be the result of executing the URL parser with
    // token as the input, and violation’s url as the base URL. [spec text]
    KURL url = is_frame_ancestors_violation
                   ? document->CompleteURLWithOverride(
                         report_endpoint, KURL(violation_data.blockedURI()))
                   // We use the FallbackBaseURL to ensure that we don't
                   // respect base elements when determining the report
                   // endpoint URL.
                   // Note: According to Step 3.4.2.1 mentioned above, the base
                   // URL is "violation’s url" which should be violation's
                   // global object's URL. So using FallbackBaseURL() might be
                   // inconsistent.
                   : document->CompleteURLWithOverride(
                         report_endpoint, document->FallbackBaseURL());
    PingLoader::SendViolationReport(
        frame, url, report, PingLoader::kContentSecurityPolicyViolationReport);
  }
}

void ExecutionContextCSPDelegate::Count(WebFeature feature) {
  UseCounter::Count(execution_context_, feature);
}

void ExecutionContextCSPDelegate::AddConsoleMessage(
    ConsoleMessage* console_message) {
  execution_context_->AddConsoleMessage(console_message);
}

void ExecutionContextCSPDelegate::DisableEval(const String& error_message) {
  execution_context_->DisableEval(error_message);
}

void ExecutionContextCSPDelegate::ReportBlockedScriptExecutionToInspector(
    const String& directive_text) {
  probe::scriptExecutionBlockedByCSP(execution_context_, directive_text);
}

void ExecutionContextCSPDelegate::DidAddContentSecurityPolicies(
    const blink::WebVector<WebContentSecurityPolicy>& policies) {
  Document* document = GetDocument();
  if (document && document->GetFrame())
    document->GetFrame()->Client()->DidAddContentSecurityPolicies(policies);
}

SecurityContext& ExecutionContextCSPDelegate::GetSecurityContext() {
  return execution_context_->GetSecurityContext();
}

Document* ExecutionContextCSPDelegate::GetDocument() {
  return DynamicTo<Document>(execution_context_.Get());
}

void ExecutionContextCSPDelegate::DispatchViolationEventInternal(
    const SecurityPolicyViolationEventInit* violation_data,
    Element* element) {
  // Worklets don't support Events in general.
  if (execution_context_->IsWorkletGlobalScope())
    return;

  // https://w3c.github.io/webappsec-csp/#report-violation.
  // Step 3.1. If target is not null, and global is a Window, and target’s
  // shadow-including root is not global’s associated Document, set target to
  // null. [spec text]
  // Step 3.2. If target is null:
  //    Step 3.2.1. Set target be violation’s global object.
  //    Step 3.2.2. If target is a Window, set target to target’s associated
  //    Document. [spec text]
  // Step 3.3. Fire an event named securitypolicyviolation that uses the
  // SecurityPolicyViolationEvent interface at target.. [spec text]
  SecurityPolicyViolationEvent& event = *SecurityPolicyViolationEvent::Create(
      event_type_names::kSecuritypolicyviolation, violation_data);
  DCHECK(event.bubbles());

  if (auto* document = GetDocument()) {
    if (element && element->isConnected() && element->GetDocument() == document)
      element->EnqueueEvent(event, TaskType::kInternalDefault);
    else
      document->EnqueueEvent(event, TaskType::kInternalDefault);
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_)) {
    scope->EnqueueEvent(event, TaskType::kInternalDefault);
  }
}

}  // namespace blink
