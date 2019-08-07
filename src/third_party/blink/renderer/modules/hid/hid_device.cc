// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_device.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid_collection_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

HIDDevice::HIDDevice(device::mojom::blink::HidDeviceInfoPtr info,
                     ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

HIDDevice::~HIDDevice() = default;

ExecutionContext* HIDDevice::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& HIDDevice::InterfaceName() const {
  return event_target_names::kHIDDevice;
}

bool HIDDevice::opened() const {
  return false;
}

uint16_t HIDDevice::vendorId() const {
  return 0;
}

uint16_t HIDDevice::productId() const {
  return 0;
}

String HIDDevice::productName() const {
  return String();
}

const HeapVector<Member<HIDCollectionInfo>>& HIDDevice::collections() const {
  return collections_;
}

ScriptPromise HIDDevice::open(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

ScriptPromise HIDDevice::close(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

ScriptPromise HIDDevice::sendReport(ScriptState* script_state,
                                    uint8_t report_id,
                                    const ArrayBufferOrArrayBufferView& data) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

ScriptPromise HIDDevice::sendFeatureReport(
    ScriptState* script_state,
    uint8_t report_id,
    const ArrayBufferOrArrayBufferView& data) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

ScriptPromise HIDDevice::receiveFeatureReport(ScriptState* script_state,
                                              uint8_t report_id) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not supported."));
}

void HIDDevice::AddedEventListener(const AtomicString& event_type,
                                   RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);
  // TODO(mattreynolds): Open a HID connection and register for input report
  // events.
}

void HIDDevice::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(collections_);
}

}  // namespace blink
