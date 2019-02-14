// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::canMakePayment().

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"

namespace blink {
namespace {

using payments::mojom::blink::CanMakePaymentQueryResult;
using payments::mojom::blink::HasEnrolledInstrumentQueryResult;
using payments::mojom::blink::PaymentErrorReason;
using payments::mojom::blink::PaymentRequestClient;

class HasEnrolledInstrumentTest : public testing::Test {
  void SetUp() override {
    testing::Test::SetUp();
    RuntimeEnabledFeatures::SetPaymentRequestHasEnrolledInstrumentEnabled(true);
  }
};

TEST_F(HasEnrolledInstrumentTest, RejectPromiseOnUserCancel) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->hasEnrolledInstrument(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::USER_CANCEL);
}

TEST_F(HasEnrolledInstrumentTest, RejectPromiseOnUnknownError) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->hasEnrolledInstrument(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::UNKNOWN);
}

TEST_F(HasEnrolledInstrumentTest, RejectDuplicateRequest) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  request->hasEnrolledInstrument(scope.GetScriptState());
  request->hasEnrolledInstrument(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
}

TEST_F(HasEnrolledInstrumentTest, RejectQueryQuotaExceeded) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->hasEnrolledInstrument(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnHasEnrolledInstrument(
      HasEnrolledInstrumentQueryResult::QUERY_QUOTA_EXCEEDED);
}

TEST_F(HasEnrolledInstrumentTest, ReturnHasNoEnrolledInstrument) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  String captor;
  request->hasEnrolledInstrument(scope.GetScriptState())
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnHasEnrolledInstrument(
      HasEnrolledInstrumentQueryResult::HAS_NO_ENROLLED_INSTRUMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ("false", captor);
}

TEST_F(HasEnrolledInstrumentTest, ReturnHasEnrolledInstrument) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  String captor;
  request->hasEnrolledInstrument(scope.GetScriptState())
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnHasEnrolledInstrument(
      HasEnrolledInstrumentQueryResult::HAS_ENROLLED_INSTRUMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ("true", captor);
}

// CanMakePaymentTest is parameterized on this enum to test both legacy and new
// behaviors.
enum class HasEnrolledInstrumentEnabled { kYes, kNo };

class CanMakePaymentTest
    : public testing::Test,
      public testing::WithParamInterface<HasEnrolledInstrumentEnabled> {
  void SetUp() override {
    testing::Test::SetUp();
    RuntimeEnabledFeatures::SetPaymentRequestHasEnrolledInstrumentEnabled(
        GetParam() == HasEnrolledInstrumentEnabled::kYes);
  }
};

TEST_P(CanMakePaymentTest, RejectPromiseOnUserCancel) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->canMakePayment(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::USER_CANCEL);
}

TEST_P(CanMakePaymentTest, RejectPromiseOnUnknownError) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->canMakePayment(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::UNKNOWN);
}

TEST_P(CanMakePaymentTest, RejectDuplicateRequest) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  request->canMakePayment(scope.GetScriptState());

  request->canMakePayment(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
}

TEST_P(CanMakePaymentTest, RejectQueryQuotaExceeded) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->canMakePayment(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::QUERY_QUOTA_EXCEEDED);
}

TEST_P(CanMakePaymentTest, ReturnCannotMakePayment) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  String captor;
  request->canMakePayment(scope.GetScriptState())
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ("false", captor);
}

TEST_P(CanMakePaymentTest, ReturnCanMakePayment) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  String captor;
  request->canMakePayment(scope.GetScriptState())
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::CAN_MAKE_PAYMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ("true", captor);
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticCanMakePaymentTest,
                         CanMakePaymentTest,
                         ::testing::Values(HasEnrolledInstrumentEnabled::kYes,
                                           HasEnrolledInstrumentEnabled::kNo));

}  // namespace
}  // namespace blink
