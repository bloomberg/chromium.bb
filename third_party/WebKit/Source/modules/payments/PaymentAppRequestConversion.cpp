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

PaymentCurrencyAmount toPaymentCurrencyAmount(
    const WebPaymentCurrencyAmount& webAmount) {
  PaymentCurrencyAmount amount;
  amount.setCurrency(webAmount.currency);
  amount.setValue(webAmount.value);
  amount.setCurrencySystem(webAmount.currencySystem);
  return amount;
}

PaymentItem toPaymentItem(const WebPaymentItem& webItem) {
  PaymentItem item;
  item.setLabel(webItem.label);
  item.setAmount(toPaymentCurrencyAmount(webItem.amount));
  item.setPending(webItem.pending);
  return item;
}

PaymentDetailsModifier toPaymentDetailsModifier(
    ScriptState* scriptState,
    const WebPaymentDetailsModifier& webModifier) {
  PaymentDetailsModifier modifier;
  Vector<String> supportedMethods;
  for (const auto& webMethod : webModifier.supportedMethods) {
    supportedMethods.push_back(webMethod);
  }
  modifier.setSupportedMethods(supportedMethods);
  modifier.setTotal(toPaymentItem(webModifier.total));
  HeapVector<PaymentItem> additionalDisplayItems;
  for (const auto& webItem : webModifier.additionalDisplayItems) {
    additionalDisplayItems.push_back(toPaymentItem(webItem));
  }
  modifier.setAdditionalDisplayItems(additionalDisplayItems);
  return modifier;
}

ScriptValue stringDataToScriptValue(ScriptState* scriptState,
                                    const WebString& stringifiedData) {
  if (!scriptState->contextIsValid())
    return ScriptValue();

  ScriptState::Scope scope(scriptState);
  v8::Local<v8::Value> v8Value;
  if (!v8::JSON::Parse(scriptState->isolate(),
                       v8String(scriptState->isolate(), stringifiedData))
           .ToLocal(&v8Value)) {
    return ScriptValue();
  }
  return ScriptValue(scriptState, v8Value);
}

PaymentMethodData toPaymentMethodData(
    ScriptState* scriptState,
    const WebPaymentMethodData& webMethodData) {
  PaymentMethodData methodData;
  Vector<String> supportedMethods;
  for (const auto& method : webMethodData.supportedMethods) {
    supportedMethods.push_back(method);
  }
  methodData.setSupportedMethods(supportedMethods);
  methodData.setData(
      stringDataToScriptValue(scriptState, webMethodData.stringifiedData));
  return methodData;
}

}  // namespace

PaymentAppRequest PaymentAppRequestConversion::toPaymentAppRequest(
    ScriptState* scriptState,
    const WebPaymentAppRequest& webAppRequest) {
  PaymentAppRequest appRequest;

  appRequest.setOrigin(webAppRequest.origin);
  HeapVector<PaymentMethodData> methodData;
  for (const auto& md : webAppRequest.methodData) {
    methodData.push_back(toPaymentMethodData(scriptState, md));
  }
  appRequest.setMethodData(methodData);
  appRequest.setTotal(toPaymentItem(webAppRequest.total));
  HeapVector<PaymentDetailsModifier> modifiers;
  for (const auto& modifier : webAppRequest.modifiers) {
    modifiers.push_back(toPaymentDetailsModifier(scriptState, modifier));
  }
  appRequest.setOptionId(webAppRequest.optionId);
  return appRequest;
}

}  // namespace blink
