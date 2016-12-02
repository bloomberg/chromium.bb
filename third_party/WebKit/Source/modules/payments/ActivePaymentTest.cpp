// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::canMakeActivePayment().

#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using payments::mojom::blink::ActivePaymentQueryResult;
using payments::mojom::blink::PaymentErrorReason;
using payments::mojom::blink::PaymentRequestClient;

TEST(ActivePaymentTest, RejectPromiseOnUserCancel) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.document(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  request->canMakeActivePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::USER_CANCEL);
}

TEST(ActivePaymentTest, RejectPromiseOnUnknownError) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.document(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  request->canMakeActivePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::UNKNOWN);
}

TEST(ActivePaymentTest, RejectDuplicateRequest) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.document(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  request->canMakeActivePayment(scope.getScriptState());

  request->canMakeActivePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(ActivePaymentTest, RejectQueryQuotaExceeded) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.document(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  request->canMakeActivePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakeActivePayment(
      ActivePaymentQueryResult::QUERY_QUOTA_EXCEEDED);
}

TEST(ActivePaymentTest, ReturnCannotMakeActivePayment) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.document(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  String captor;
  request->canMakeActivePayment(scope.getScriptState())
      .then(funcs.expectCall(&captor), funcs.expectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakeActivePayment(
      ActivePaymentQueryResult::CANNOT_MAKE_ACTIVE_PAYMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("false", captor);
}

TEST(ActivePaymentTest, ReturnCanMakeActivePayment) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.document(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  String captor;
  request->canMakeActivePayment(scope.getScriptState())
      .then(funcs.expectCall(&captor), funcs.expectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakeActivePayment(
      ActivePaymentQueryResult::CAN_MAKE_ACTIVE_PAYMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("true", captor);
}

}  // namespace
}  // namespace blink
