// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentResponse.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentCompleter.h"
#include "platform/wtf/Assertions.h"

namespace blink {

PaymentResponse::PaymentResponse(
    payments::mojom::blink::PaymentResponsePtr response,
    PaymentAddress* shipping_address,
    PaymentCompleter* payment_completer,
    const String& requestId)
    : requestId_(requestId),
      method_name_(response->method_name),
      stringified_details_(response->stringified_details),
      shipping_address_(shipping_address),
      shipping_option_(response->shipping_option),
      payer_name_(response->payer_name),
      payer_email_(response->payer_email),
      payer_phone_(response->payer_phone),
      payment_completer_(payment_completer) {
  DCHECK(payment_completer_);
}

PaymentResponse::~PaymentResponse() {}

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
  return ScriptValue(script_state,
                     FromJSONString(script_state->GetIsolate(),
                                    stringified_details_, exception_state));
}

ScriptPromise PaymentResponse::complete(ScriptState* script_state,
                                        const String& result) {
  PaymentCompleter::PaymentComplete converted_result =
      PaymentCompleter::kUnknown;
  if (result == "success")
    converted_result = PaymentCompleter::kSuccess;
  else if (result == "fail")
    converted_result = PaymentCompleter::kFail;
  return payment_completer_->Complete(script_state, converted_result);
}

void PaymentResponse::Trace(blink::Visitor* visitor) {
  visitor->Trace(shipping_address_);
  visitor->Trace(payment_completer_);
}

}  // namespace blink
