// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/payments/CurrencyAmount.h"
#include "modules/payments/PaymentDetailsTestHelper.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/ShippingOption.h"
#include "platform/heap/HeapAllocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"

namespace blink {

namespace {

class PaymentRequestTest : public testing::Test {
public:
    PaymentRequestTest()
        : m_page(DummyPageHolder::create())
    {
        setSecurityOrigin("https://www.example.com/");
    }

    ~PaymentRequestTest() override {}

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExceptionState& getExceptionState() { return m_exceptionState; }

    void setSecurityOrigin(const String& securityOrigin)
    {
        m_page->document().setSecurityOrigin(SecurityOrigin::create(KURL(KURL(), securityOrigin)));
    }

private:
    OwnPtr<DummyPageHolder> m_page;
    TrackExceptionState m_exceptionState;
};

TEST_F(PaymentRequestTest, NoExceptionWithValidData)
{
    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_FALSE(getExceptionState().hadException());
}

TEST_F(PaymentRequestTest, SecureContextRequired)
{
    setSecurityOrigin("http://www.example.com");

    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(SecurityError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, SupportedMethodListRequired)
{
    PaymentRequest::create(getScriptState(), Vector<String>(), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, ItemListRequired)
{
    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), PaymentDetails(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, AtLeastOnePaymentDetailsItemRequired)
{
    PaymentDetails details;
    details.setShippingOptions(HeapVector<ShippingOption>(2, buildShippingOptionForTest()));

    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, NullShippingOptionWhenNoOptionsAvailable)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, NullShippingOptionWhenMultipleOptionsAvailable)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    details.setShippingOptions(HeapVector<ShippingOption>(2, buildShippingOptionForTest()));

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, SelectSingleAvailableShippingOption)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest()));

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_EQ("standard", request->shippingOption());
}

TEST_F(PaymentRequestTest, AbortWithoutShowShouldThrow)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->abort(getExceptionState());
    EXPECT_TRUE(getExceptionState().hadException());
}

} // namespace
} // namespace blink
