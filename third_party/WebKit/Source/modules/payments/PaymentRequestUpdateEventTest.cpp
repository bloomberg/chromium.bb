// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestUpdateEvent.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/event_type_names.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentTestHelper.h"
#include "modules/payments/PaymentUpdater.h"
#include "platform/bindings/ScriptState.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class MockPaymentUpdater : public GarbageCollectedFinalized<MockPaymentUpdater>,
                           public PaymentUpdater {
  USING_GARBAGE_COLLECTED_MIXIN(MockPaymentUpdater);
  WTF_MAKE_NONCOPYABLE(MockPaymentUpdater);

 public:
  MockPaymentUpdater() {}
  ~MockPaymentUpdater() override {}

  MOCK_METHOD1(OnUpdatePaymentDetails,
               void(const ScriptValue& detailsScriptValue));
  MOCK_METHOD1(OnUpdatePaymentDetailsFailure, void(const String& error));

  void Trace(blink::Visitor* visitor) {}
};

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsCalled) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  MockPaymentUpdater* updater = new MockPaymentUpdater;
  event->SetTrusted(true);
  event->SetPaymentDetailsUpdater(updater);
  event->SetEventPhase(Event::kCapturingPhase);
  ScriptPromiseResolver* payment_details =
      ScriptPromiseResolver::Create(scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_CALL(*updater, OnUpdatePaymentDetails(::testing::_));
  EXPECT_CALL(*updater, OnUpdatePaymentDetailsFailure(::testing::_)).Times(0);

  payment_details->Resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsFailureCalled) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  MockPaymentUpdater* updater = new MockPaymentUpdater;
  event->SetTrusted(true);
  event->SetPaymentDetailsUpdater(updater);
  event->SetEventPhase(Event::kCapturingPhase);
  ScriptPromiseResolver* payment_details =
      ScriptPromiseResolver::Create(scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_CALL(*updater, OnUpdatePaymentDetails(::testing::_)).Times(0);
  EXPECT_CALL(*updater, OnUpdatePaymentDetailsFailure(::testing::_));

  payment_details->Reject("oops");
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateWithoutDispatching) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  event->SetPaymentDetailsUpdater(new MockPaymentUpdater);

  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateTwice) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  MockPaymentUpdater* updater = new MockPaymentUpdater;
  event->SetTrusted(true);
  event->SetPaymentDetailsUpdater(updater);
  event->SetEventPhase(Event::kCapturingPhase);
  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, UpdaterNotRequired) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  event->SetTrusted(true);

  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, AddressChangeUpdateWithTimeout) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  event->SetPaymentDetailsUpdater(request);
  event->SetTrusted(true);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  event->OnUpdateEventTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a response to a "
      "'shippingaddresschange' event",
      error_message);

  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, OptionChangeUpdateWithTimeout) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingoptionchange);
  event->SetTrusted(true);
  event->SetPaymentDetailsUpdater(request);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  event->OnUpdateEventTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a response to a "
      "'shippingoptionchange' event",
      error_message);

  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, AddressChangePromiseTimeout) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  event->SetTrusted(true);
  event->SetPaymentDetailsUpdater(request);
  event->SetEventPhase(Event::kCapturingPhase);
  ScriptPromiseResolver* payment_details =
      ScriptPromiseResolver::Create(scope.GetScriptState());
  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  event->OnUpdateEventTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a response to a "
      "'shippingaddresschange' event",
      error_message);

  payment_details->Resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, OptionChangePromiseTimeout) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  MakePaymentRequestOriginSecure(scope.GetDocument());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingoptionchange);
  event->SetTrusted(true);
  event->SetPaymentDetailsUpdater(request);
  event->SetEventPhase(Event::kCapturingPhase);
  ScriptPromiseResolver* payment_details =
      ScriptPromiseResolver::Create(scope.GetScriptState());
  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  event->OnUpdateEventTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a response to a "
      "'shippingoptionchange' event",
      error_message);

  payment_details->Resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, NotAllowUntrustedEvent) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), EventTypeNames::shippingaddresschange);
  event->SetTrusted(false);

  event->updateWith(
      scope.GetScriptState(),
      ScriptPromiseResolver::Create(scope.GetScriptState())->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

}  // namespace
}  // namespace blink
