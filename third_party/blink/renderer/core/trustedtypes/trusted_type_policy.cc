// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"

namespace blink {

TrustedTypePolicy* TrustedTypePolicy::Create(const String& policyName) {
  return new TrustedTypePolicy(policyName);
}

String TrustedTypePolicy::name() const {
  return name_;
}

TrustedTypePolicy::TrustedTypePolicy(const String& policyName) {
  name_ = policyName;
}

}  // namespace blink
