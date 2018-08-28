// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

TrustedTypePolicy* TrustedTypePolicyFactory::createPolicy(
    const String& policy_name,
    const TrustedTypePolicyOptions& policy_options,
    ExceptionState& exception_state) {
  for (const TrustedTypePolicy* p : policies_) {
    if (p->name() == policy_name) {
      exception_state.ThrowTypeError("Policy with name " + policy_name +
                                     " already exists.");
      return nullptr;
    }
  }
  TrustedTypePolicy* policy =
      TrustedTypePolicy::Create(policy_name, policy_options);
  policies_.push_back(policy);
  return policy;
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(LocalFrame* frame)
    : DOMWindowClient(frame) {}

Vector<String> TrustedTypePolicyFactory::getPolicyNames() const {
  Vector<String> policyNames;
  for (const TrustedTypePolicy* p : policies_) {
    policyNames.push_back(p->name());
  }
  return policyNames;
}

void TrustedTypePolicyFactory::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  visitor->Trace(policies_);
}

}  // namespace blink
