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

static WebPaymentItem createWebPaymentItemForTest() {
  WebPaymentItem webItem;
  webItem.label = WebString::fromUTF8("Label");
  webItem.amount.currency = WebString::fromUTF8("USD");
  webItem.amount.value = WebString::fromUTF8("9.99");
  return webItem;
}

static WebPaymentMethodData createWebPaymentMethodDataForTest() {
  WebPaymentMethodData webMethodData;
  WebString method = WebString::fromUTF8("foo");
  webMethodData.supportedMethods = WebVector<WebString>(&method, 1);
  webMethodData.stringifiedData = "{\"merchantId\":\"12345\"}";
  return webMethodData;
}

static WebPaymentAppRequest createWebPaymentAppRequestForTest() {
  WebPaymentAppRequest webData;
  webData.origin = WebString::fromUTF8("https://example.com");
  Vector<WebPaymentMethodData> methodData;
  methodData.push_back(createWebPaymentMethodDataForTest());
  webData.methodData = WebVector<WebPaymentMethodData>(methodData);
  webData.total = createWebPaymentItemForTest();
  webData.optionId = WebString::fromUTF8("payment-app-id");
  return webData;
}

TEST(PaymentAppRequestConversionTest, ToPaymentAppRequest) {
  V8TestingScope scope;
  WebPaymentAppRequest webData = createWebPaymentAppRequestForTest();
  PaymentAppRequest data = PaymentAppRequestConversion::toPaymentAppRequest(
      scope.getScriptState(), webData);

  ASSERT_TRUE(data.hasMethodData());
  ASSERT_EQ(1UL, data.methodData().size());
  ASSERT_TRUE(data.methodData().front().hasSupportedMethods());
  ASSERT_EQ(1UL, data.methodData().front().supportedMethods().size());
  ASSERT_EQ("foo", data.methodData().front().supportedMethods().front());
  ASSERT_TRUE(data.methodData().front().hasData());
  ASSERT_TRUE(data.methodData().front().data().isObject());
  String stringifiedData = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(
          scope.context(),
          data.methodData().front().data().v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);
  EXPECT_EQ("{\"merchantId\":\"12345\"}", stringifiedData);

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
