// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

static payments::mojom::blink::PaymentCurrencyAmountPtr
CreatePaymentCurrencyAmountForTest() {
  auto currency_amount = payments::mojom::blink::PaymentCurrencyAmount::New();
  currency_amount->currency = String::FromUTF8("USD");
  currency_amount->value = String::FromUTF8("9.99");
  return currency_amount;
}

static payments::mojom::blink::PaymentMethodDataPtr
CreatePaymentMethodDataForTest() {
  auto method_data = payments::mojom::blink::PaymentMethodData::New();
  method_data->supported_method = String::FromUTF8("foo");
  method_data->stringified_data =
      String::FromUTF8("{\"merchantId\":\"12345\"}");
  return method_data;
}

static payments::mojom::blink::CanMakePaymentEventDataPtr
CreateCanMakePaymentEventDataForTest() {
  auto event_data = payments::mojom::blink::CanMakePaymentEventData::New();
  event_data->top_origin = KURL("https://example.com");
  event_data->payment_request_origin = KURL("https://example.com");
  Vector<payments::mojom::blink::PaymentMethodDataPtr> method_data;
  method_data.push_back(CreatePaymentMethodDataForTest());
  event_data->method_data = std::move(method_data);
  return event_data;
}

static payments::mojom::blink::PaymentRequestEventDataPtr
CreatePaymentRequestEventDataForTest() {
  auto event_data = payments::mojom::blink::PaymentRequestEventData::New();
  event_data->top_origin = KURL("https://example.com");
  event_data->payment_request_origin = KURL("https://example.com");
  event_data->payment_request_id = String::FromUTF8("payment-request-id");
  Vector<payments::mojom::blink::PaymentMethodDataPtr> method_data;
  method_data.push_back(CreatePaymentMethodDataForTest());
  event_data->method_data = std::move(method_data);
  event_data->total = CreatePaymentCurrencyAmountForTest();
  event_data->instrument_key = String::FromUTF8("payment-instrument-key");
  return event_data;
}

TEST(PaymentEventDataConversionTest, ToCanMakePaymentEventData) {
  V8TestingScope scope;
  payments::mojom::blink::CanMakePaymentEventDataPtr event_data =
      CreateCanMakePaymentEventDataForTest();
  CanMakePaymentEventInit* data =
      PaymentEventDataConversion::ToCanMakePaymentEventInit(
          scope.GetScriptState(), std::move(event_data));

  ASSERT_TRUE(data->hasTopOrigin());
  EXPECT_EQ(KURL("https://example.com"), KURL(data->topOrigin()));

  ASSERT_TRUE(data->hasPaymentRequestOrigin());
  EXPECT_EQ(KURL("https://example.com"), KURL(data->paymentRequestOrigin()));

  ASSERT_TRUE(data->hasMethodData());
  ASSERT_EQ(1UL, data->methodData().size());
  ASSERT_TRUE(data->methodData().front()->hasSupportedMethod());
  ASSERT_EQ("foo", data->methodData().front()->supportedMethod());
  ASSERT_TRUE(data->methodData().front()->hasData());
  ASSERT_TRUE(data->methodData().front()->data().IsObject());
  String stringified_data = ToBlinkString<String>(
      v8::JSON::Stringify(
          scope.GetContext(),
          data->methodData().front()->data().V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);
  EXPECT_EQ("{\"merchantId\":\"12345\"}", stringified_data);
}

TEST(PaymentEventDataConversionTest, ToPaymentRequestEventData) {
  V8TestingScope scope;
  payments::mojom::blink::PaymentRequestEventDataPtr event_data =
      CreatePaymentRequestEventDataForTest();
  PaymentRequestEventInit* data =
      PaymentEventDataConversion::ToPaymentRequestEventInit(
          scope.GetScriptState(), std::move(event_data));

  ASSERT_TRUE(data->hasTopOrigin());
  EXPECT_EQ(KURL("https://example.com"), KURL(data->topOrigin()));

  ASSERT_TRUE(data->hasPaymentRequestOrigin());
  EXPECT_EQ(KURL("https://example.com"), KURL(data->paymentRequestOrigin()));

  ASSERT_TRUE(data->hasPaymentRequestId());
  EXPECT_EQ("payment-request-id", data->paymentRequestId());

  ASSERT_TRUE(data->hasMethodData());
  ASSERT_EQ(1UL, data->methodData().size());
  ASSERT_TRUE(data->methodData().front()->hasSupportedMethod());
  ASSERT_EQ("foo", data->methodData().front()->supportedMethod());
  ASSERT_TRUE(data->methodData().front()->hasData());
  ASSERT_TRUE(data->methodData().front()->data().IsObject());
  String stringified_data = ToBlinkString<String>(
      v8::JSON::Stringify(
          scope.GetContext(),
          data->methodData().front()->data().V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);
  EXPECT_EQ("{\"merchantId\":\"12345\"}", stringified_data);

  ASSERT_TRUE(data->hasTotal());
  ASSERT_TRUE(data->total()->hasCurrency());
  EXPECT_EQ("USD", data->total()->currency());
  ASSERT_TRUE(data->total()->hasValue());
  EXPECT_EQ("9.99", data->total()->value());

  ASSERT_TRUE(data->hasInstrumentKey());
  EXPECT_EQ("payment-instrument-key", data->instrumentKey());
}

}  // namespace
}  // namespace blink
