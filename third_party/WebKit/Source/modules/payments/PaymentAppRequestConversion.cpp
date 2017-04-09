// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppRequestConversion.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "modules/payments/PaymentAppRequest.h"
#include "modules/payments/PaymentCurrencyAmount.h"
#include "modules/payments/PaymentDetailsModifier.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/PaymentMethodData.h"
#include "public/platform/modules/payments/WebPaymentAppRequest.h"
#include "public/platform/modules/payments/WebPaymentMethodData.h"

namespace blink {
namespace {

PaymentCurrencyAmount ToPaymentCurrencyAmount(
    const WebPaymentCurrencyAmount& web_amount) {
  PaymentCurrencyAmount amount;
  amount.setCurrency(web_amount.currency);
  amount.setValue(web_amount.value);
  amount.setCurrencySystem(web_amount.currency_system);
  return amount;
}

PaymentItem ToPaymentItem(const WebPaymentItem& web_item) {
  PaymentItem item;
  item.setLabel(web_item.label);
  item.setAmount(ToPaymentCurrencyAmount(web_item.amount));
  item.setPending(web_item.pending);
  return item;
}

PaymentDetailsModifier ToPaymentDetailsModifier(
    ScriptState* script_state,
    const WebPaymentDetailsModifier& web_modifier) {
  PaymentDetailsModifier modifier;
  Vector<String> supported_methods;
  for (const auto& web_method : web_modifier.supported_methods) {
    supported_methods.push_back(web_method);
  }
  modifier.setSupportedMethods(supported_methods);
  modifier.setTotal(ToPaymentItem(web_modifier.total));
  HeapVector<PaymentItem> additional_display_items;
  for (const auto& web_item : web_modifier.additional_display_items) {
    additional_display_items.push_back(ToPaymentItem(web_item));
  }
  modifier.setAdditionalDisplayItems(additional_display_items);
  return modifier;
}

ScriptValue StringDataToScriptValue(ScriptState* script_state,
                                    const WebString& stringified_data) {
  if (!script_state->ContextIsValid())
    return ScriptValue();

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> v8_value;
  if (!v8::JSON::Parse(script_state->GetIsolate(),
                       V8String(script_state->GetIsolate(), stringified_data))
           .ToLocal(&v8_value)) {
    return ScriptValue();
  }
  return ScriptValue(script_state, v8_value);
}

PaymentMethodData ToPaymentMethodData(
    ScriptState* script_state,
    const WebPaymentMethodData& web_method_data) {
  PaymentMethodData method_data;
  Vector<String> supported_methods;
  for (const auto& method : web_method_data.supported_methods) {
    supported_methods.push_back(method);
  }
  method_data.setSupportedMethods(supported_methods);
  method_data.setData(
      StringDataToScriptValue(script_state, web_method_data.stringified_data));
  return method_data;
}

}  // namespace

PaymentAppRequest PaymentAppRequestConversion::ToPaymentAppRequest(
    ScriptState* script_state,
    const WebPaymentAppRequest& web_app_request) {
  DCHECK(script_state);

  PaymentAppRequest app_request;
  if (!script_state->ContextIsValid())
    return app_request;

  ScriptState::Scope scope(script_state);

  app_request.setOrigin(web_app_request.origin);
  HeapVector<PaymentMethodData> method_data;
  for (const auto& md : web_app_request.method_data) {
    method_data.push_back(ToPaymentMethodData(script_state, md));
  }
  app_request.setMethodData(method_data);
  app_request.setTotal(ToPaymentItem(web_app_request.total));
  HeapVector<PaymentDetailsModifier> modifiers;
  for (const auto& modifier : web_app_request.modifiers) {
    modifiers.push_back(ToPaymentDetailsModifier(script_state, modifier));
  }
  app_request.setOptionId(web_app_request.option_id);
  return app_request;
}

}  // namespace blink
