// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_response.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_state_resolver.h"
#include "third_party/blink/renderer/modules/payments/payment_validation_errors.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

PaymentResponse::PaymentResponse(
    payments::mojom::blink::PaymentResponsePtr response,
    PaymentAddress* shipping_address,
    PaymentStateResolver* payment_state_resolver,
    const String& requestId)
    : requestId_(requestId),
      method_name_(response->method_name),
      stringified_details_(response->stringified_details),
      shipping_address_(shipping_address),
      shipping_option_(response->shipping_option),
      payer_name_(response->payer_name),
      payer_email_(response->payer_email),
      payer_phone_(response->payer_phone),
      payment_state_resolver_(payment_state_resolver) {
  DCHECK(payment_state_resolver_);
}

PaymentResponse::~PaymentResponse() = default;

ScriptValue PaymentResponse::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("requestId", requestId());
  result.AddString("methodName", methodName());
  result.Add("details", details(script_state, ASSERT_NO_EXCEPTION));

  if (shippingAddress())
    result.Add("shippingAddress",
               shippingAddress()->toJSONForBinding(script_state));
  else
    result.AddNull("shippingAddress");

  result.AddStringOrNull("shippingOption", shippingOption())
      .AddStringOrNull("payerName", payerName())
      .AddStringOrNull("payerEmail", payerEmail())
      .AddStringOrNull("payerPhone", payerPhone());

  return result.GetScriptValue();
}

ScriptValue PaymentResponse::details(ScriptState* script_state,
                                     ExceptionState& exception_state) const {
  return ScriptValue(
      script_state,
      FromJSONString(script_state->GetIsolate(), script_state->GetContext(),
                     stringified_details_, exception_state));
}

ScriptPromise PaymentResponse::complete(ScriptState* script_state,
                                        const String& result) {
  PaymentStateResolver::PaymentComplete converted_result =
      PaymentStateResolver::PaymentComplete::kUnknown;
  if (result == "success")
    converted_result = PaymentStateResolver::PaymentComplete::kSuccess;
  else if (result == "fail")
    converted_result = PaymentStateResolver::PaymentComplete::kFail;
  return payment_state_resolver_->Complete(script_state, converted_result);
}

ScriptPromise PaymentResponse::retry(
    ScriptState* script_state,
    const PaymentValidationErrors& error_fields) {
  return payment_state_resolver_->Retry(script_state, error_fields);
}

void PaymentResponse::Trace(blink::Visitor* visitor) {
  visitor->Trace(shipping_address_);
  visitor->Trace(payment_state_resolver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
