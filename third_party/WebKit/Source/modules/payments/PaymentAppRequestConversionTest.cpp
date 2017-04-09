// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppRequestConversion.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "public/platform/modules/payments/WebPaymentAppRequest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

static WebPaymentItem CreateWebPaymentItemForTest() {
  WebPaymentItem web_item;
  web_item.label = WebString::FromUTF8("Label");
  web_item.amount.currency = WebString::FromUTF8("USD");
  web_item.amount.value = WebString::FromUTF8("9.99");
  return web_item;
}

static WebPaymentMethodData CreateWebPaymentMethodDataForTest() {
  WebPaymentMethodData web_method_data;
  WebString method = WebString::FromUTF8("foo");
  web_method_data.supported_methods = WebVector<WebString>(&method, 1);
  web_method_data.stringified_data = "{\"merchantId\":\"12345\"}";
  return web_method_data;
}

static WebPaymentAppRequest CreateWebPaymentAppRequestForTest() {
  WebPaymentAppRequest web_data;
  web_data.origin = WebString::FromUTF8("https://example.com");
  Vector<WebPaymentMethodData> method_data;
  method_data.push_back(CreateWebPaymentMethodDataForTest());
  web_data.method_data = WebVector<WebPaymentMethodData>(method_data);
  web_data.total = CreateWebPaymentItemForTest();
  web_data.option_id = WebString::FromUTF8("payment-app-id");
  return web_data;
}

TEST(PaymentAppRequestConversionTest, ToPaymentAppRequest) {
  V8TestingScope scope;
  WebPaymentAppRequest web_data = CreateWebPaymentAppRequestForTest();
  PaymentAppRequest data = PaymentAppRequestConversion::ToPaymentAppRequest(
      scope.GetScriptState(), web_data);

  ASSERT_TRUE(data.hasMethodData());
  ASSERT_EQ(1UL, data.methodData().size());
  ASSERT_TRUE(data.methodData().front().hasSupportedMethods());
  ASSERT_EQ(1UL, data.methodData().front().supportedMethods().size());
  ASSERT_EQ("foo", data.methodData().front().supportedMethods().front());
  ASSERT_TRUE(data.methodData().front().hasData());
  ASSERT_TRUE(data.methodData().front().data().IsObject());
  String stringified_data = V8StringToWebCoreString<String>(
      v8::JSON::Stringify(
          scope.GetContext(),
          data.methodData().front().data().V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);
  EXPECT_EQ("{\"merchantId\":\"12345\"}", stringified_data);

  ASSERT_TRUE(data.hasTotal());
  ASSERT_TRUE(data.total().hasLabel());
  EXPECT_EQ("Label", data.total().label());
  ASSERT_TRUE(data.total().hasAmount());
  ASSERT_TRUE(data.total().amount().hasCurrency());
  EXPECT_EQ("USD", data.total().amount().currency());
  ASSERT_TRUE(data.total().amount().hasValue());
  EXPECT_EQ("9.99", data.total().amount().value());

  ASSERT_TRUE(data.hasOptionId());
  EXPECT_EQ("payment-app-id", data.optionId());

  ASSERT_TRUE(data.hasOrigin());
  EXPECT_EQ("https://example.com", data.origin());
}

}  // namespace
}  // namespace blink
