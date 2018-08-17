// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ScriptPromise TrustedTypePolicyFactory::createPolicy(
    ScriptState* script_state,
    const String& policy_name,
    const TrustedTypePolicyOptions& policy_options) {
  TrustedTypePolicy* policy =
      TrustedTypePolicy::Create(policy_name, policy_options);
  return ScriptPromise::Cast(script_state, ToV8(policy, script_state));
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(LocalFrame* frame)
    : DOMWindowClient(frame) {}

void TrustedTypePolicyFactory::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
}

}  // namespace blink
