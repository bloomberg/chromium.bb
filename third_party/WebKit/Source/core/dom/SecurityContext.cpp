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

#include "core/dom/SecurityContext.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"

namespace blink {

// static
std::vector<unsigned> SecurityContext::SerializeInsecureNavigationSet(
    const InsecureNavigationsSet& set) {
  // The set is serialized as a sorted array. Sorting it makes it easy to know
  // if two serialized sets are equal.
  std::vector<unsigned> serialized;
  serialized.reserve(set.size());
  for (unsigned host : set)
    serialized.push_back(host);
  std::sort(serialized.begin(), serialized.end());

  return serialized;
}

SecurityContext::SecurityContext()
    : sandbox_flags_(kSandboxNone),
      address_space_(mojom::IPAddressSpace::kPublic),
      insecure_request_policy_(kLeaveInsecureRequestsAlone),
      require_safe_types_(false) {}

SecurityContext::~SecurityContext() = default;

void SecurityContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(content_security_policy_);
}

void SecurityContext::SetSecurityOrigin(
    scoped_refptr<SecurityOrigin> security_origin) {
  security_origin_ = std::move(security_origin);
  UpdateFeaturePolicyOrigin();
}

void SecurityContext::SetContentSecurityPolicy(
    ContentSecurityPolicy* content_security_policy) {
  content_security_policy_ = content_security_policy;
}

void SecurityContext::EnforceSandboxFlags(SandboxFlags mask) {
  ApplySandboxFlags(mask);
}

void SecurityContext::ApplySandboxFlags(SandboxFlags mask,
                                        bool is_potentially_trustworthy) {
  sandbox_flags_ |= mask;

  if (IsSandboxed(kSandboxOrigin) && GetSecurityOrigin() &&
      !GetSecurityOrigin()->IsUnique()) {
    scoped_refptr<SecurityOrigin> security_origin =
        SecurityOrigin::CreateUnique();
    security_origin->SetUniqueOriginIsPotentiallyTrustworthy(
        is_potentially_trustworthy);
    SetSecurityOrigin(std::move(security_origin));
    DidUpdateSecurityOrigin();
  }
}

String SecurityContext::addressSpaceForBindings() const {
  switch (address_space_) {
    case mojom::IPAddressSpace::kPublic:
      return "public";

    case mojom::IPAddressSpace::kPrivate:
      return "private";

    case mojom::IPAddressSpace::kLocal:
      return "local";
  }
  NOTREACHED();
  return "public";
}

// Enforces the given suborigin as part of the security origin for this
// security context. |name| must not be empty, although it may be null. A null
// name represents a lack of a suborigin.
// See: https://w3c.github.io/webappsec-suborigins/index.html
void SecurityContext::EnforceSuborigin(const Suborigin& suborigin) {
  if (!RuntimeEnabledFeatures::SuboriginsEnabled())
    return;

  DCHECK(!suborigin.GetName().IsEmpty());
  DCHECK(RuntimeEnabledFeatures::SuboriginsEnabled());
  DCHECK(security_origin_.get());
  DCHECK(!security_origin_->HasSuborigin() ||
         security_origin_->GetSuborigin()->GetName() == suborigin.GetName());
  security_origin_->AddSuborigin(suborigin);
  DidUpdateSecurityOrigin();
}

void SecurityContext::InitializeFeaturePolicy(
    const ParsedFeaturePolicy& parsed_header,
    const ParsedFeaturePolicy& container_policy,
    const FeaturePolicy* parent_feature_policy) {
  feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
      parent_feature_policy, container_policy, security_origin_->ToUrlOrigin());
  feature_policy_->SetHeaderPolicy(parsed_header);
}

void SecurityContext::UpdateFeaturePolicyOrigin() {
  if (!feature_policy_)
    return;
  feature_policy_ = FeaturePolicy::CreateFromPolicyWithOrigin(
      *feature_policy_, security_origin_->ToUrlOrigin());
}

}  // namespace blink
