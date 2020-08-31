/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/execution_context/security_context.h"

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
WTF::Vector<unsigned> SecurityContext::SerializeInsecureNavigationSet(
    const InsecureNavigationsSet& set) {
  // The set is serialized as a sorted array. Sorting it makes it easy to know
  // if two serialized sets are equal.
  WTF::Vector<unsigned> serialized;
  serialized.ReserveCapacity(set.size());
  for (unsigned host : set)
    serialized.emplace_back(host);
  std::sort(serialized.begin(), serialized.end());

  return serialized;
}

SecurityContext::SecurityContext(const SecurityContextInit& init,
                                 SecurityContextType context_type)
    : sandbox_flags_(init.GetSandboxFlags()),
      security_origin_(init.GetSecurityOrigin()),
      feature_policy_(init.CreateFeaturePolicy()),
      report_only_feature_policy_(init.CreateReportOnlyFeaturePolicy()),
      document_policy_(init.CreateDocumentPolicy()),
      report_only_document_policy_(init.CreateReportOnlyDocumentPolicy()),
      content_security_policy_(init.GetCSP()),
      address_space_(network::mojom::IPAddressSpace::kUnknown),
      insecure_request_policy_(
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone),
      require_safe_types_(false),
      context_type_for_asserts_(context_type),
      agent_(init.GetAgent()),
      secure_context_mode_(init.GetSecureContextMode()),
      origin_trial_context_(init.GetOriginTrialContext()),
      bind_csp_immediately_(init.BindCSPImmediately()) {}

void SecurityContext::Trace(Visitor* visitor) {
  visitor->Trace(content_security_policy_);
  visitor->Trace(agent_);
  visitor->Trace(origin_trial_context_);
}

void SecurityContext::SetSecurityOrigin(
    scoped_refptr<SecurityOrigin> security_origin) {
  // Enforce that we don't change access, we might change the reference (via
  // IsolatedCopy but we can't change the security policy).
  CHECK(security_origin);
  // The purpose of this check is to ensure that the SecurityContext does not
  // change after script has executed in the ExecutionContext. If this is a
  // RemoteSecurityContext, then there is no local script execution and the
  // context is permitted to represent multiple origins over its lifetime, so it
  // is safe for the SecurityOrigin to change.
  // NOTE: A worker may need to make its origin opaque after the main worker
  // script is loaded if the worker is origin-sandboxed. Specifically exempt
  // that transition. See https://crbug.com/1068008. It would be great if we
  // could get rid of this exemption.
  bool is_worker_transition_to_opaque =
      context_type_for_asserts_ == kWorker &&
      IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin) &&
      security_origin->IsOpaque() &&
      security_origin->GetOriginOrPrecursorOriginIfOpaque() == security_origin_;
  CHECK(context_type_for_asserts_ == kRemoteFrame || !security_origin_ ||
        security_origin_->CanAccess(security_origin.get()) ||
        is_worker_transition_to_opaque);
  security_origin_ = std::move(security_origin);
}

void SecurityContext::SetSecurityOriginForTesting(
    scoped_refptr<SecurityOrigin> security_origin) {
  security_origin_ = std::move(security_origin);
}

void SecurityContext::SetContentSecurityPolicy(
    ContentSecurityPolicy* content_security_policy) {
  content_security_policy_ = content_security_policy;
}

bool SecurityContext::IsSandboxed(
    network::mojom::blink::WebSandboxFlags mask) const {
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    mojom::blink::FeaturePolicyFeature feature =
        FeaturePolicy::FeatureForSandboxFlag(mask);
    if (feature != mojom::blink::FeaturePolicyFeature::kNotFound)
      return !feature_policy_->IsFeatureEnabled(feature);
  }
  return (sandbox_flags_ & mask) !=
         network::mojom::blink::WebSandboxFlags::kNone;
}

void SecurityContext::ApplySandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  sandbox_flags_ |= flags;
}

void SecurityContext::SetRequireTrustedTypes() {
  DCHECK(require_safe_types_ ||
         content_security_policy_->IsRequireTrustedTypes());
  require_safe_types_ = true;
}

void SecurityContext::SetRequireTrustedTypesForTesting() {
  require_safe_types_ = true;
}

bool SecurityContext::TrustedTypesRequiredByPolicy() const {
  return require_safe_types_;
}

void SecurityContext::SetFeaturePolicy(
    std::unique_ptr<FeaturePolicy> feature_policy) {
  feature_policy_ = std::move(feature_policy);
}

void SecurityContext::SetDocumentPolicyForTesting(
    std::unique_ptr<DocumentPolicy> document_policy) {
  document_policy_ = std::move(document_policy);
}

bool SecurityContext::IsFeatureEnabled(
    mojom::blink::FeaturePolicyFeature feature,
    bool* should_report) const {
  DCHECK(feature_policy_);
  bool feature_policy_result = feature_policy_->IsFeatureEnabled(feature);
  bool report_only_feature_policy_result =
      !report_only_feature_policy_ ||
      report_only_feature_policy_->IsFeatureEnabled(feature);

  if (should_report) {
    *should_report =
        !feature_policy_result || !report_only_feature_policy_result;
  }

  return feature_policy_result;
}

bool SecurityContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature) const {
  DCHECK(GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type() ==
         mojom::blink::PolicyValueType::kBool);
  return IsFeatureEnabled(feature, PolicyValue(true)).enabled;
}

SecurityContext::FeatureStatus SecurityContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    PolicyValue threshold_value) const {
  DCHECK(document_policy_);
  bool policy_result =
      document_policy_->IsFeatureEnabled(feature, threshold_value);
  bool report_only_policy_result =
      !report_only_document_policy_ ||
      report_only_document_policy_->IsFeatureEnabled(feature, threshold_value);
  return {policy_result, !policy_result || !report_only_policy_result};
}

}  // namespace blink
