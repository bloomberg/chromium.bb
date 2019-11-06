// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid_connection_event.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

HID::HID(ExecutionContext& context) : ContextLifecycleObserver(&context) {}

HID::~HID() = default;

ScriptPromise HID::getDevices(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

ScriptPromise HID::requestDevice(ScriptState* script_state,
                                 const HIDDeviceRequestOptions* options) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

ExecutionContext* HID::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& HID::InterfaceName() const {
  return event_target_names::kHID;
}

void HID::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);
  // TODO(mattreynolds): Connect to the HID service and register for connect
  // and disconnect events.
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
