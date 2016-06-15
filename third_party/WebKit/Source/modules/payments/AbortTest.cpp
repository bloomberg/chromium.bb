// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptState.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentRequestTestBase.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// Tests for PaymentRequest::abort().
class AbortTest : public PaymentRequestTestBase {
};

// If request.abort() is called without calling request.show() first, then
// abort() should reject with exception.
TEST_F(AbortTest, CannotAbortBeforeShow)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());

    request->abort(getScriptState()).then(expectNoCall(), expectCall());
}

// If request.abort() is called again before the previous abort() resolved, then
// the second abort() should reject with exception.
TEST_F(AbortTest, CannotAbortTwiceConcurrently)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    request->show(getScriptState());
    request->abort(getScriptState());

    request->abort(getScriptState()).then(expectNoCall(), expectCall());
}

// If request.abort() is called after calling request.show(), then abort()
// should not reject with exception.
TEST_F(AbortTest, CanAbortAfterShow)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    request->show(getScriptState());

    request->abort(getScriptState()).then(expectNoCall(), expectNoCall());
}

// If the browser is unable to abort the payment, then the request.abort()
// promise should be rejected.
TEST_F(AbortTest, FailedAbortShouldRejectAbortPromise)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    request->show(getScriptState());

    request->abort(getScriptState()).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnAbort(false);
}

// After the browser is unable to abort the payment once, the second abort()
// call should not be rejected, as it's not a duplicate request anymore.
TEST_F(AbortTest, CanAbortAgainAfterFirstAbortRejected)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    request->show(getScriptState());
    request->abort(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnAbort(false);

    request->abort(getScriptState()).then(expectNoCall(), expectNoCall());
}

// If the browser successfully aborts the payment, then the request.show()
// promise should be rejected, and request.abort() promise should be resolved.
TEST_F(AbortTest, SuccessfulAbortShouldRejectShowPromiseAndResolveAbortPromise)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());

    request->show(getScriptState()).then(expectNoCall(), expectCall());
    request->abort(getScriptState()).then(expectCall(), expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnAbort(true);
}

} // namespace
} // namespace blink
