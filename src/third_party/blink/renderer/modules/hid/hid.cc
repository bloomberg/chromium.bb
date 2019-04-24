// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

HID::HID(ExecutionContext& context) : ContextLifecycleObserver(&context) {}

HID::~HID() = default;

ExecutionContext* HID::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& HID::InterfaceName() const {
  return event_target_names::kHID;
}

FeatureEnabledState HID::GetFeatureEnabledState() const {
  return GetExecutionContext()->GetSecurityContext().GetFeatureEnabledState(
      mojom::FeaturePolicyFeature::kHid);
}

void HID::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
