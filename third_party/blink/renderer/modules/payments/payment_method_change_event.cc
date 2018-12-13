// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_method_change_event.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

PaymentMethodChangeEvent::~PaymentMethodChangeEvent() = default;

// static
PaymentMethodChangeEvent* PaymentMethodChangeEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const PaymentMethodChangeEventInit* init) {
  return MakeGarbageCollected<PaymentMethodChangeEvent>(script_state, type,
                                                        init);
}

const String& PaymentMethodChangeEvent::methodName() const {
  return method_name_;
}

const ScriptValue PaymentMethodChangeEvent::methodDetails(
    ScriptState* script_state) const {
  if (method_details_.IsEmpty())
    return ScriptValue::CreateNull(script_state);
  return ScriptValue(script_state,
                     method_details_.GetAcrossWorld(script_state));
}

void PaymentMethodChangeEvent::Trace(Visitor* visitor) {
  visitor->Trace(method_details_);
  PaymentRequestUpdateEvent::Trace(visitor);
}

PaymentMethodChangeEvent::PaymentMethodChangeEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const PaymentMethodChangeEventInit* init)
    : PaymentRequestUpdateEvent(ExecutionContext::From(script_state),
                                type,
                                init),
      method_name_(init->methodName()) {
  if (init->hasMethodDetails()) {
    method_details_.Set(init->methodDetails().GetIsolate(),
                        init->methodDetails().V8Value());
  }
}

}  // namespace blink
